#include "PipelineRunner.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

PipelineRunner::PipelineRunner(QObject *parent)
    : QObject(parent), stage_(Idle), stopped_(false), blocked_(false),
      hostSucceeded_(false), cleanupSucceeded_(false), uncertain_(false), signalSent_(false),
      hostReady_(false), lockFd_(-1)
{
    timer_.setSingleShot(true);
    connect(&process_, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
    connect(&process_, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
    connect(&process_, SIGNAL(started()), this, SLOT(processStarted()));
    connect(&process_, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(processFinished(int,QProcess::ExitStatus)));
#if QT_VERSION >= 0x050600
    connect(&process_, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(processError(QProcess::ProcessError)));
#else
    connect(&process_, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(processError(QProcess::ProcessError)));
#endif
    connect(&timer_, SIGNAL(timeout()), this, SLOT(stageTimeout()));
}

bool PipelineRunner::start(const QString &directory, const RunRequest &request)
{
    if (isBusy() || blocked_) {
        emit log(QString::fromUtf8("已有任务或 DSP 状态待检查，不能重复启动。"));
        return false;
    }
    const QString error = request.validate();
    if (!error.isEmpty()) {
        emit log(error);
        return false;
    }
    if (directory.trimmed().isEmpty()) {
        emit log(QString::fromUtf8("请选择业务目录。"));
        return false;
    }
    directory_ = QDir(directory).absolutePath();
    QStringList files;
    files << "app_host";
    if (request.mode != "record")
        files << "slaveloader" << "server_dsp.xe674";
    foreach (const QString &name, files) {
        const QFileInfo info(QDir(directory_).filePath(name));
        if (!info.isFile() || !info.isReadable() ||
                (name != "server_dsp.xe674" && !info.isExecutable())) {
            emit log(QString::fromUtf8("文件缺失或权限不正确：") + info.absoluteFilePath());
            return false;
        }
    }
#ifdef Q_OS_UNIX
    const QByteArray lockPath = QFile::encodeName(QDir(QDir::tempPath()).filePath("ex04-qt-audio.lock"));
    lockFd_ = ::open(lockPath.constData(), O_CREAT | O_RDWR, 0600);
    if (lockFd_ < 0 || ::flock(lockFd_, LOCK_EX | LOCK_NB) < 0) {
        if (lockFd_ >= 0)
            ::close(lockFd_);
        lockFd_ = -1;
        emit log(QString::fromUtf8("无法取得音频任务锁，可能有另一个板端界面正在运行任务。"));
        return false;
    }
    // Child processes must not inherit ownership of the GUI's file lock.
    ::fcntl(lockFd_, F_SETFD, FD_CLOEXEC);
#endif
    request_ = request;
    stopped_ = false;
    uncertain_ = false;
    signalSent_ = false;
    hostReady_ = false;
    readyBuffer_.clear();
    hostSucceeded_ = false;
    cleanupSucceeded_ = request.mode == "record";
    launch(request.mode == "record" ? Running : Loading);
    return true;
}

void PipelineRunner::launch(Stage stage)
{
    stage_ = stage;
    process_.setWorkingDirectory(directory_);
    QString program;
    QStringList args;
    if (stage == Running) {
        program = "app_host";
        args = request_.arguments();
        emit stateChanged(QString::fromUtf8("运行中"));
    } else {
        program = "slaveloader";
        args << (stage == Loading ? "startup" : "shutdown") << "DSP";
        if (stage == Loading)
            args << "server_dsp.xe674";
        emit stateChanged(stage == Loading ? QString::fromUtf8("启动 DSP")
                                          : QString::fromUtf8("关闭 DSP"));
    }
    emit log(QString("[%1] %2 %3").arg(directory_, program, args.join(" ")));
    // A display string only. QProcess receives an argument list, never a shell command.
    process_.start(QDir(directory_).filePath(program), args);
    if (stage != Running)
        timer_.start(30000);
}

void PipelineRunner::readOutput()
{
    const QByteArray out = process_.readAllStandardOutput();
    const QByteArray err = process_.readAllStandardError();
    if (!out.isEmpty())
        emit log(QString::fromLocal8Bit(out.constData(), out.size()));
    if (!err.isEmpty())
        emit log(QString::fromLocal8Bit(err.constData(), err.size()));
    if (stage_ == Running && !hostReady_) {
        readyBuffer_ += out;
        if (readyBuffer_.contains("main: control ready\n")) {
            hostReady_ = true;
            if (stopped_ && process_.state() == QProcess::Running)
                signalHost();
        }
        readyBuffer_ = readyBuffer_.right(128);
    }
}

void PipelineRunner::processStarted()
{
    if (stage_ == Running && stopped_ && hostReady_)
        signalHost();
}

void PipelineRunner::processFinished(int code, QProcess::ExitStatus status)
{
    readOutput();
    timer_.stop();
    emit log(QString::fromUtf8("阶段退出：code=%1，normal=%2").arg(code).arg(status == QProcess::NormalExit));
    advance(code, status == QProcess::NormalExit);
}

void PipelineRunner::processError(QProcess::ProcessError error)
{
    emit log(QString::fromUtf8("进程错误：") + process_.errorString());
    if (error == QProcess::FailedToStart) {
        timer_.stop();
        // QProcess is still tearing down its failed launch here. Reusing it
        // synchronously can close the new loader's descriptors in Qt5.
        QTimer::singleShot(0, this, SLOT(processFailedToStart()));
    }
    // Crashed is followed by finished(); only that signal advances the lifecycle.
}

void PipelineRunner::processFailedToStart()
{
    advance(-1, false);
}

void PipelineRunner::advance(int code, bool normal)
{
    const bool ok = normal && code == 0;
    if (stage_ == Loading) {
        if (!ok) {
            // A failed loader may already have changed hardware state.
            blocked_ = true;
            finish();
        } else if (stopped_)
            launch(Unloading);
        else
            launch(Running);
    } else if (stage_ == Running) {
        hostSucceeded_ = ok;
        if (!normal || !ok)
            uncertain_ = true;
        if (request_.mode == "record")
            finish();
        else
            launch(Unloading);
    } else if (stage_ == Unloading) {
        cleanupSucceeded_ = ok;
        blocked_ = !ok || uncertain_;
        finish();
    }
}

void PipelineRunner::finish()
{
    stage_ = Idle;
#ifdef Q_OS_UNIX
    if (lockFd_ >= 0) {
        ::close(lockFd_);
        lockFd_ = -1;
    }
#endif
    const bool ok = hostSucceeded_ && cleanupSucceeded_ && !blocked_ && !stopped_;
    QString summary;
    if (blocked_)
        summary = QString::fromUtf8("任务异常，需检查日志及 DSP 状态后重启界面。");
    else if (stopped_ && !uncertain_ && cleanupSucceeded_)
        summary = QString::fromUtf8("已停止并完成清理。");
    else if (ok)
        summary = QString::fromUtf8("任务成功，清理完成。");
    else
        summary = QString::fromUtf8("任务失败，请查看日志。");
    emit stateChanged(summary);
    emit finished(ok, summary);
}

void PipelineRunner::stop()
{
    if (!isBusy() || stopped_)
        return;
    stopped_ = true;
    emit stateChanged(QString::fromUtf8("正在停止，等待进程清理"));
    if (stage_ == Running) {
        if (process_.state() == QProcess::Running && hostReady_)
            signalHost();
        else
            emit log(QString::fromUtf8("等待 host 就绪通知；请确保已部署本次更新的 app_host。"));
        timer_.start(30000);
    }
    // Let an in-flight loader finish. A successful startup is followed by shutdown.
}

void PipelineRunner::signalHost()
{
    if (signalSent_)
        return;
    signalSent_ = true;
#ifdef Q_OS_UNIX
#if QT_VERSION >= 0x050300
    const pid_t pid = static_cast<pid_t>(process_.processId());
#else
    const pid_t pid = static_cast<pid_t>(process_.pid());
#endif
    if (pid > 0 && ::kill(pid, SIGINT) == 0)
        emit log(QString::fromUtf8("已向当前 host 发送 SIGINT，等待安全退出。"));
    else
        emit log(QString::fromUtf8("发送 SIGINT 失败：") + QString::fromLocal8Bit(strerror(errno)));
#else
    emit log(QString::fromUtf8("停止操作仅支持 Linux/Unix。"));
#endif
}

void PipelineRunner::stageTimeout()
{
    // Do not kill a loader or host while it owns DSP/IPC resources.
    emit stateChanged(QString::fromUtf8("等待超时，任务仍未退出，请保留日志检查板端"));
    emit log(QString::fromUtf8("已等待 30 秒；界面继续响应，不自动强杀或重载 DSP。"));
}
