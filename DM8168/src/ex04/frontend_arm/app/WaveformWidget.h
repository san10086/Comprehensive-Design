#ifndef EX04_WAVEFORM_WIDGET_H
#define EX04_WAVEFORM_WIDGET_H
#include <QWidget>
#include <QTimer>
#include "AudioBackend.h"
#include "WavReader.h"

class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = 0);
    ~WaveformWidget();
    void openFile(int stream, const QString &path);
    void clearSource(int stream);
    void beginSession(quint64 session, bool demo, bool keepFiles);
    void showDemo();
    bool hasFile(int stream) const;
    QString filePath(int stream) const;
    double fileDuration() const;
    bool isPaused() const { return paused_; }
    bool isLoading() const;
    double visibleDuration() const { return duration_; }
    double visibleStart() const { return start_; }
    qint64 retainedFrames(int stream) const;
public slots:
    void appendChunk(const PcmChunk &chunk);
    void setPosition(double seconds);
    void zoomIn();
    void zoomOut();
    void fitAll();
    void togglePaused();
signals:
    void fileLoaded(int stream, const QString &path);
    void fileError(int stream, const QString &message);
    void pausedChanged(bool paused);
    void loadingChanged();
protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
private slots:
    void sliceReady(const WaveformSlice &slice);
    void readFailed(int stream, quint64 generation, const QString &error);
    void cursorReady(int stream, quint64 generation, qint64 frame, int left, int right);
    void requestViews();
    void requestCursor();
private:
    enum { SampleRate = 44100, Capacity = 220500 };
    struct Source {
        Source() : generation(0), live(false), loading(false), total(0), first(0),
                   frozenTotal(0), frozenFirst(0), cursorValid(false) { cursor[0] = cursor[1] = 0; }
        quint64 generation;
        QString path;
        WavInfo info;
        WaveformSlice overview, view;
        bool live, loading;
        QVector<qint16> ring, frozen;
        qint64 total, first, frozenTotal, frozenFirst;
        bool cursorValid;
        int cursor[2];
    } sources_[2];
    WavReader *readers_[2];
    QTimer viewportTimer_, cursorTimer_;
    quint64 session_;
    bool demo_, paused_, dragging_;
    double start_, duration_, cursor_, playhead_, pressStart_;
    QPoint pressPoint_;
    bool moved_;
    QRect plotRect() const;
    bool hasLive() const;
    double endTime() const;
    void zoom(double factor, double anchor);
    void freeze();
    void clampView();
    void updateCursor(double seconds);
    QVector<WavePeak> livePeaks(int stream, int channel, int width) const;
};
#endif
