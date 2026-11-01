#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#if QT_VERSION >= 0x040700
#include <QElapsedTimer>
#else
#include <QTime>
#endif
#include <QTimer>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include "RunRequest.h"
#include "PipelineRunner.h"
#include "IpcSession.h"

class FrontendTest : public QObject
{
    Q_OBJECT
private:
    QString directory_;

    void writeFile(const QString &name, const QByteArray &bytes, bool executable = true)
    {
        QFile file(directory_ + "/" + name);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
        file.close();
        if (executable)
            QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

    QByteArray trace()
    {
        QFile file(directory_ + "/trace");
        if (!file.open(QIODevice::ReadOnly))
            return QByteArray();
        return file.readAll();
    }

    bool waitFor(QSignalSpy &spy, int timeout = 6000)
    {
#if QT_VERSION >= 0x040700
        QElapsedTimer timer;
#else
        QTime timer;
#endif
        timer.start();
        while (spy.isEmpty() && timer.elapsed() < timeout)
            QTest::qWait(20);
        return !spy.isEmpty();
    }

    void verifyIpcRemoved(const QSignalSpy &logs)
    {
        bool found = false;
        for (int i = 0; i < logs.size(); ++i) {
            const QString line = logs.at(i).at(0).toString();
            if (!line.startsWith("IPC session:"))
                continue;
            found = true;
            const int semId = line.section("semid=", 1).section(' ', 0, 0).toInt();
            const int shmId = line.section("shmid=", 1).toInt();
            QCOMPARE(semctl(semId, 0, GETVAL), -1);
            if (shmId >= 0) {
                struct shmid_ds info;
                QCOMPARE(shmctl(shmId, IPC_STAT, &info), -1);
            }
        }
        QVERIFY(found);
    }

private slots:
    void init()
    {
        char path[] = "/tmp/ex04-qt-test-XXXXXX";
        char *created = mkdtemp(path);
        QVERIFY(created != 0);
        directory_ = QString::fromLocal8Bit(created);
        writeFile("slaveloader", "#!/bin/sh\necho \"$1\" >> trace\nexit 0\n");
        writeFile("app_host", "#!/bin/sh\necho 'main: control ready'\nprintf '<%s>\\n' \"$@\" >> trace\n");
        writeFile("server_dsp.xe674", "fake image", false);
    }

    void cleanup()
    {
        QDir dir(directory_);
        foreach (const QString &name, dir.entryList(QDir::Files | QDir::Hidden))
            QVERIFY(dir.remove(name));
        QVERIFY(QDir().rmdir(directory_));
    }

    void modeRestrictions()
    {
        RunRequest request;
        request.mode = "audio";
        request.algorithm = "speed2";
        QVERIFY(!request.validate().isEmpty());
        request.algorithm = "spatial";
        QVERIFY(request.validate().isEmpty());
        request.mode = "smoke";
        QVERIFY(!request.validate().isEmpty());
        request.mode = "record";
        QVERIFY(!request.validate().isEmpty());
        request.outputWav = "record.wav";
        QVERIFY(request.validate().isEmpty());
        request.seconds = 2148;
        QVERIFY(!request.validate().isEmpty());
    }

    void lifecycleAndArguments()
    {
        RunRequest request;
        request.mode = "file";
        request.inputWav = "input name;$(touch injected).wav";
        request.outputWav = "output name.wav";
        request.algorithm = "spatial";
        request.direction = SoundDirection(60, 250);
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, request));
        QVERIFY(!runner.start(directory_, request));
        QVERIFY(waitFor(done));
        QVERIFY(done.at(0).at(0).toBool());
        QCOMPARE(trace(), QByteArray("startup\n<DSP>\n<file>\n<--input>\n<input name;$(touch injected).wav>\n<--output>\n<output name.wav>\n<--algo>\n<spatial>\n<--elevation>\n<60>\n<--azimuth>\n<250>\nshutdown\n"));
        QVERIFY(!QFile::exists(directory_ + "/injected"));
    }

    void recordSkipsLoader()
    {
        RunRequest request;
        request.mode = "record";
        request.outputWav = "record.wav";
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, request));
        QVERIFY(waitFor(done));
        QVERIFY(done.at(0).at(0).toBool());
        QVERIFY(!trace().contains("startup"));
        QVERIFY(!trace().contains("shutdown"));
    }

    void loaderFailure()
    {
        writeFile("slaveloader", "#!/bin/sh\necho \"$1\" >> trace\nexit 7\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        QVERIFY(waitFor(done));
        QVERIFY(!done.at(0).at(0).toBool());
        QVERIFY(runner.isBlocked());
        QCOMPARE(trace(), QByteArray("startup\n"));
    }

    void hostFailureStillUnloads()
    {
        writeFile("app_host", "#!/bin/sh\necho host >> trace\nexit 4\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        QVERIFY(waitFor(done));
        QVERIFY(!done.at(0).at(0).toBool());
        QVERIFY(runner.isBlocked());
        QCOMPARE(trace(), QByteArray("startup\nhost\nshutdown\n"));
    }

    void shutdownFailureIsFailure()
    {
        writeFile("slaveloader", "#!/bin/sh\necho \"$1\" >> trace\n[ \"$1\" = startup ]\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        QVERIFY(waitFor(done));
        QVERIFY(!done.at(0).at(0).toBool());
        QVERIFY(runner.isBlocked());
    }

    void hostCannotExecStillUnloads()
    {
        writeFile("app_host", "#!/nonexistent/ex04-interpreter\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        QVERIFY(waitFor(done));
        QVERIFY(!done.at(0).at(0).toBool());
        QVERIFY(runner.isBlocked());
        QCOMPARE(trace(), QByteArray("startup\nshutdown\n"));
    }

    void stopDuringStartup()
    {
        writeFile("slaveloader", "#!/bin/sh\necho \"$1\" >> trace\nsleep 0.1\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        runner.stop();
        QVERIFY(waitFor(done));
        QVERIFY(!runner.isBlocked());
        QCOMPARE(trace(), QByteArray("startup\nshutdown\n"));
    }

    void cooperativeStop()
    {
        writeFile("app_host", "#!/bin/sh\ntrap 'echo stopped >> trace; exit 0' INT\necho 'main: control ready'\nwhile :; do sleep 0.1; done\n");
        PipelineRunner runner;
        QSignalSpy done(&runner, SIGNAL(finished(bool,QString)));
        QVERIFY(runner.start(directory_, RunRequest()));
        QTimer::singleShot(200, &runner, SLOT(stop()));
        QVERIFY(waitFor(done));
        QVERIFY(!runner.isBlocked());
        QCOMPARE(trace(), QByteArray("startup\nstopped\nshutdown\n"));
    }

    void separateWindowLock()
    {
        writeFile("app_host", "#!/bin/sh\nsleep 0.2\n");
        PipelineRunner first, second;
        QSignalSpy done(&first, SIGNAL(finished(bool,QString)));
        QVERIFY(first.start(directory_, RunRequest()));
        QVERIFY(!second.start(directory_, RunRequest()));
        QVERIFY(waitFor(done));
        QVERIFY(second.start(directory_, RunRequest()));
        QSignalSpy secondDone(&second, SIGNAL(finished(bool,QString)));
        QVERIFY(waitFor(secondDone));
    }

    void ipcRoundTrip_data()
    {
        QTest::addColumn<bool>("dataMode");
        QTest::newRow("semaphores") << false;
        QTest::newRow("shared-memory") << true;
    }

    void ipcRoundTrip()
    {
        QFETCH(bool, dataMode);
        const QString peer = QString::fromLocal8Bit(qgetenv("EX04_IPC_PEER"));
        QVERIFY2(QFile::exists(peer), "Set EX04_IPC_PEER to the built Linux ex04_ipc_peer");
        IpcSession session;
        QSignalSpy messages(&session, SIGNAL(message(uint,QString)));
        QSignalSpy logs(&session, SIGNAL(log(QString)));
        QSignalSpy done(&session, SIGNAL(finished(bool,QString)));
        QVERIFY(session.start(dataMode, peer));
        QVERIFY(waitFor(done));
        QVERIFY(done.at(0).at(0).toBool());
        QCOMPARE(messages.size(), 10);
        verifyIpcRemoved(logs);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(messages.at(i).at(0).toUInt(), uint(i + 1));
            if (dataMode)
                QCOMPARE(messages.at(i).at(1).toString(), QString("ARM C process: message %1").arg(i + 1));
        }
    }

    void ipcStopAndCrash()
    {
        const QString peer = QString::fromLocal8Bit(qgetenv("EX04_IPC_PEER"));
        QVERIFY(QFile::exists(peer));
        IpcSession session;
        QSignalSpy done(&session, SIGNAL(finished(bool,QString)));
        QSignalSpy logs(&session, SIGNAL(log(QString)));
        QVERIFY(session.start(true, peer));
        QTimer::singleShot(100, &session, SLOT(stop()));
        QVERIFY(waitFor(done));
        QVERIFY(!session.isBusy());
        QVERIFY(!done.at(0).at(0).toBool());
        verifyIpcRemoved(logs);
        done.clear();
        logs.clear();
        writeFile("bad-peer", "#!/bin/sh\nexit 3\n");
        QVERIFY(session.start(true, directory_ + "/bad-peer"));
        QVERIFY(waitFor(done));
        QVERIFY(!done.at(0).at(0).toBool());
        QVERIFY(!session.isBusy());
        verifyIpcRemoved(logs);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FrontendTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_frontend.moc"
