#include "IpcSession.h"
#include <QFileInfo>
#include <QMutexLocker>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

union Ex04Semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

IpcReader::IpcReader(int semId, const Ex04IpcMessage *message, QObject *parent)
    : QThread(parent), stop_(false), semId_(semId), message_(message) {}

void IpcReader::stopReading()
{
    QMutexLocker lock(&mutex_);
    stop_ = true;
}

bool IpcReader::stopping()
{
    QMutexLocker lock(&mutex_);
    return stop_;
}

void IpcReader::run()
{
    unsigned int expected = 1;
    while (!stopping() && expected <= EX04_IPC_MESSAGES) {
        struct sembuf ready = { EX04_IPC_READABLE, -1, IPC_NOWAIT };
        if (semop(semId_, &ready, 1) < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                msleep(20);
                continue;
            }
            emit failed(QString::fromLocal8Bit(strerror(errno)));
            return;
        }
        unsigned int sequence = expected;
        QString text = QString::fromUtf8("信号量握手 %1").arg(expected);
        if (message_) {
            sequence = message_->sequence;
            // Copy before acknowledging. The producer can overwrite after the ack.
            QByteArray bytes(message_->text, EX04_IPC_TEXT_SIZE);
            bytes[EX04_IPC_TEXT_SIZE - 1] = '\0';
            text = QString::fromUtf8(bytes.constData());
        }
        if (sequence != expected) {
            emit failed(QString::fromUtf8("消息序号错误：expected=%1 actual=%2").arg(expected).arg(sequence));
            return;
        }
        emit received(sequence, text);
        struct sembuf ack = { EX04_IPC_WRITABLE, 1, 0 };
        if (semop(semId_, &ack, 1) < 0) {
            emit failed(QString::fromLocal8Bit(strerror(errno)));
            return;
        }
        ++expected;
    }
}

IpcSession::IpcSession(QObject *parent)
    : QObject(parent), reader_(0), semId_(-1), shmId_(-1), shared_(0),
      busy_(false), stopping_(false), failed_(false), peerDone_(true),
      readerDone_(true), received_(0)
{
    timer_.setSingleShot(true);
    connect(&peer_, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
    connect(&peer_, SIGNAL(readyReadStandardError()), this, SLOT(readOutput()));
    connect(&peer_, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(peerFinished(int,QProcess::ExitStatus)));
#if QT_VERSION >= 0x050600
    connect(&peer_, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(peerError(QProcess::ProcessError)));
#else
    connect(&peer_, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(peerError(QProcess::ProcessError)));
#endif
    connect(&timer_, SIGNAL(timeout()), this, SLOT(peerTimeout()));
}

IpcSession::~IpcSession()
{
    // Normal window closure waits for finished(). This also covers app teardown.
    if (reader_) {
        reader_->stopReading();
        reader_->wait();
    }
    if (peer_.state() != QProcess::NotRunning) {
        peer_.kill(); // This demo child owns no DSP/audio resources.
        peer_.waitForFinished(1000);
    }
    cleanup();
}

bool IpcSession::start(bool dataMode, const QString &peerPath)
{
    if (busy_)
        return false;
    if (!QFileInfo(peerPath).isExecutable()) {
        emit log(QString::fromUtf8("找不到可执行 IPC 示例：") + peerPath);
        return false;
    }
    semId_ = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
    if (semId_ < 0) {
        emit log(QString::fromUtf8("semget 失败：") + QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    unsigned short values[2] = { 1, 0 };
    Ex04Semun arg;
    arg.array = values;
    bool resourceOk = semctl(semId_, 0, SETALL, arg) >= 0;
    if (resourceOk && dataMode) {
        shmId_ = shmget(IPC_PRIVATE, sizeof(Ex04IpcMessage), IPC_CREAT | 0600);
        resourceOk = shmId_ >= 0;
        if (resourceOk) {
            void *address = shmat(shmId_, 0, 0);
            resourceOk = address != reinterpret_cast<void *>(-1);
            if (resourceOk) {
                shared_ = static_cast<Ex04IpcMessage *>(address);
                memset(shared_, 0, sizeof(*shared_));
            }
        }
    }
    if (!resourceOk) {
        emit log(QString::fromUtf8("创建 IPC 资源失败：") + QString::fromLocal8Bit(strerror(errno)));
        cleanup();
        return false;
    }
    emit log(QString("IPC session: semid=%1 shmid=%2").arg(semId_).arg(shmId_));
    reader_ = new IpcReader(semId_, shared_, this);
    connect(reader_, SIGNAL(received(uint,QString)), this, SLOT(gotMessage(uint,QString)));
    connect(reader_, SIGNAL(failed(QString)), this, SLOT(readerFailed(QString)));
    connect(reader_, SIGNAL(finished()), this, SLOT(readerFinished()));
    busy_ = true;
    stopping_ = false;
    failed_ = false;
    peerDone_ = false;
    readerDone_ = false;
    received_ = 0;
    QStringList args;
    args << (dataMode ? "data" : "sync") << QString::number(semId_);
    if (dataMode)
        args << QString::number(shmId_);
    reader_->start();
    peer_.start(QFileInfo(peerPath).absoluteFilePath(), args);
    timer_.start(10000);
    return true;
}

void IpcSession::readOutput()
{
    const QByteArray bytes = peer_.readAllStandardOutput() + peer_.readAllStandardError();
    if (!bytes.isEmpty())
        emit log(QString::fromLocal8Bit(bytes.constData(), bytes.size()));
}

void IpcSession::gotMessage(unsigned int sequence, const QString &text)
{
    received_ = sequence;
    emit message(sequence, text);
}

void IpcSession::stop()
{
    if (!busy_ || stopping_)
        return;
    stopping_ = true;
    if (reader_)
        reader_->stopReading();
    if (peer_.state() != QProcess::NotRunning)
        peer_.terminate();
    timer_.start(2000);
}

void IpcSession::peerFinished(int code, QProcess::ExitStatus status)
{
    readOutput();
    peerDone_ = true;
    if (!stopping_ && (code != 0 || status != QProcess::NormalExit)) {
        failed_ = true;
        emit log(QString::fromUtf8("IPC 示例异常退出：%1").arg(code));
    }
    // Worker finishes itself after ten messages; early peer exit must wake it too.
    if (reader_)
        reader_->stopReading();
    complete();
}

void IpcSession::peerError(QProcess::ProcessError error)
{
    emit log(peer_.errorString());
    if (error == QProcess::FailedToStart) {
        failed_ = true;
        peerDone_ = true;
        reader_->stopReading();
        complete();
    }
}

void IpcSession::readerFailed(const QString &error)
{
    failed_ = true;
    emit log(QString::fromUtf8("IPC 接收失败：") + error);
    stop();
}

void IpcSession::readerFinished()
{
    readerDone_ = true;
    complete();
}

void IpcSession::peerTimeout()
{
    if (!busy_)
        return;
    if (!stopping_) {
        failed_ = true;
        emit log(QString::fromUtf8("IPC 示例超时，正在回收本次会话。"));
        stop();
    } else if (peer_.state() != QProcess::NotRunning) {
        peer_.kill();
        timer_.start(2000);
    }
}

void IpcSession::complete()
{
    if (!busy_ || !readerDone_ || !peerDone_)
        return;
    timer_.stop();
    const bool success = !failed_ && !stopping_ && received_ == EX04_IPC_MESSAGES;
    cleanup();
    busy_ = false;
    const QString summary = success ? QString::fromUtf8("IPC 验证成功：10 次交互，资源已清理。") :
        (failed_ || !stopping_ ? QString::fromUtf8("IPC 验证失败，资源已清理。")
                             : QString::fromUtf8("IPC 验证已停止，资源已清理。"));
    emit finished(success, summary);
}

void IpcSession::cleanup()
{
    if (reader_) {
        reader_->wait();
        delete reader_;
        reader_ = 0;
    }
    if (shared_) {
        shmdt(shared_);
        shared_ = 0;
    }
    if (shmId_ >= 0) {
        shmctl(shmId_, IPC_RMID, 0);
        shmId_ = -1;
    }
    if (semId_ >= 0) {
        semctl(semId_, 0, IPC_RMID);
        semId_ = -1;
    }
}
