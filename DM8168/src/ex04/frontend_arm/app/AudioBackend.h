#ifndef EX04_AUDIO_BACKEND_H
#define EX04_AUDIO_BACKEND_H
#include "PipelineRunner.h"
#include <QVector>
#include <QMetaType>
#if QT_VERSION >= 0x040700
#include <QElapsedTimer>
#else
#include <QTime>
#endif

// Transport-neutral interleaved PCM16. Frame indices are per stream and per run.
struct PcmChunk
{
    PcmChunk() : sessionId(0), stream(0), firstFrame(0), sampleRate(44100), channels(2) {}
    quint64 sessionId;
    int stream; // 0: input, 1: output
    qint64 firstFrame;
    int sampleRate;
    int channels;
    QVector<qint16> samples;
};
Q_DECLARE_METATYPE(PcmChunk)

class AudioBackend : public QObject
{
    Q_OBJECT
public:
    explicit AudioBackend(QObject *parent = 0) : QObject(parent) { qRegisterMetaType<PcmChunk>("PcmChunk"); }
    virtual bool start(const QString &directory, const RunRequest &request, quint64 session) = 0;
    virtual bool isBusy() const = 0;
    virtual bool isBlocked() const { return false; }
public slots:
    virtual void stop() = 0;
signals:
    void stateChanged(const QString &state);
    void log(const QString &text);
    void finished(bool success, const QString &summary);
    void waveformReady(const PcmChunk &chunk);
    void positionChanged(double seconds);
};

class DemoBackend : public AudioBackend
{
    Q_OBJECT
public:
    explicit DemoBackend(QObject *parent = 0);
    void setFilePreview(bool loaded, double duration);
    bool start(const QString &directory, const RunRequest &request, quint64 session);
    bool isBusy() const { return timer_.isActive(); }
    static PcmChunk makeChunk(quint64 session, int stream, qint64 firstFrame, int frames);
public slots:
    void stop();
private slots:
    void tick();
private:
    QTimer timer_;
#if QT_VERSION >= 0x040700
    QElapsedTimer elapsed_;
#else
    QTime elapsed_;
#endif
    RunRequest request_;
    quint64 session_;
    qint64 nextFrame_;
    bool fileLoaded_;
    double fileDuration_;
    double duration_;
};

class BoardBackend : public AudioBackend
{
    Q_OBJECT
public:
    explicit BoardBackend(QObject *parent = 0);
    bool start(const QString &directory, const RunRequest &request, quint64 session);
    bool isBusy() const { return runner_.isBusy(); }
    bool isBlocked() const { return runner_.isBlocked(); }
public slots:
    void stop();
private:
    PipelineRunner runner_;
    quint64 sessionId_; // Stamp future PCM transport packets with this run ID.
};
#endif
