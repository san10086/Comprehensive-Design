#include "MainWindow.h"
#include "DirectionPicker.h"
#include "WaveformWidget.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTextCursor>
#include <QVBoxLayout>
#include <cstdio>

static QString tr8(const char *text) { return QString::fromUtf8(text); }
static QPushButton *button(const char *text, const char *name, QWidget *parent)
{
    QPushButton *b = new QPushButton(tr8(text), parent);
    b->setObjectName(name);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
static QLabel *label(const char *text, QWidget *parent, const char *role = "muted")
{
    QLabel *l = new QLabel(tr8(text), parent);
    l->setProperty("role", role);
    return l;
}
static QWidget *pathRow(QLineEdit *edit, QPushButton *browse, QWidget *parent)
{
    QWidget *row = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    edit->setMinimumWidth(70);
    layout->addWidget(edit, 1);
    browse->setFixedWidth(54);
    layout->addWidget(browse);
    return row;
}
static QString algorithmLabel(const QString &algorithm)
{
    if (algorithm == "spatial") return tr8("单声源空间定位");
    if (algorithm == "dual") return tr8("双声源空间定位");
    if (algorithm == "passthrough") return tr8("直通");
    if (algorithm == "swap") return tr8("左右声道交换");
    if (algorithm == "speed2") return tr8("倍速 ×2");
    return tr8("增益 ×") + algorithm.right(1);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), demo_(this), board_(this), ipc_(this), invalidField_(0),
      session_(0), closing_(false)
{
    setObjectName("mainWindow");
    setWindowTitle(tr8("EX04 · 空间音效"));
    setMinimumSize(800, 480);
    QWidget *central = new QWidget(this);
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    QFrame *header = new QFrame(central); header->setObjectName("header");
    QHBoxLayout *head = new QHBoxLayout(header);
    head->setContentsMargins(16, 8, 16, 8); head->setSpacing(8);
    head->addWidget(label("EX04", header, "brand"));
    head->addWidget(label("空间音效", header, "title"));
    head->addSpacing(16);
    navigation_ = new QButtonGroup(this);
    const char *navNames[] = {"波形", "参数", "诊断", "设置"};
    const char *navIds[] = {"navWave", "navParameters", "navDiagnostics", "navSettings"};
    for (int i = 0; i < 4; ++i) {
        QPushButton *b = button(navNames[i], navIds[i], header);
        b->setCheckable(true); b->setProperty("role", "nav"); b->setFixedWidth(62);
        navigation_->addButton(b, i); head->addWidget(b);
    }
    navigation_->button(0)->setChecked(true);
    head->addStretch();
    badge_ = label("演示", header, "badge"); badge_->setObjectName("environmentBadge");
    head->addWidget(badge_);
    QPushButton *exit = button("退出", "exit", header); exit->setProperty("role", "quiet");
    head->addWidget(exit);
    root->addWidget(header);
    error_ = label("", central, "error"); error_->setObjectName("errorBanner");
    error_->setWordWrap(true); error_->hide(); root->addWidget(error_);
    pages_ = new QStackedWidget(central); pages_->setObjectName("pages");
    pages_->addWidget(buildWavePage());
    parameterPage_ = buildParameterPage(); pages_->addWidget(parameterPage_);
    pages_->addWidget(buildDiagnosticPage());
    settingsPage_ = buildSettingsPage(); pages_->addWidget(settingsPage_);
    root->addWidget(pages_, 1);
    QFrame *footer = new QFrame(central); footer->setObjectName("footer");
    QHBoxLayout *foot = new QHBoxLayout(footer); foot->setContentsMargins(16, 9, 16, 9);
    QVBoxLayout *meta = new QVBoxLayout; meta->setSpacing(3);
    summary_ = label("", footer, "summary"); status_ = label("就绪", footer, "muted");
    summary_->setObjectName("taskSummary"); status_->setObjectName("runStatus");
    meta->addWidget(summary_); meta->addWidget(status_); foot->addLayout(meta, 1);
    start_ = button("开始演示", "start", footer); start_->setProperty("role", "primary");
    stop_ = button("停止", "stop", footer); start_->setFixedWidth(112); stop_->setFixedWidth(72);
    foot->addWidget(start_); foot->addWidget(stop_); root->addWidget(footer);
    setCentralWidget(central);

    connect(navigation_, SIGNAL(buttonClicked(int)), this, SLOT(navigate(int)));
    connect(exit, SIGNAL(clicked()), this, SLOT(close()));
    connect(mode_, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMode()));
    connect(algorithm_, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFields()));
    connect(continuous_, SIGNAL(toggled(bool)), this, SLOT(updateFields()));
    connect(saveOutput_, SIGNAL(toggled(bool)), this, SLOT(updateFields()));
    connect(environment_, SIGNAL(currentIndexChanged(int)), this, SLOT(changeEnvironment()));
    connect(input_, SIGNAL(editingFinished()), this, SLOT(loadInput()));
    connect(start_, SIGNAL(clicked()), this, SLOT(startAudio()));
    connect(stop_, SIGNAL(clicked()), this, SLOT(stopAudio()));
    connect(wave_, SIGNAL(fileLoaded(int,QString)), this, SLOT(waveformLoaded(int,QString)));
    connect(wave_, SIGNAL(fileError(int,QString)), this, SLOT(waveformError(int,QString)));
    connect(wave_, SIGNAL(loadingChanged()), this, SLOT(updateBusy()));
    connect(wave_, SIGNAL(pausedChanged(bool)), this, SLOT(updatePause(bool)));
    AudioBackend *backends[] = { &demo_, &board_ };
    for (int i = 0; i < 2; ++i) {
        connect(backends[i], SIGNAL(log(QString)), this, SLOT(appendLog(QString)));
        connect(backends[i], SIGNAL(stateChanged(QString)), this, SLOT(setState(QString)));
        connect(backends[i], SIGNAL(finished(bool,QString)), this, SLOT(pipelineFinished(bool,QString)));
        connect(backends[i], SIGNAL(waveformReady(PcmChunk)), wave_, SLOT(appendChunk(PcmChunk)));
        connect(backends[i], SIGNAL(positionChanged(double)), wave_, SLOT(setPosition(double)));
    }
    connect(&ipc_, SIGNAL(log(QString)), this, SLOT(appendLog(QString)));
    connect(&ipc_, SIGNAL(message(uint,QString)), this, SLOT(showMessage(uint,QString)));
    connect(&ipc_, SIGNAL(finished(bool,QString)), this, SLOT(ipcFinished(bool,QString)));
    updateMode();
    wave_->showDemo();
    appendLog(tr8("界面就绪 · 演示环境"));
}

QWidget *MainWindow::buildWavePage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page); layout->setContentsMargins(16, 10, 16, 6); layout->setSpacing(8);
    QHBoxLayout *toolbar = new QHBoxLayout; toolbar->setSpacing(6);
    toolbar->addWidget(label("波形监视", page, "section")); toolbar->addStretch();
    importInput_ = button("输入 WAV", "importInput", page);
    importOutput_ = button("对比 WAV", "importComparison", page);
    demoReset_ = button("演示信号", "demoSignal", page);
    QPushButton *minus = button("−", "zoomOut", page), *plus = button("+", "zoomIn", page);
    minus->setFixedWidth(36); plus->setFixedWidth(36);
    minus->setAccessibleName(tr8("缩小波形")); plus->setAccessibleName(tr8("放大波形"));
    QPushButton *fit = button("全览", "fitWave", page);
    pause_ = button("暂停观察", "pauseWave", page);
    toolbar->addWidget(importInput_); toolbar->addWidget(importOutput_); toolbar->addWidget(demoReset_);
    toolbar->addSpacing(6); toolbar->addWidget(minus); toolbar->addWidget(plus);
    toolbar->addWidget(fit); toolbar->addWidget(pause_); layout->addLayout(toolbar);
    wave_ = new WaveformWidget(page); layout->addWidget(wave_, 1);
    connect(importInput_, SIGNAL(clicked()), this, SLOT(browseInput()));
    connect(importOutput_, SIGNAL(clicked()), this, SLOT(browseComparison()));
    connect(demoReset_, SIGNAL(clicked()), this, SLOT(restoreDemo()));
    connect(minus, SIGNAL(clicked()), wave_, SLOT(zoomOut()));
    connect(plus, SIGNAL(clicked()), wave_, SLOT(zoomIn()));
    connect(fit, SIGNAL(clicked()), wave_, SLOT(fitAll()));
    connect(pause_, SIGNAL(clicked()), wave_, SLOT(togglePaused()));
    return page;
}

QWidget *MainWindow::buildParameterPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page); layout->setContentsMargins(16, 10, 16, 10); layout->setSpacing(10);
    QHBoxLayout *heading = new QHBoxLayout;
    heading->addWidget(label("任务参数", page, "section")); heading->addStretch();
    mode_ = new QComboBox(page); mode_->setObjectName("mode"); mode_->setMinimumWidth(180);
    mode_->addItem(tr8("文件处理"), "file"); mode_->addItem(tr8("实时处理"), "audio"); mode_->addItem(tr8("录音"), "record");
    heading->addWidget(mode_); layout->addLayout(heading);
    QHBoxLayout *columns = new QHBoxLayout; columns->setSpacing(14);
    QGroupBox *source = new QGroupBox(tr8("音源与输出"), page);
    QVBoxLayout *src = new QVBoxLayout(source); src->setSpacing(8); src->setContentsMargins(12, 24, 12, 12);
    input_ = new QLineEdit(source); input_->setObjectName("inputPath");
    inputRow_ = new QWidget(source); QVBoxLayout *in = new QVBoxLayout(inputRow_);
    in->setContentsMargins(0, 0, 0, 0); in->setSpacing(4); in->addWidget(label("输入文件", source));
    QPushButton *inputBrowse = button("选择", "browseInput", source);
    in->addWidget(pathRow(input_, inputBrowse, source)); src->addWidget(inputRow_);
    durationRow_ = new QWidget(source); QHBoxLayout *dur = new QHBoxLayout(durationRow_);
    dur->setContentsMargins(0, 0, 0, 0); dur->setSpacing(8);
    seconds_ = new QSpinBox(source); seconds_->setObjectName("seconds"); seconds_->setRange(1, 2147);
    seconds_->setValue(5); seconds_->setSuffix(tr8(" 秒"));
    continuous_ = new QCheckBox(tr8("持续运行"), source); continuous_->setObjectName("continuous");
    dur->addWidget(label("时长", source)); dur->addWidget(seconds_, 1); dur->addWidget(continuous_);
    src->addWidget(durationRow_);
    saveOutput_ = new QCheckBox(tr8("保存处理结果"), source); saveOutput_->setObjectName("saveOutput");
    src->addWidget(saveOutput_);
    output_ = new QLineEdit(source); output_->setObjectName("outputPath");
    outputRow_ = new QWidget(source); QVBoxLayout *out = new QVBoxLayout(outputRow_);
    out->setContentsMargins(0, 0, 0, 0); out->setSpacing(4); out->addWidget(label("保存位置", source));
    QPushButton *outputBrowse = button("选择", "browseOutput", source);
    out->addWidget(pathRow(output_, outputBrowse, source)); src->addWidget(outputRow_);
    src->addStretch();
    QLabel *format = label("44.1 kHz  ·  双声道  ·  PCM 16", source); src->addWidget(format);
    columns->addWidget(source, 1);
    QGroupBox *effect = new QGroupBox(tr8("声音处理"), page);
    QVBoxLayout *fx = new QVBoxLayout(effect); fx->setContentsMargins(12, 24, 12, 12); fx->setSpacing(10);
    algorithm_ = new QComboBox(effect); algorithm_->setObjectName("algorithm"); fx->addWidget(algorithm_);
    directions_ = new QWidget(effect); QVBoxLayout *directionLayout = new QVBoxLayout(directions_);
    directionLayout->setContentsMargins(0, 0, 0, 0); directionLayout->setSpacing(8);
    QHBoxLayout *angleLabels = new QHBoxLayout;
    angleLabels->addSpacing(42); angleLabels->addWidget(label("仰角", directions_), 1);
    angleLabels->addWidget(label("方位角", directions_), 1); directionLayout->addLayout(angleLabels);
    single_ = new DirectionPicker(tr8("声源"), SoundDirection(0, 0), directions_); single_->setObjectName("singleDirection");
    sourceA_ = new DirectionPicker("A", SoundDirection(0, 270), directions_); sourceA_->setObjectName("sourceA");
    sourceB_ = new DirectionPicker("B", SoundDirection(0, 90), directions_); sourceB_->setObjectName("sourceB");
    directionLayout->addWidget(single_); directionLayout->addWidget(sourceA_); directionLayout->addWidget(sourceB_);
    directionLayout->addWidget(label("前 0°  ·  右 90°  ·  后 180°  ·  左 270°", directions_));
    fx->addWidget(directions_); fx->addStretch();
    columns->addWidget(effect, 1); layout->addLayout(columns, 1);
    connect(inputBrowse, SIGNAL(clicked()), this, SLOT(browseInput()));
    connect(outputBrowse, SIGNAL(clicked()), this, SLOT(browseOutput()));
    return page;
}

QWidget *MainWindow::buildDiagnosticPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page); layout->setContentsMargins(16, 10, 16, 10); layout->setSpacing(8);
    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(label("运行记录", page, "section")); header->addStretch();
    QPushButton *clear = button("清空", "clearLog", page), *save = button("保存日志", "saveLog", page);
    header->addWidget(clear); header->addWidget(save); layout->addLayout(header);
    log_ = new QPlainTextEdit(page); log_->setObjectName("log"); log_->setReadOnly(true); log_->setMaximumBlockCount(1500);
    layout->addWidget(log_, 1);
    QHBoxLayout *checks = new QHBoxLayout; checks->setSpacing(6);
    smokeAlgorithm_ = new QComboBox(page); smokeAlgorithm_->setObjectName("smokeAlgorithm");
    foreach (const QString &a, RunRequest::algorithms("smoke")) smokeAlgorithm_->addItem(algorithmLabel(a), a);
    smoke_ = button("链路自检", "smoke", page); sync_ = button("信号量", "ipcSync", page);
    data_ = button("共享内存", "ipcData", page); ipcStop_ = button("停止验证", "ipcStop", page);
    checks->addWidget(smokeAlgorithm_); checks->addWidget(smoke_); checks->addSpacing(6);
    checks->addWidget(sync_); checks->addWidget(data_); checks->addWidget(ipcStop_); checks->addStretch();
    ipcStatus_ = label("IPC 就绪", page); checks->addWidget(ipcStatus_); layout->addLayout(checks);
    connect(clear, SIGNAL(clicked()), log_, SLOT(clear())); connect(save, SIGNAL(clicked()), this, SLOT(saveLog()));
    connect(smoke_, SIGNAL(clicked()), this, SLOT(startSmoke()));
    connect(sync_, SIGNAL(clicked()), this, SLOT(startSync())); connect(data_, SIGNAL(clicked()), this, SLOT(startData()));
    connect(ipcStop_, SIGNAL(clicked()), this, SLOT(stopIpc()));
    return page;
}

QWidget *MainWindow::buildSettingsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page); layout->setContentsMargins(16, 10, 16, 16); layout->setSpacing(12);
    layout->addWidget(label("运行设置", page, "section"));
    QGroupBox *group = new QGroupBox(tr8("环境"), page);
    QVBoxLayout *form = new QVBoxLayout(group); form->setContentsMargins(16, 26, 16, 16); form->setSpacing(10);
    QHBoxLayout *envRow = new QHBoxLayout; envRow->addWidget(label("运行环境", group)); envRow->addStretch();
    environment_ = new QComboBox(group); environment_->setObjectName("environment"); environment_->setMinimumWidth(220);
    environment_->addItem(tr8("演示环境")); environment_->addItem(tr8("板端环境")); envRow->addWidget(environment_); form->addLayout(envRow);
    form->addWidget(label("工作目录", group));
    directory_ = new QLineEdit(group); directory_->setObjectName("businessDirectory");
    directory_->setText(QSettings().value("arm/businessDirectory", "/home/root/ex04_syslink_dsp/release").toString());
    QPushButton *browse = button("选择", "browseDirectory", group); form->addWidget(pathRow(directory_, browse, group));
    layout->addWidget(group); layout->addWidget(label("演示环境不写入音频文件。", page)); layout->addStretch();
    connect(browse, SIGNAL(clicked()), this, SLOT(browseDirectory()));
    return page;
}

bool MainWindow::isDemo() const { return environment_->currentIndex() == 0; }
AudioBackend *MainWindow::backend() { return isDemo() ? static_cast<AudioBackend *>(&demo_) : static_cast<AudioBackend *>(&board_); }
QString MainWindow::mode() const { return mode_->itemData(mode_->currentIndex()).toString(); }
QString MainWindow::algorithm() const { return algorithm_->itemData(algorithm_->currentIndex()).toString(); }
QString MainWindow::absolutePath(const QString &path) const { return QDir(directory_->text()).absoluteFilePath(path); }
void MainWindow::navigate(int page) { pages_->setCurrentIndex(page); navigation_->button(page)->setChecked(true); }
void MainWindow::updateMode()
{
    const QString previous = algorithm();
    algorithm_->blockSignals(true); algorithm_->clear();
    QStringList choices = RunRequest::algorithms(mode());
    if (choices.contains("spatial")) { choices.removeAll("spatial"); choices.prepend("spatial"); }
    if (choices.contains("dual")) { choices.removeAll("dual"); choices.insert(1, "dual"); }
    foreach (const QString &a, choices) algorithm_->addItem(algorithmLabel(a), a);
    const int found = algorithm_->findData(previous);
    if (found >= 0) algorithm_->setCurrentIndex(found);
    if (mode() == "record") algorithm_->addItem(tr8("原始录音"), "");
    algorithm_->blockSignals(false);
    clearError(); updateFields();
}
void MainWindow::updateFields()
{
    const QString m = mode(), a = algorithm();
    inputRow_->setVisible(m == "file"); durationRow_->setVisible(m != "file");
    seconds_->setEnabled(!continuous_->isChecked()); saveOutput_->setVisible(m != "record");
    outputRow_->setVisible(m == "record" || saveOutput_->isChecked());
    algorithm_->setEnabled(m != "record");
    directions_->setVisible(m != "record" && (a == "spatial" || a == "dual"));
    single_->setVisible(a == "spatial"); sourceA_->setVisible(a == "dual"); sourceB_->setVisible(a == "dual");
    summary_->setText(mode_->currentText() + tr8("  ·  ") + algorithm_->currentText());
    updateBusy();
}
void MainWindow::updateBusy()
{
    const bool busy = demo_.isBusy() || board_.isBusy();
    const bool editable = !busy && !ipc_.isBusy() && !closing_;
    parameterPage_->setEnabled(editable); settingsPage_->setEnabled(editable);
    importInput_->setEnabled(editable); importOutput_->setEnabled(editable);
    demoReset_->setEnabled(editable && isDemo());
    start_->setEnabled(editable && !backend()->isBlocked() && !wave_->isLoading());
    start_->setText(isDemo() ? tr8("开始演示") : tr8("开始运行"));
    stop_->setEnabled(busy && !closing_);
    smoke_->setEnabled(editable && !backend()->isBlocked()); smokeAlgorithm_->setEnabled(editable);
    sync_->setEnabled(editable); data_->setEnabled(editable); ipcStop_->setEnabled(ipc_.isBusy() && !closing_);
}
void MainWindow::clearError()
{
    error_->hide();
    if (invalidField_) {
        invalidField_->setProperty("invalid", false); invalidField_->style()->unpolish(invalidField_);
        invalidField_->style()->polish(invalidField_); invalidField_ = 0;
    }
}
void MainWindow::parameterError(const QString &message, QWidget *field)
{
    clearError(); error_->setText(message); error_->show(); navigate(1);
    if (field) {
        invalidField_ = field; field->setProperty("invalid", true);
        field->style()->unpolish(field); field->style()->polish(field); field->setFocus();
    }
}
void MainWindow::changeEnvironment()
{
    clearError(); badge_->setText(isDemo() ? tr8("演示") : tr8("板端"));
    wave_->beginSession(++session_, isDemo(), false);
    if (isDemo() && input_->text().trimmed().isEmpty()) wave_->showDemo();
    else if (!input_->text().trimmed().isEmpty()) loadInput();
    setState(tr8("就绪")); updateBusy();
}
void MainWindow::browseDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr8("选择工作目录"), directory_->text());
    if (!path.isEmpty()) { directory_->setText(path); QSettings().setValue("arm/businessDirectory", path); }
}
void MainWindow::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(this, tr8("选择输入 WAV"), directory_->text(), "WAV (*.wav)");
    if (path.isEmpty()) return;
    mode_->setCurrentIndex(0); input_->setText(path); loadInput();
}
void MainWindow::loadInput()
{
    if (demo_.isBusy() || board_.isBusy() || closing_) return;
    const QString path = input_->text().trimmed();
    if (path.isEmpty()) { wave_->clearSource(0); return; }
    const QString absolute = absolutePath(path);
    if (wave_->hasFile(0) && wave_->filePath(0) == absolute) return;
    clearError(); wave_->openFile(0, absolute);
}
void MainWindow::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(this, tr8("选择保存位置"), directory_->text(), "WAV (*.wav)",
        0, QFileDialog::DontConfirmOverwrite);
    if (!path.isEmpty()) output_->setText(path);
}
void MainWindow::browseComparison()
{
    const QString path = QFileDialog::getOpenFileName(this, tr8("载入对比 WAV"), directory_->text(), "WAV (*.wav)");
    if (!path.isEmpty()) { clearError(); wave_->openFile(1, path); }
}
void MainWindow::restoreDemo()
{
    if (!isDemo() || demo_.isBusy() || board_.isBusy()) return;
    clearError(); input_->clear(); wave_->beginSession(++session_, true, false); wave_->showDemo();
}
RunRequest MainWindow::request() const
{
    RunRequest r;
    r.mode = mode(); r.algorithm = algorithm(); r.seconds = continuous_->isChecked() ? 0 : seconds_->value();
    r.inputWav = r.mode == "file" ? input_->text().trimmed() : QString();
    r.outputWav = (r.mode == "record" || saveOutput_->isChecked()) ? output_->text().trimmed() : QString();
    r.direction = single_->direction(); r.sourceA = sourceA_->direction(); r.sourceB = sourceB_->direction();
    return r;
}
void MainWindow::startAudio()
{
    clearError();
    RunRequest r = request();
    if (r.mode == "file" && !r.inputWav.isEmpty() &&
            (!wave_->hasFile(0) || wave_->filePath(0) != absolutePath(r.inputWav))) {
        loadInput(); parameterError(tr8("请等待输入 WAV 读取完成"), input_); return;
    }
    if (!isDemo() && r.mode == "file" && r.inputWav.isEmpty()) {
        parameterError(tr8("请选择输入 WAV"), input_); return;
    }
    if (!isDemo() && (r.mode == "record" || saveOutput_->isChecked()) && r.outputWav.isEmpty()) {
        parameterError(tr8("请选择保存位置"), output_); return;
    }
    if (r.mode != "record" && r.algorithm == "spatial" && !single_->isValid()) {
        parameterError(tr8("请选择声源方位角"), single_); return;
    }
    if (r.mode != "record" && r.algorithm == "dual" && (!sourceA_->isValid() || !sourceB_->isValid())) {
        parameterError(tr8("请选择 A / B 声源方位角"), sourceA_->isValid() ? sourceB_ : sourceA_); return;
    }
    const QString error = r.validate(!isDemo());
    if (!error.isEmpty()) { parameterError(error, mode_); return; }
    if (!isDemo() && !r.outputWav.isEmpty()) {
        const QFileInfo output(absolutePath(r.outputWav));
        const QFileInfo input(absolutePath(r.inputWav));
        if (r.mode == "file" && (output.absoluteFilePath() == input.absoluteFilePath() ||
                (output.exists() && output.canonicalFilePath() == input.canonicalFilePath()))) {
            parameterError(tr8("输入与输出不能是同一个文件"), output_); return;
        }
        if (output.exists() && QMessageBox::question(this, tr8("覆盖文件"), tr8("输出文件已存在，是否覆盖？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    }
    launch(r);
}
void MainWindow::launch(const RunRequest &r)
{
    if (backend()->isBusy() || ipc_.isBusy()) return;
    runningRequest_ = r; runningDirectory_ = directory_->text();
    const bool imported = wave_->hasFile(0) || wave_->hasFile(1);
    demo_.setFilePreview(imported, imported ? wave_->fileDuration() : 5.0);
    if (r.mode != "smoke") {
        const bool keep = r.mode == "file" && (isDemo() ? imported : wave_->hasFile(0));
        wave_->beginSession(++session_, isDemo(), keep);
        if (!isDemo()) wave_->clearSource(1);
    } else ++session_;
    if (backend()->start(runningDirectory_, r, session_)) {
        if (r.mode == "smoke")
            summary_->setText(tr8("链路自检  ·  ") + algorithmLabel(r.algorithm));
        if (r.mode != "smoke") navigate(0);
        QSettings().setValue("arm/businessDirectory", directory_->text());
    } else {
        error_->setText(tr8("启动失败，请查看诊断日志")); error_->show(); navigate(2);
    }
    updateBusy();
}
void MainWindow::stopAudio() { backend()->stop(); updateBusy(); }
void MainWindow::startSmoke()
{
    clearError(); RunRequest r; r.algorithm = smokeAlgorithm_->itemData(smokeAlgorithm_->currentIndex()).toString(); launch(r);
}
void MainWindow::startSync() { startIpc(false); }
void MainWindow::startData() { startIpc(true); }
void MainWindow::stopIpc() { ipc_.stop(); }
void MainWindow::startIpc(bool dataMode)
{
    const QString peer = QDir(QApplication::applicationDirPath()).filePath("ex04_ipc_peer");
    if (ipc_.start(dataMode, peer)) ipcStatus_->setText(tr8("IPC 验证中"));
    updateBusy();
}
void MainWindow::appendLog(const QString &text)
{
    const QByteArray bytes = (text.endsWith('\n') ? text : text + "\n").toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr); std::fflush(stderr);
    log_->moveCursor(QTextCursor::End); log_->insertPlainText(QString::fromUtf8(bytes.constData(), bytes.size()));
    log_->ensureCursorVisible();
}
void MainWindow::saveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, tr8("保存日志"), QDir::homePath() + "/ex04-qt.log", "Log (*.log)");
    if (path.isEmpty()) return;
    QFile file(path); const QByteArray bytes = log_->toPlainText().toUtf8();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) {
        error_->setText(tr8("日志保存失败：") + file.errorString()); error_->show();
    }
}
void MainWindow::setState(const QString &state)
{
    status_->setText(status_->fontMetrics().elidedText(state, Qt::ElideRight, 480));
    status_->setToolTip(state);
}
void MainWindow::pipelineFinished(bool success, const QString &summary)
{
    appendLog(summary); setState(summary);
    if (success && !isDemo() && !runningRequest_.outputWav.isEmpty()) {
        const QString path = QDir(runningDirectory_).absoluteFilePath(runningRequest_.outputWav);
        wave_->openFile(runningRequest_.mode == "record" ? 0 : 1, path);
    }
    updateBusy(); closeWhenIdle();
}
void MainWindow::ipcFinished(bool success, const QString &summary)
{
    ipcStatus_->setText(success ? tr8("IPC 通过") : tr8("IPC 已结束"));
    appendLog(summary); updateBusy(); closeWhenIdle();
}
void MainWindow::showMessage(unsigned int sequence, const QString &text)
{
    ipcStatus_->setText(QString("IPC %1 / 10").arg(sequence));
    appendLog(QString("IPC_RECEIVED sequence=%1 text=%2").arg(sequence).arg(text));
}
void MainWindow::waveformLoaded(int stream, const QString &path)
{
    appendLog((stream == 0 ? tr8("载入输入：") : tr8("载入对比：")) + path);
    clearError(); updateBusy();
}
void MainWindow::waveformError(int stream, const QString &error)
{
    appendLog(error);
    if (stream == 0) parameterError(error, input_);
    else { error_->setText(error); error_->show(); }
    updateBusy();
}
void MainWindow::updatePause(bool paused) { pause_->setText(paused ? tr8("继续观察") : tr8("暂停观察")); }
void MainWindow::closeWhenIdle()
{
    if (closing_ && !demo_.isBusy() && !board_.isBusy() && !ipc_.isBusy()) QTimer::singleShot(0, this, SLOT(close()));
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (demo_.isBusy() || board_.isBusy() || ipc_.isBusy()) {
        closing_ = true; demo_.stop(); board_.stop(); ipc_.stop(); updateBusy(); closeWhenIdle(); event->ignore(); return;
    }
    QSettings().setValue("arm/businessDirectory", directory_->text()); event->accept();
}
