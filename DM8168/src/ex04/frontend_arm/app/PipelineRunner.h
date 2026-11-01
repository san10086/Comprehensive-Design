#ifndef EX04_ARM_PIPELINE_RUNNER_H
#define EX04_ARM_PIPELINE_RUNNER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include "RunRequest.h"

class PipelineRunner : public QObject
{
    Q_OBJECT
public:
    explicit PipelineRunner(QObject *parent = 0);
    bool isBusy() const { return stage_ != Idle; }
    bool isBlocked() const { return blocked_; }
    bool start(const QString &directory, const RunRequest &request);

public slots:
    void stop();

signals:
    void log(const QString &text);
    void stateChanged(const QString &state);
    void finished(bool success, const QString &summary);

private slots:
    void readOutput();
    void processStarted();
    void processFinished(int code, QProcess::ExitStatus status);
    void processError(QProcess::ProcessError error);
    void processFailedToStart();
    void stageTimeout();

private:
    enum Stage { Idle, Loading, Running, Unloading };
    void launch(Stage stage);
    void advance(int code, bool normal);
    void finish();
    void signalHost();

    QProcess process_;
    QTimer timer_;
    Stage stage_;
    RunRequest request_;
    QString directory_;
    bool stopped_;
    bool blocked_;
    bool hostSucceeded_;
    bool cleanupSucceeded_;
    bool uncertain_;
    bool signalSent_;
    bool hostReady_;
    QByteArray readyBuffer_;
    int lockFd_;
};

#endif
