#include <QApplication>
#include <QStringList>
#include <QFile>
#include <QFontDatabase>
#include <QStyleFactory>
#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif
#include "MainWindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("ex04_frontend_arm");
    app.setOrganizationName("ex04");
#if QT_VERSION >= 0x050000
    app.setStyle(QStyleFactory::create("Fusion"));
#endif
    QFontDatabase fonts;
    const QStringList preferred = QStringList() << "WenQuanYi Micro Hei" << "Noto Sans CJK SC" << "Microsoft YaHei";
    QFont font = app.font();
    foreach (const QString &family, preferred) {
        if (fonts.families().contains(family)) { font.setFamily(family); break; }
    }
    font.setPixelSize(14);
    app.setFont(font);
    QFile theme(":/ui/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(theme.readAll()));
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif
    MainWindow window;
    if (app.arguments().contains("--windowed")) {
        window.resize(800, 480);
        window.show();
    } else
        window.showFullScreen();
    return app.exec();
}
