#include "WaveformWidget.h"
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent), session_(0), demo_(true), paused_(false), dragging_(false),
      start_(0), duration_(5), cursor_(-1), playhead_(-1), pressStart_(0), moved_(false)
{
    setObjectName("waveform");
    setMinimumSize(360, 210);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    for (int i = 0; i < 2; ++i) {
        readers_[i] = new WavReader(i, this);
        connect(readers_[i], SIGNAL(ready(WaveformSlice)), this, SLOT(sliceReady(WaveformSlice)));
        connect(readers_[i], SIGNAL(failed(int,quint64,QString)), this, SLOT(readFailed(int,quint64,QString)));
        connect(readers_[i], SIGNAL(cursorReady(int,quint64,qint64,int,int)),
                this, SLOT(cursorReady(int,quint64,qint64,int,int)));
    }
    viewportTimer_.setSingleShot(true);
    cursorTimer_.setSingleShot(true);
    connect(&viewportTimer_, SIGNAL(timeout()), this, SLOT(requestViews()));
    connect(&cursorTimer_, SIGNAL(timeout()), this, SLOT(requestCursor()));
}
WaveformWidget::~WaveformWidget()
{
    for (int i = 0; i < 2; ++i) readers_[i]->shutdown();
}
QRect WaveformWidget::plotRect() const { return QRect(65, 25, width() - 83, height() - 79); }
bool WaveformWidget::hasFile(int stream) const { return !sources_[stream].path.isEmpty() && sources_[stream].info.frames > 0; }
QString WaveformWidget::filePath(int stream) const { return sources_[stream].path; }
bool WaveformWidget::isLoading() const { return sources_[0].loading || sources_[1].loading; }
double WaveformWidget::fileDuration() const { return qMax(sources_[0].info.duration(), sources_[1].info.duration()); }
bool WaveformWidget::hasLive() const { return sources_[0].live || sources_[1].live; }
qint64 WaveformWidget::retainedFrames(int stream) const
{
    const Source &s = sources_[stream];
    return s.live ? s.total - s.first : 0;
}
void WaveformWidget::clearSource(int stream)
{
    readers_[stream]->cancel();
    const quint64 next = sources_[stream].generation + 1;
    sources_[stream] = Source();
    sources_[stream].generation = next;
    update();
    emit loadingChanged();
}
void WaveformWidget::openFile(int stream, const QString &path)
{
    if (stream < 0 || stream > 1) return;
    // Importing real data never leaves a simulated counterpart on screen.
    for (int i = 0; i < 2; ++i) if (sources_[i].live) clearSource(i);
    clearSource(stream);
    Source &s = sources_[stream];
    s.path = path;
    s.loading = true;
    paused_ = false;
    cursor_ = playhead_ = -1;
    emit pausedChanged(false);
    emit loadingChanged();
    readers_[stream]->request(path, s.generation);
    update();
}
void WaveformWidget::beginSession(quint64 session, bool demo, bool keepFiles)
{
    session_ = session;
    demo_ = demo;
    paused_ = false;
    emit pausedChanged(false);
    cursor_ = playhead_ = -1;
    if (!keepFiles) { clearSource(0); clearSource(1); start_ = 0; duration_ = 5; }
    else fitAll();
    update();
}
void WaveformWidget::showDemo()
{
    beginSession(session_, true, false);
    appendChunk(DemoBackend::makeChunk(session_, 0, 0, Capacity));
    appendChunk(DemoBackend::makeChunk(session_, 1, 0, Capacity));
}
void WaveformWidget::appendChunk(const PcmChunk &chunk)
{
    if (chunk.sessionId != session_ || chunk.stream < 0 || chunk.stream > 1 ||
            chunk.channels != 2 || chunk.sampleRate != SampleRate ||
            chunk.samples.size() % 2 || chunk.firstFrame < 0 || chunk.samples.isEmpty()) return;
    Source &s = sources_[chunk.stream];
    if (!s.path.isEmpty()) return;
    if (s.live && chunk.firstFrame < s.total) return;
    const int count = chunk.samples.size() / 2;
    if (!s.live || chunk.firstFrame != s.total) {
        s.ring.resize(Capacity * 2);
        s.first = chunk.firstFrame;
    }
    s.live = true;
    for (int i = qMax(0, count - Capacity); i < count; ++i) {
        const int at = int((chunk.firstFrame + i) % Capacity) * 2;
        s.ring[at] = chunk.samples[i * 2];
        s.ring[at + 1] = chunk.samples[i * 2 + 1];
    }
    s.total = chunk.firstFrame + count;
    s.first = qMax(s.first, s.total - Capacity);
    if (!paused_) {
        start_ = qMax(0.0, endTime() - 5.0);
        duration_ = 5.0;
        if (cursor_ >= 0) updateCursor(cursor_);
        update();
    }
}
void WaveformWidget::setPosition(double seconds)
{
    if (!paused_) { playhead_ = seconds; update(); }
}
double WaveformWidget::endTime() const
{
    double end = fileDuration();
    for (int i = 0; i < 2; ++i) {
        const Source &s = sources_[i];
        if (s.live) end = qMax(end, double(paused_ ? s.frozenTotal : s.total) / SampleRate);
    }
    return qMax(end, 0.0001);
}
void WaveformWidget::clampView()
{
    const double end = qMax(hasLive() ? 5.0 : 0.0001, endTime());
    const double maximumSpan = hasLive() ? 5.0 : end;
    duration_ = qBound(16.0 / SampleRate, duration_, qMax(maximumSpan, 16.0 / SampleRate));
    double earliest = 0;
    if (hasLive()) {
        qint64 first = 0;
        for (int i = 0; i < 2; ++i) if (sources_[i].live)
            first = qMax(first, paused_ ? sources_[i].frozenFirst : sources_[i].first);
        earliest = double(first) / SampleRate;
    }
    start_ = qBound(earliest, start_, qMax(earliest, end - duration_));
}
void WaveformWidget::freeze()
{
    if (paused_) return;
    for (int i = 0; i < 2; ++i) {
        sources_[i].frozen = sources_[i].ring;
        sources_[i].frozenFirst = sources_[i].first;
        sources_[i].frozenTotal = sources_[i].total;
    }
    paused_ = true;
    emit pausedChanged(true);
}
void WaveformWidget::togglePaused()
{
    if (!paused_) freeze();
    else {
        paused_ = false;
        for (int i = 0; i < 2; ++i) sources_[i].frozen.clear();
        emit pausedChanged(false);
        if (hasLive()) fitAll();
    }
    update();
}
void WaveformWidget::fitAll()
{
    duration_ = hasLive() ? 5.0 : qMax(fileDuration(), 5.0 / SampleRate);
    if (!hasLive() && !hasFile(0) && !hasFile(1)) duration_ = 5;
    start_ = hasLive() ? qMax(0.0, endTime() - 5) : 0;
    viewportTimer_.start(40);
    update();
}
void WaveformWidget::zoom(double factor, double anchor)
{
    if (hasLive()) freeze();
    const double position = start_ + duration_ * anchor;
    duration_ *= factor;
    start_ = position - duration_ * anchor;
    clampView();
    viewportTimer_.start(80);
    update();
}
void WaveformWidget::zoomIn() { zoom(0.5, 0.5); }
void WaveformWidget::zoomOut() { zoom(2.0, 0.5); }
void WaveformWidget::requestViews()
{
    for (int i = 0; i < 2; ++i) {
        Source &s = sources_[i];
        if (!hasFile(i)) continue;
        ++s.generation;
        readers_[i]->request(s.path, s.generation, qint64(start_ * SampleRate),
            qMax(qint64(1), qint64(duration_ * SampleRate)), qMax(1, plotRect().width()));
    }
    cursorTimer_.start(100);
}
void WaveformWidget::sliceReady(const WaveformSlice &slice)
{
    Source &s = sources_[slice.stream];
    if (slice.generation != s.generation || slice.path != s.path) return;
    const bool initial = s.info.frames == 0;
    s.info = slice.info;
    s.view = slice;
    s.loading = false;
    if (initial) {
        s.overview = slice;
        fitAll();
        emit fileLoaded(slice.stream, slice.path);
        emit loadingChanged();
    }
    update();
}
void WaveformWidget::readFailed(int stream, quint64 generation, const QString &error)
{
    if (generation != sources_[stream].generation) return;
    clearSource(stream);
    emit fileError(stream, error);
}
void WaveformWidget::updateCursor(double seconds)
{
    cursor_ = qMax(0.0, seconds);
    for (int i = 0; i < 2; ++i) {
        Source &s = sources_[i];
        s.cursorValid = false;
        if (!s.live) continue;
        const qint64 frame = qint64(cursor_ * SampleRate);
        const qint64 first = paused_ ? s.frozenFirst : s.first;
        const qint64 last = paused_ ? s.frozenTotal : s.total;
        const QVector<qint16> &data = paused_ ? s.frozen : s.ring;
        if (frame >= first && frame < last && data.size() == Capacity * 2) {
            const int at = int(frame % Capacity) * 2;
            s.cursor[0] = data[at]; s.cursor[1] = data[at + 1]; s.cursorValid = true;
        }
    }
    cursorTimer_.start(50);
    update();
}
void WaveformWidget::requestCursor()
{
    if (cursor_ < 0) return;
    for (int i = 0; i < 2; ++i) if (hasFile(i))
        readers_[i]->requestCursor(sources_[i].path, sources_[i].generation, qint64(cursor_ * SampleRate));
}
void WaveformWidget::cursorReady(int stream, quint64 generation, qint64 frame, int left, int right)
{
    Source &s = sources_[stream];
    if (generation != s.generation || frame != qint64(cursor_ * SampleRate)) return;
    s.cursor[0] = left; s.cursor[1] = right; s.cursorValid = true;
    update();
}
QVector<WavePeak> WaveformWidget::livePeaks(int stream, int channel, int width) const
{
    QVector<WavePeak> peaks(qMax(1, width));
    const Source &s = sources_[stream];
    const QVector<qint16> &data = paused_ ? s.frozen : s.ring;
    if (data.size() != Capacity * 2) return peaks;
    const qint64 from = qMax(qint64(start_ * SampleRate), paused_ ? s.frozenFirst : s.first);
    const qint64 to = qMin(qint64((start_ + duration_) * SampleRate), paused_ ? s.frozenTotal : s.total);
    for (qint64 frame = from; frame < to; ++frame) {
        const int x = int((double(frame) / SampleRate - start_) * width / duration_);
        if (x >= 0 && x < width) peaks[x].add(data[int(frame % Capacity) * 2 + channel]);
    }
    return peaks;
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    const QColor ink("#193b63"), muted("#72869d"), grid("#e6edf4");
    const QRect plot = plotRect();
    const int laneHeight = plot.height() / 4;
    QFont small = font(); small.setPixelSize(11);
    QFont normal = font(); normal.setPixelSize(12);
    p.setFont(normal);
    p.setPen(muted);
    QString names[2];
    for (int i = 0; i < 2; ++i) {
        const Source &s = sources_[i];
        names[i] = s.live ? QString::fromUtf8(demo_ ? "演示信号" : "实时信号") :
                   (s.loading ? QString::fromUtf8("读取中…") : QFileInfo(s.path).fileName());
        if (names[i].isEmpty()) names[i] = QString::fromUtf8("未载入");
    }
    p.drawText(QRect(0, 0, width() / 2, 20), Qt::AlignLeft | Qt::AlignVCenter,
        fontMetrics().elidedText(QString::fromUtf8("输入  ·  ") + names[0], Qt::ElideMiddle, width() / 2 - 15));
    p.drawText(QRect(width() / 2, 0, width() / 2, 20), Qt::AlignRight | Qt::AlignVCenter,
        fontMetrics().elidedText(QString::fromUtf8("输出  ·  ") + names[1], Qt::ElideMiddle, width() / 2 - 15));
    for (int lane = 0; lane < 4; ++lane) {
        const int stream = lane / 2, channel = lane % 2;
        const Source &s = sources_[stream];
        const QRect band(plot.left(), plot.top() + lane * laneHeight, plot.width(), laneHeight);
        const double middle = band.center().y();
        const double scale = (laneHeight - 14) / 2.0 / 32768.0;
        p.fillRect(band, lane % 2 ? QColor("#fbfdff") : QColor("#f6f9fc"));
        p.setPen(grid);
        for (int tick = 0; tick <= 5; ++tick) {
            const int x = plot.left() + tick * plot.width() / 5;
            p.drawLine(x, band.top(), x, band.bottom());
        }
        p.drawLine(band.left(), int(middle), band.right(), int(middle));
        p.setPen(ink); p.setFont(normal);
        p.drawText(QRect(0, int(middle) - 10, 43, 20), Qt::AlignLeft | Qt::AlignVCenter,
            QString::fromUtf8(stream == 0 ? "输入 " : "输出 ") + (channel == 0 ? "L" : "R"));
        p.setFont(small); p.setPen(muted);
        p.drawText(QRect(43, band.top(), 17, 14), Qt::AlignRight, "+1");
        p.drawText(QRect(43, band.bottom() - 14, 17, 14), Qt::AlignRight, "−1");
        const QColor trace = channel == 0 ? QColor("#245d97") : QColor("#5699c8");
        p.save(); p.setClipRect(band); p.setPen(QPen(trace, 1));
        QVector<WavePeak> peaks;
        double first = start_, span = duration_;
        if (s.live) peaks = livePeaks(stream, channel, plot.width());
        else {
            peaks = s.view.peaks[channel];
            first = double(s.view.firstFrame) / SampleRate;
            span = double(s.view.frameCount) / SampleRate;
        }
        bool any = false;
        QPointF previous;
        bool havePrevious = false;
        for (int j = 0; j < peaks.size(); ++j) {
            if (!peaks[j].valid) { havePrevious = false; continue; }
            const double x = plot.left() + ((first + span * j / peaks.size()) - start_) * plot.width() / duration_;
            if (x < plot.left() - 2 || x > plot.right() + 2) continue;
            any = true;
            const QPointF top(x, middle - peaks[j].high * scale);
            const QPointF bottom(x, middle - peaks[j].low * scale);
            p.drawLine(top, bottom);
            if (peaks[j].high == peaks[j].low) {
                if (havePrevious) p.drawLine(previous, top);
                else p.drawPoint(top);
                previous = top; havePrevious = true;
            } else havePrevious = false;
        }
        p.restore();
        if (!any) {
            p.setPen(muted); p.setFont(normal);
            const QString empty = (s.live || hasFile(stream)) ? QString::fromUtf8("此时段无数据") :
                QString::fromUtf8(stream == 0 ? "载入 WAV 或开始演示" : "等待输出数据");
            p.drawText(band, Qt::AlignCenter, s.loading ? QString::fromUtf8("读取中…") : empty);
        }
    }
    p.setPen(grid); p.drawLine(plot.bottomLeft(), plot.bottomRight());
    p.setFont(small); p.setPen(muted);
    for (int tick = 0; tick <= 5; ++tick) {
        const int x = plot.left() + tick * plot.width() / 5;
        const QString value = QString::number(start_ + duration_ * tick / 5.0, 'f', duration_ < .1 ? 4 : 2);
        p.drawText(QRect(x - 25, plot.bottom() + 5, 50, 18), Qt::AlignCenter, value);
    }
    if (playhead_ >= start_ && playhead_ <= start_ + duration_) {
        const int x = plot.left() + int((playhead_ - start_) * plot.width() / duration_);
        p.setPen(QPen(QColor("#a9bdcf"), 1, Qt::DashLine));
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    QString readout = QString::fromUtf8("44.1 kHz    /    PCM 16    /    立体声");
    if (cursor_ >= start_ && cursor_ <= start_ + duration_) {
        const int x = plot.left() + int((cursor_ - start_) * plot.width() / duration_);
        p.setPen(QPen(ink, 1, Qt::DashLine)); p.drawLine(x, plot.top(), x, plot.bottom());
        readout = QString("%1 s").arg(cursor_, 0, 'f', 5);
        for (int i = 0; i < 2; ++i) {
            const Source &s = sources_[i];
            readout += QString::fromUtf8(i == 0 ? "    输入 " : "    输出 ");
            readout += s.cursorValid ? QString("L %1  R %2").arg(s.cursor[0] / 32768.0, 0, 'f', 3)
                .arg(s.cursor[1] / 32768.0, 0, 'f', 3) : QString("—");
        }
    }
    p.setPen(muted); p.setFont(small);
    p.drawText(QRect(0, height() - 23, width() - 50, 20), Qt::AlignLeft | Qt::AlignVCenter, readout);
    p.drawText(QRect(width() - 50, height() - 23, 50, 20), Qt::AlignRight | Qt::AlignVCenter, "t / s");
}
void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    viewportTimer_.start(100);
}
void WaveformWidget::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= 0x050000
    const int delta = event->angleDelta().y();
#else
    const int delta = event->delta();
#endif
#if QT_VERSION >= 0x050e00
    const double x = event->position().x();
#else
    const double x = event->pos().x();
#endif
    zoom(delta > 0 ? 0.8 : 1.25, qBound(0.0, (x - plotRect().left()) / plotRect().width(), 1.0));
    event->accept();
}
void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !plotRect().contains(event->pos())) return;
    dragging_ = true; moved_ = false; pressPoint_ = event->pos(); pressStart_ = start_;
    updateCursor(start_ + duration_ * (event->pos().x() - plotRect().left()) / plotRect().width());
}
void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging_) return;
    if ((event->pos() - pressPoint_).manhattanLength() > 4) moved_ = true;
    if (moved_) {
        if (hasLive()) freeze();
        start_ = pressStart_ - duration_ * (event->pos().x() - pressPoint_.x()) / plotRect().width();
        clampView(); viewportTimer_.start(80); update();
    }
}
void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging_ && !moved_)
        updateCursor(start_ + duration_ * (event->pos().x() - plotRect().left()) / plotRect().width());
    dragging_ = false;
}
