#ifndef EX04_ARM_MAIN_WINDOW_H
#define EX04_ARM_MAIN_WINDOW_H
#include <QMainWindow>
#include "AudioBackend.h"
#include "IpcSession.h"
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class DirectionPicker;
class WaveformWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
protected:
    void closeEvent(QCloseEvent *event);
private slots:
    void navigate(int page);
    void updateMode();
    void updateFields();
    void updateBusy();
    void changeEnvironment();
    void browseDirectory();
    void browseInput();
    void browseOutput();
    void browseComparison();
    void loadInput();
    void restoreDemo();
    void startAudio();
    void stopAudio();
    void startSmoke();
    void startSync();
    void startData();
    void stopIpc();
    void appendLog(const QString &text);
    void saveLog();
    void setState(const QString &state);
    void pipelineFinished(bool success, const QString &summary);
    void ipcFinished(bool success, const QString &summary);
    void showMessage(unsigned int sequence, const QString &text);
    void waveformLoaded(int stream, const QString &path);
    void waveformError(int stream, const QString &error);
    void updatePause(bool paused);
private:
    QWidget *buildWavePage();
    QWidget *buildParameterPage();
    QWidget *buildDiagnosticPage();
    QWidget *buildSettingsPage();
    AudioBackend *backend();
    bool isDemo() const;
    QString mode() const;
    QString algorithm() const;
    QString absolutePath(const QString &path) const;
    RunRequest request() const;
    void launch(const RunRequest &request);
    void parameterError(const QString &message, QWidget *field);
    void clearError();
    void startIpc(bool dataMode);
    void closeWhenIdle();
    DemoBackend demo_;
    BoardBackend board_;
    IpcSession ipc_;
    QStackedWidget *pages_;
    QButtonGroup *navigation_;
    WaveformWidget *wave_;
    QComboBox *mode_, *algorithm_, *environment_, *smokeAlgorithm_;
    QLineEdit *directory_, *input_, *output_;
    QSpinBox *seconds_;
    QCheckBox *continuous_, *saveOutput_;
    DirectionPicker *single_, *sourceA_, *sourceB_;
    QWidget *inputRow_, *outputRow_, *durationRow_, *directions_, *parameterPage_, *settingsPage_;
    QPushButton *start_, *stop_, *pause_, *importInput_, *importOutput_, *demoReset_;
    QPushButton *sync_, *data_, *ipcStop_, *smoke_;
    QLabel *status_, *summary_, *badge_, *error_, *ipcStatus_;
    QPlainTextEdit *log_;
    QWidget *invalidField_;
    quint64 session_;
    RunRequest runningRequest_;
    QString runningDirectory_;
    bool closing_;
};
#endif
