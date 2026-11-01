#ifndef EX04_WAV_READER_H
#define EX04_WAV_READER_H
#include <QFile>
#include <QMetaType>
#include <QMutex>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

struct WavInfo
{
    WavInfo() : dataOffset(0), frames(0), sampleRate(44100) {}
    qint64 dataOffset;
    qint64 frames;
    int sampleRate;
    double duration() const { return double(frames) / sampleRate; }
};
struct WavePeak
{
    WavePeak() : low(32767), high(-32768), valid(false) {}
    qint16 low, high;
    bool valid;
    void add(qint16 value) { low = qMin(low, value); high = qMax(high, value); valid = true; }
};
struct WaveformSlice
{
    WaveformSlice() : stream(0), generation(0), firstFrame(0), frameCount(0) {}
    int stream;
    quint64 generation;
    QString path;
    WavInfo info;
    qint64 firstFrame, frameCount;
    QVector<WavePeak> peaks[2];
};
Q_DECLARE_METATYPE(WaveformSlice)

// One persistent worker per source. New viewport/file jobs cancel obsolete reads.
// Storage is bounded by 4096 envelope buckets and a 64 KiB read block.
class WavReader : public QThread
{
    Q_OBJECT
public:
    explicit WavReader(int stream, QObject *parent = 0);
    ~WavReader();
    static bool readInfo(QFile &file, WavInfo *info, QString *error);
    void request(const QString &path, quint64 generation, qint64 firstFrame = 0,
                 qint64 frameCount = -1, int buckets = 2048);
    void requestCursor(const QString &path, quint64 generation, qint64 frame);
    void cancel();
    void shutdown();
signals:
    void ready(const WaveformSlice &slice);
    void failed(int stream, quint64 generation, const QString &error);
    void cursorReady(int stream, quint64 generation, qint64 frame, int left, int right);
protected:
    void run();
private:
    struct Job {
        QString path;
        quint64 generation;
        qint64 firstFrame, frameCount;
        int buckets;
    };
    bool obsolete(quint64 serial);
    int stream_;
    QMutex mutex_;
    QWaitCondition wake_;
    Job job_, cursor_;
    quint64 serial_;
    bool pending_, cursorPending_, stopping_;
};
#endif
