#include "WavReader.h"
#include <QMutexLocker>

static quint16 u16(const char *p)
{
    return quint16(uchar(p[0])) | (quint16(uchar(p[1])) << 8);
}
static quint32 u32(const char *p)
{
    return quint32(u16(p)) | (quint32(u16(p + 2)) << 16);
}

WavReader::WavReader(int stream, QObject *parent)
    : QThread(parent), stream_(stream), serial_(0), pending_(false),
      cursorPending_(false), stopping_(false)
{
    qRegisterMetaType<WaveformSlice>("WaveformSlice");
    start();
}
WavReader::~WavReader() { shutdown(); }
void WavReader::shutdown()
{
    { QMutexLocker lock(&mutex_); stopping_ = true; wake_.wakeOne(); }
    wait();
}
void WavReader::cancel()
{
    QMutexLocker lock(&mutex_);
    ++serial_;
    pending_ = cursorPending_ = false;
}
void WavReader::request(const QString &path, quint64 generation, qint64 firstFrame,
                        qint64 frameCount, int buckets)
{
    QMutexLocker lock(&mutex_);
    job_.path = path;
    job_.generation = generation;
    job_.firstFrame = qMax(qint64(0), firstFrame);
    job_.frameCount = frameCount;
    job_.buckets = qBound(1, buckets, 4096);
    ++serial_;
    pending_ = true;
    cursorPending_ = false;
    wake_.wakeOne();
}
void WavReader::requestCursor(const QString &path, quint64 generation, qint64 frame)
{
    QMutexLocker lock(&mutex_);
    cursor_.path = path;
    cursor_.generation = generation;
    cursor_.firstFrame = frame;
    cursorPending_ = true;
    wake_.wakeOne();
}
bool WavReader::obsolete(quint64 serial)
{
    QMutexLocker lock(&mutex_);
    return stopping_ || serial != serial_;
}

bool WavReader::readInfo(QFile &file, WavInfo *info, QString *error)
{
    *error = QString::fromUtf8("WAV 文件损坏或数据不完整");
    if (!file.isOpen() && !file.open(QIODevice::ReadOnly)) {
        *error = file.errorString(); return false;
    }
    if (!file.seek(0)) return false;
    QByteArray header = file.read(12);
    if (header.size() != 12 || header.left(4) != "RIFF" || header.mid(8, 4) != "WAVE")
        return false;
    const qint64 end = qint64(u32(header.constData() + 4)) + 8;
    if (end > file.size() || end < 12) return false;
    bool haveFormat = false, haveData = false;
    qint64 position = 12;
    while (position + 8 <= end && !(haveFormat && haveData)) {
        if (!file.seek(position)) return false;
        const QByteArray chunk = file.read(8);
        if (chunk.size() != 8) return false;
        const qint64 size = u32(chunk.constData() + 4);
        if (size > end - position - 8) return false;
        if (chunk.left(4) == "fmt ") {
            const QByteArray fmt = file.read(qMin(qint64(16), size));
            if (fmt.size() != 16) return false;
            if (u16(fmt.constData()) != 1 || u16(fmt.constData() + 2) != 2 ||
                    u32(fmt.constData() + 4) != 44100 || u16(fmt.constData() + 14) != 16) {
                *error = QString::fromUtf8("需要 44.1 kHz / 双声道 / PCM16 WAV");
                return false;
            }
            if (u16(fmt.constData() + 12) != 4 || u32(fmt.constData() + 8) != 176400)
                return false;
            haveFormat = true;
        } else if (chunk.left(4) == "data") {
            if (size == 0 || size % 4) return false;
            info->dataOffset = position + 8;
            info->frames = size / 4;
            haveData = true;
        }
        position += 8 + size + (size & 1);
        if (position > end) return false;
    }
    if (!haveFormat || !haveData) return false;
    info->sampleRate = 44100;
    error->clear();
    return true;
}

void WavReader::run()
{
    for (;;) {
        Job job;
        quint64 serial;
        bool cursorOnly;
        {
            QMutexLocker lock(&mutex_);
            while (!stopping_ && !pending_ && !cursorPending_) wake_.wait(&mutex_);
            if (stopping_) return;
            cursorOnly = !pending_;
            job = cursorOnly ? cursor_ : job_;
            if (cursorOnly) cursorPending_ = false; else pending_ = false;
            serial = serial_;
        }
        QFile file(job.path);
        WavInfo info;
        QString error;
        if (!readInfo(file, &info, &error)) {
            if (!obsolete(serial)) emit failed(stream_, job.generation, error);
            continue;
        }
        if (cursorOnly) {
            if (job.firstFrame < 0 || job.firstFrame >= info.frames) continue;
            if (!file.seek(info.dataOffset + job.firstFrame * 4)) continue;
            const QByteArray sample = file.read(4);
            if (sample.size() == 4 && !obsolete(serial))
                emit cursorReady(stream_, job.generation, job.firstFrame,
                    qint16(u16(sample.constData())), qint16(u16(sample.constData() + 2)));
            continue;
        }
        WaveformSlice slice;
        slice.stream = stream_;
        slice.generation = job.generation;
        slice.path = job.path;
        slice.info = info;
        slice.firstFrame = job.firstFrame;
        slice.frameCount = job.frameCount < 0 ? info.frames : qMax(qint64(1), job.frameCount);
        slice.peaks[0].resize(job.buckets);
        slice.peaks[1].resize(job.buckets);
        const qint64 stop = qMin(info.frames, slice.firstFrame + slice.frameCount);
        qint64 frame = qMin(info.frames, slice.firstFrame);
        if (!file.seek(info.dataOffset + frame * 4)) error = file.errorString();
        while (error.isEmpty() && frame < stop && !obsolete(serial)) {
            const int frames = int(qMin(qint64(16384), stop - frame));
            const QByteArray bytes = file.read(frames * 4);
            if (bytes.size() != frames * 4) {
                error = QString::fromUtf8("读取 WAV 时数据被截断"); break;
            }
            for (int i = 0; i < frames; ++i) {
                const int bin = int((frame + i - slice.firstFrame) * job.buckets / slice.frameCount);
                slice.peaks[0][bin].add(qint16(u16(bytes.constData() + i * 4)));
                slice.peaks[1][bin].add(qint16(u16(bytes.constData() + i * 4 + 2)));
            }
            frame += frames;
        }
        if (obsolete(serial)) continue;
        if (!error.isEmpty()) emit failed(stream_, job.generation, error);
        else emit ready(slice);
    }
}
