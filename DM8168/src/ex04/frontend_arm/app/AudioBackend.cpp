#include "AudioBackend.h"
#include <cmath>

DemoBackend::DemoBackend(QObject *parent)
    : AudioBackend(parent), session_(0), nextFrame_(0), fileLoaded_(false),
      fileDuration_(5.0), duration_(5.0)
{
    connect(&timer_, SIGNAL(timeout()), this, SLOT(tick()));
}

void DemoBackend::setFilePreview(bool loaded, double duration)
{
    fileLoaded_ = loaded;
    fileDuration_ = duration;
}

bool DemoBackend::start(const QString &directory, const RunRequest &request, quint64 session)
{
    Q_UNUSED(directory);
    if (isBusy()) return false;
    const QString error = request.validate(false);
    if (!error.isEmpty()) { emit log(error); return false; }
    request_ = request;
    session_ = session;
    nextFrame_ = 0;
    duration_ = request.mode == "file" ? fileDuration_ : request.seconds;
    if (request.mode == "smoke") duration_ = 1;
    elapsed_.start();
    timer_.start(50);
    emit stateChanged(QString::fromUtf8("演示中"));
    emit log(QString::fromUtf8("演示开始 · 不启动板端进程，不写入音频文件"));
    return true;
}

PcmChunk DemoBackend::makeChunk(quint64 session, int stream, qint64 firstFrame, int frames)
{
    PcmChunk chunk;
    chunk.sessionId = session;
    chunk.stream = stream;
    chunk.firstFrame = firstFrame;
    chunk.samples.resize(frames * 2);
    for (int i = 0; i < frames; ++i) {
        const double t = double(firstFrame + i) / 44100.0;
        for (int channel = 0; channel < 2; ++channel) {
            const double phase = t - (stream ? 0.0017 * (channel + 1) : 0.0);
            const double envelope = 0.18 + 0.55 * std::pow(std::sin(t * 2.1 + channel * 0.7), 2.0);
            const double tone = (std::sin(phase * 6.28318530718 * (180 + channel * 43)) +
                                 0.26 * std::sin(phase * 6.28318530718 * 430)) / 1.26;
            chunk.samples[i * 2 + channel] = qint16(tone * envelope * (stream ? 24000 : 31000));
        }
    }
    return chunk;
}

void DemoBackend::tick()
{
    const double elapsed = elapsed_.elapsed() / 1000.0;
    const double position = duration_ > 0 ? qMin(duration_, elapsed) : elapsed;
    if (request_.mode != "smoke" && !(request_.mode == "file" && fileLoaded_)) {
        const qint64 target = qint64(position * 44100);
        if (target - nextFrame_ > 220500) nextFrame_ = target - 220500;
        while (nextFrame_ < target) {
            const int frames = int(qMin(qint64(2205), target - nextFrame_));
            emit waveformReady(makeChunk(session_, 0, nextFrame_, frames));
            if (request_.mode != "record")
                emit waveformReady(makeChunk(session_, 1, nextFrame_, frames));
            nextFrame_ += frames;
        }
    }
    emit positionChanged(position);
    if (duration_ > 0 && elapsed >= duration_) {
        timer_.stop();
        emit stateChanged(QString::fromUtf8("演示结束"));
        emit finished(true, QString::fromUtf8("演示结束"));
    }
}

void DemoBackend::stop()
{
    if (!isBusy()) return;
    timer_.stop();
    emit stateChanged(QString::fromUtf8("已停止"));
    emit finished(false, QString::fromUtf8("演示已停止"));
}

BoardBackend::BoardBackend(QObject *parent) : AudioBackend(parent), runner_(this), sessionId_(0)
{
    connect(&runner_, SIGNAL(log(QString)), this, SIGNAL(log(QString)));
    connect(&runner_, SIGNAL(stateChanged(QString)), this, SIGNAL(stateChanged(QString)));
    connect(&runner_, SIGNAL(finished(bool,QString)), this, SIGNAL(finished(bool,QString)));
}

bool BoardBackend::start(const QString &directory, const RunRequest &request, quint64 session)
{
    sessionId_ = session;
    // A future PCM adapter emits waveformReady using sessionId_.
    return runner_.start(directory, request);
}

void BoardBackend::stop() { runner_.stop(); }
