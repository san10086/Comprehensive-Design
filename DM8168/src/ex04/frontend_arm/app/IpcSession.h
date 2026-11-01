#ifndef EX04_ARM_IPC_SESSION_H
#define EX04_ARM_IPC_SESSION_H

#include <QObject>
#include <QMutex>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include "ipc_protocol.h"

class IpcReader : public QThread
{
    Q_OBJECT
public:
    IpcReader(int semId, const Ex04IpcMessage *message, QObject *parent);
    void stopReading();
signals:
    void received(unsigned int sequence, const QString &text);
    void failed(const QString &error);
protected:
    void run();
private:
    bool stopping();
    QMutex mutex_;
    bool stop_;
    int semId_;
    const Ex04IpcMessage *message_;
};

class IpcSession : public QObject
{
    Q_OBJECT
public:
    explicit IpcSession(QObject *parent = 0);
    ~IpcSession();
    bool isBusy() const { return busy_; }
    bool start(bool dataMode, const QString &peerPath);
public slots:
    void stop();
signals:
    void log(const QString &text);
    void message(unsigned int sequence, const QString &text);
    void finished(bool success, const QString &summary);
private slots:
    void readOutput();
    void peerFinished(int code, QProcess::ExitStatus status);
    void peerError(QProcess::ProcessError error);
    void readerFinished();
    void readerFailed(const QString &error);
    void gotMessage(unsigned int sequence, const QString &text);
    void peerTimeout();
private:
    void cleanup();
    void complete();
    QProcess peer_;
    QTimer timer_;
    IpcReader *reader_;
    int semId_;
    int shmId_;
    Ex04IpcMessage *shared_;
    bool busy_;
    bool stopping_;
    bool failed_;
    bool peerDone_;
    bool readerDone_;
    unsigned int received_;
};

#endif
