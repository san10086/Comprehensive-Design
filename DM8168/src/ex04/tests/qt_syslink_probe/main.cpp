#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
#include <QTimerEvent>
#include <QWidget>
#include <cstdio>

// Display-only probe: no DSP, ALSA, IPC, OpenGL, or network dependencies.
class DisplayProbe : public QWidget
{
public:
    DisplayProbe() : ticks_(0), clicks_(0), timerId_(startTimer(100))
    {
        setWindowTitle("Qt / SysLink coexistence probe");
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void timerEvent(QTimerEvent *event)
    {
        if (event->timerId() != timerId_)
            return;
        ++ticks_;
        update();
        if (ticks_ % 10 == 0) {
            const QByteArray time = QTime::currentTime()
                .toString("HH:mm:ss.zzz").toLatin1();
            std::fprintf(stderr, "QT_HEARTBEAT time=%s ticks=%lu clicks=%lu\n",
                         time.constData(), ticks_, clicks_);
            std::fflush(stderr);
        }
    }

    void mousePressEvent(QMouseEvent *)
    {
        ++clicks_;
        update();
    }

    void keyPressEvent(QKeyEvent *event)
    {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(20, 28, 40));
        painter.fillRect(20, 20, 60, 60,
                         (ticks_ / 5) % 2 ? Qt::green : Qt::yellow);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(24);
        painter.setFont(font);
        const QString text = QString(
            "Qt / SysLink display probe\n\n"
            "Time: %1\nTicks (100 ms): %2\nMouse clicks: %3\n\n"
            "Color and counters must keep changing.\nEscape: exit")
            .arg(QTime::currentTime().toString("HH:mm:ss"))
            .arg(ticks_).arg(clicks_);
        painter.drawText(rect().adjusted(20, 100, -20, -20),
                         Qt::AlignLeft | Qt::AlignTop, text);
    }

private:
    unsigned long ticks_;
    unsigned long clicks_;
    int timerId_;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    DisplayProbe probe;
    probe.showFullScreen();
    return app.exec();
}
