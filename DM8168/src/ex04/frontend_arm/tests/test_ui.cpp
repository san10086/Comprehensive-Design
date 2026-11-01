#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTemporaryFile>
#include "MainWindow.h"
#include "DirectionPicker.h"
#include "WaveformWidget.h"

class UiTest : public QObject
{
    Q_OBJECT
    QString root_;
    QString captures_;
    bool waitFor(QSignalSpy &spy, int count = 1, int timeout = 6000)
    {
        int waited = 0;
        while (spy.size() < count && waited < timeout) { QTest::qWait(20); waited += 20; }
        return spy.size() >= count;
    }
    void capture(QWidget &window, const QString &name)
    {
        if (captures_.isEmpty()) return;
        QTest::qWait(150);
#if QT_VERSION >= 0x050000
        QVERIFY(window.grab().save(captures_ + "/" + name + ".png"));
#else
        QVERIFY(QPixmap::grabWidget(&window).save(captures_ + "/" + name + ".png"));
#endif
    }
    void verifyBounds(MainWindow &window)
    {
        const QList<QWidget *> widgets = window.findChildren<QWidget *>();
        foreach (QWidget *widget, widgets) {
            if (!widget->isVisibleTo(&window) || widget->isWindow()) continue;
            if (!qobject_cast<QPushButton *>(widget) && !qobject_cast<QComboBox *>(widget) &&
                !qobject_cast<QLineEdit *>(widget) && !qobject_cast<QCheckBox *>(widget) &&
                !qobject_cast<QSpinBox *>(widget)) continue;
            const QRect bounds(widget->mapTo(&window, QPoint(0, 0)), widget->size());
            QVERIFY2(window.rect().contains(bounds), qPrintable(widget->objectName() + " outside window"));
        }
    }
private slots:
    void initTestCase()
    {
        root_ = QString::fromLocal8Bit(qgetenv("EX04_PROJECT_ROOT"));
        QVERIFY(QFile::exists(root_ + "/tests/spatial/input.wav"));
        captures_ = QString::fromLocal8Bit(qgetenv("EX04_SCREENSHOT_DIR"));
        if (!captures_.isEmpty()) QVERIFY(QDir().mkpath(captures_));
    }
    void namedParameters()
    {
        RunRequest r; r.mode = "file"; r.inputWav = "two words;$(no).wav"; r.outputWav = "swap";
        r.algorithm = "spatial"; r.direction = SoundDirection(60, 250);
        QVERIFY(r.validate().isEmpty());
        QCOMPARE(r.arguments(), QStringList() << "DSP" << "file" << "--input" << r.inputWav
            << "--output" << "swap" << "--algo" << "spatial" << "--elevation" << "60" << "--azimuth" << "250");
        r.algorithm = "dual"; r.sourceA = SoundDirection(0, 270); r.sourceB = SoundDirection(0, 90);
        QVERIFY(r.validate().isEmpty());
        QVERIFY(r.arguments().contains("--a-elevation")); QVERIFY(!r.arguments().contains("--elevation"));
        r.sourceB = SoundDirection(90, 1); QVERIFY(!r.validate().isEmpty());
        const QStringList old = QStringList() << "spatial40" << "spatial60" << "orbit" << "dual4060" << "6";
        foreach (const QString &a, old) { r.algorithm = a; QVERIFY(!r.validate().isEmpty()); }
        int count = 0;
        foreach (int e, SoundDirection::elevations()) foreach (int a, SoundDirection::azimuths(e)) {
            QVERIFY(SoundDirection(e, a).isValid()); ++count;
        }
        QCOMPARE(count, 710);
        r.mode = "record"; r.outputWav = "record.wav"; r.seconds = 0;
        QCOMPARE(r.arguments(), QStringList() << "record" << "--output" << "record.wav" << "--seconds" << "0");
    }
    void directionsDoNotSnap()
    {
        DirectionPicker picker("A", SoundDirection(0, 90));
        QComboBox *e = picker.findChild<QComboBox *>("elevation");
        QComboBox *a = picker.findChild<QComboBox *>("azimuth");
        QVERIFY(picker.isValid());
        e->setCurrentIndex(e->findData(90));
        QCOMPARE(a->currentIndex(), 0); QVERIFY(!picker.isValid());
        a->setCurrentIndex(a->findData(0)); QVERIFY(picker.isValid());
    }
    void wavReaderAndCursor()
    {
        WavReader reader(0);
        QSignalSpy ready(&reader, SIGNAL(ready(WaveformSlice)));
        QSignalSpy failed(&reader, SIGNAL(failed(int,quint64,QString)));
        QSignalSpy cursor(&reader, SIGNAL(cursorReady(int,quint64,qint64,int,int)));
        const QString path = root_ + "/tests/spatial/input.wav";
        reader.request(path, 1, 0, -1, 256);
        QVERIFY(waitFor(ready));
        WaveformSlice slice = qvariant_cast<WaveformSlice>(ready.takeFirst().at(0));
        QCOMPARE(slice.info.frames, qint64(220500)); QCOMPARE(slice.peaks[0].size(), 256);
        // Every raw sample must be contained in its envelope, including impulses.
        QFile file(path); WavInfo info; QString error; QVERIFY(WavReader::readInfo(file, &info, &error));
        QVERIFY(file.seek(info.dataOffset)); const QByteArray pcm = file.readAll();
        for (int f = 0; f < info.frames; ++f) for (int c = 0; c < 2; ++c) {
            const uchar *p = reinterpret_cast<const uchar *>(pcm.constData() + f * 4 + c * 2);
            const qint16 value = qint16(quint16(p[0]) | quint16(p[1]) << 8);
            const WavePeak &peak = slice.peaks[c][qint64(f) * 256 / info.frames];
            QVERIFY(peak.valid && value >= peak.low && value <= peak.high);
        }
        reader.requestCursor(path, 1, 1234); QVERIFY(waitFor(cursor));
        const uchar *sample = reinterpret_cast<const uchar *>(pcm.constData() + 1234 * 4);
        QCOMPARE(cursor.at(0).at(3).toInt(), int(qint16(quint16(sample[0]) | quint16(sample[1]) << 8)));
        reader.request(path, 2, 1000, 32, 32); QVERIFY(waitFor(ready));
        slice = qvariant_cast<WaveformSlice>(ready.takeFirst().at(0));
        QCOMPARE(slice.firstFrame, qint64(1000)); QCOMPARE(slice.frameCount, qint64(32));
        for (int i = 0; i < 32; ++i) QCOMPARE(slice.peaks[0][i].low, slice.peaks[0][i].high);
        QTemporaryFile broken; QVERIFY(broken.open()); broken.write("RIFFbad"); broken.flush();
        reader.request(broken.fileName(), 3); QVERIFY(waitFor(failed));
        // Quickly supersede an old request; the latest result must carry the new generation.
        ready.clear(); reader.request(path, 4); reader.request(root_ + "/tests/spatial/output_40e045.wav", 5);
        QVERIFY(waitFor(ready)); QTest::qWait(100);
        slice = qvariant_cast<WaveformSlice>(ready.last().at(0));
        QCOMPARE(slice.generation, quint64(5)); QCOMPARE(slice.info.frames, qint64(256409));
    }
    void malformedWav()
    {
        const QString path = root_ + "/tests/spatial/input.wav";
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const QByteArray bytes = original.readAll();
        for (int scenario = 0; scenario < 5; ++scenario) {
            QByteArray changed = bytes;
            if (scenario == 0) changed.truncate(100);
            if (scenario == 1) changed[22] = 1; // mono
            if (scenario == 2) changed[24] = 0; // wrong sample rate
            if (scenario == 3) changed[32] = 2; // invalid blockAlign
            if (scenario == 4) changed[34] = 8; // unsupported bit depth
            QTemporaryFile file; QVERIFY(file.open()); QCOMPARE(file.write(changed), qint64(changed.size())); file.flush();
            WavInfo info; QString error; QVERIFY(!WavReader::readInfo(file, &info, &error)); QVERIFY(!error.isEmpty());
        }
        // Unknown odd-sized chunks and their padding must be skipped correctly.
        QByteArray padded = bytes;
        padded.insert(12, QByteArray("JUNK\x01\x00\x00\x00x\x00", 10));
        const quint32 size = padded.size() - 8;
        for (int i = 0; i < 4; ++i) padded[4 + i] = char(size >> (8 * i));
        QTemporaryFile file; QVERIFY(file.open()); file.write(padded); file.flush();
        WavInfo info; QString error; QVERIFY(WavReader::readInfo(file, &info, &error)); QCOMPARE(info.frames, qint64(220500));
    }
    void waveformFilesAndBoundedStream()
    {
        WaveformWidget wave; wave.resize(768, 300); wave.show();
        QSignalSpy loaded(&wave, SIGNAL(fileLoaded(int,QString)));
        wave.openFile(0, root_ + "/tests/spatial/input.wav");
        wave.openFile(1, root_ + "/tests/spatial/output_40e045.wav");
        QVERIFY(waitFor(loaded, 2)); QTest::qWait(150);
        QVERIFY(wave.hasFile(0)); QVERIFY(wave.hasFile(1));
        QVERIFY(wave.fileDuration() > 5.8 && wave.fileDuration() < 5.9);
        const double duration = wave.visibleDuration(); wave.zoomIn(); QVERIFY(wave.visibleDuration() < duration);
        wave.fitAll(); QCOMPARE(wave.visibleStart(), 0.0);
        wave.beginSession(77, true, false);
        PcmChunk chunk; chunk.sessionId = 77; chunk.samples.fill(100, 2205 * 2);
        for (int i = 0; i < 250; ++i) { chunk.firstFrame = i * 2205; wave.appendChunk(chunk); }
        QCOMPARE(wave.retainedFrames(0), qint64(220500));
        wave.togglePaused(); const double frozen = wave.visibleStart();
        chunk.firstFrame = 250 * 2205; wave.appendChunk(chunk); QCOMPARE(wave.visibleStart(), frozen);
        wave.togglePaused(); QVERIFY(wave.visibleStart() > frozen);
        chunk.sessionId = 76; chunk.firstFrame += 2205; const double before = wave.visibleStart();
        wave.appendChunk(chunk); QCOMPARE(wave.visibleStart(), before);
        wave.openFile(0, root_ + "/tests/spatial/input.wav"); QVERIFY(waitFor(loaded, 3));
        QCOMPARE(wave.retainedFrames(0), qint64(0));
    }
    void demoLifecycle()
    {
        DemoBackend demo; RunRequest r; r.mode = "audio"; r.algorithm = "spatial"; r.seconds = 1;
        QSignalSpy data(&demo, SIGNAL(waveformReady(PcmChunk)));
        QSignalSpy done(&demo, SIGNAL(finished(bool,QString)));
        QVERIFY(demo.start(QString(), r, 9)); QVERIFY(!demo.start(QString(), r, 10));
        QVERIFY(waitFor(done)); QVERIFY(!demo.isBusy()); QVERIFY(data.size() > 10);
        QCOMPARE(qvariant_cast<PcmChunk>(data.first().at(0)).sessionId, quint64(9));
        r.seconds = 0; QVERIFY(demo.start(QString(), r, 10)); QTest::qWait(100); demo.stop(); QVERIFY(!demo.isBusy());
        data.clear(); r.mode = "file"; r.inputWav = "actual.wav"; demo.setFilePreview(true, .15);
        QVERIFY(demo.start(QString(), r, 11)); QVERIFY(waitFor(done, 3)); QCOMPARE(data.size(), 0);
    }
    void windowLayoutsAndInteractions()
    {
        MainWindow window; window.resize(800, 480); window.show(); QTest::qWait(150);
        QCOMPARE(window.size(), QSize(800, 480)); verifyBounds(window);
        capture(window, "800-wave-demo");
        QPushButton *start = window.findChild<QPushButton *>("start");
        QPushButton *stop = window.findChild<QPushButton *>("stop");
        QComboBox *mode = window.findChild<QComboBox *>("mode");
        QComboBox *algorithm = window.findChild<QComboBox *>("algorithm");
        QComboBox *environment = window.findChild<QComboBox *>("environment");
        QStackedWidget *pages = window.findChild<QStackedWidget *>("pages");
        QVERIFY(start && stop && mode && algorithm && environment && pages);
        const char *navs[] = {"navWave", "navParameters", "navDiagnostics", "navSettings"};
        const char *names[] = {"wave", "parameters", "diagnostics", "settings"};
        for (int size = 0; size < 2; ++size) {
            window.resize(size ? QSize(1024, 768) : QSize(800, 480));
            for (int page = 0; page < 4; ++page) {
                QTest::mouseClick(window.findChild<QPushButton *>(navs[page]), Qt::LeftButton); QTest::qWait(30);
                QCOMPARE(pages->currentIndex(), page); verifyBounds(window);
                capture(window, QString(size ? "1024-" : "800-") + names[page]);
            }
        }
        window.resize(800, 480); QTest::mouseClick(window.findChild<QPushButton *>("navParameters"), Qt::LeftButton);
        algorithm->setCurrentIndex(algorithm->findData("dual")); window.findChild<QCheckBox *>("saveOutput")->setChecked(true);
        verifyBounds(window); capture(window, "800-dual");
        mode->setCurrentIndex(1); window.findChild<QCheckBox *>("continuous")->setChecked(true);
        verifyBounds(window); capture(window, "800-realtime-parameters");
        mode->setCurrentIndex(2); verifyBounds(window); capture(window, "800-record-parameters");
        mode->setCurrentIndex(1); algorithm->setCurrentIndex(algorithm->findData("spatial"));
        QTest::mouseClick(start, Qt::LeftButton); QTest::qWait(150);
        QVERIFY(!start->isEnabled()); QVERIFY(stop->isEnabled()); QVERIFY(!environment->isEnabled());
        QTest::mouseClick(window.findChild<QPushButton *>("navDiagnostics"), Qt::LeftButton); QCOMPARE(pages->currentIndex(), 2);
        QTest::mouseClick(stop, Qt::LeftButton); QTest::qWait(50); QVERIFY(start->isEnabled()); QVERIFY(!stop->isEnabled());
        QTest::mouseClick(start, Qt::LeftButton); QTest::qWait(100); window.close(); QTest::qWait(100); QVERIFY(!window.isVisible());
    }
    void realFileUiAndBoardEmpty()
    {
        MainWindow window; window.resize(800, 480); window.show();
        WaveformWidget *wave = window.findChild<WaveformWidget *>("waveform");
        QLineEdit *input = window.findChild<QLineEdit *>("inputPath");
        QSignalSpy loaded(wave, SIGNAL(fileLoaded(int,QString)));
        input->setText(root_ + "/tests/spatial/input.wav");
        QVERIFY(QMetaObject::invokeMethod(&window, "loadInput")); QVERIFY(waitFor(loaded));
        wave->openFile(1, root_ + "/tests/spatial/output_40e045.wav"); QVERIFY(waitFor(loaded, 2));
        QTest::qWait(150); capture(window, "800-wave-files");
        QTest::mouseClick(wave, Qt::LeftButton, Qt::NoModifier, QPoint(300, 100)); QTest::qWait(150);
        capture(window, "800-wave-cursor");
        wave->zoomIn(); wave->zoomIn(); QTest::qWait(150); capture(window, "800-wave-zoom");
        QComboBox *env = window.findChild<QComboBox *>("environment"); input->clear(); env->setCurrentIndex(1);
        QCOMPARE(wave->retainedFrames(0), qint64(0)); QVERIFY(!wave->hasFile(0)); QVERIFY(!wave->hasFile(1));
        capture(window, "800-board-empty");
        QTest::mouseClick(window.findChild<QPushButton *>("start"), Qt::LeftButton);
        QVERIFY(window.findChild<QLabel *>("errorBanner")->isVisible());
        QCOMPARE(window.findChild<QStackedWidget *>("pages")->currentIndex(), 1);
        verifyBounds(window); capture(window, "800-validation");
    }
};
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("ex04-tests"); app.setApplicationName("ui-tests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::tempPath() + "/ex04-ui-settings");
    qRegisterMetaType<PcmChunk>("PcmChunk");
#if QT_VERSION >= 0x050000
    app.setStyle(QStyleFactory::create("Fusion"));
#endif
    QFont font("WenQuanYi Micro Hei"); font.setPixelSize(14); app.setFont(font);
    QFile theme(":/ui/theme.qss"); if (theme.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(theme.readAll()));
    UiTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_ui.moc"
