#include "../include/widget.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>

int main(int argc, char *argv[])
{

    SetConsoleOutputCP(CP_UTF8);
    QApplication a(argc, argv);
    QString iconPath = QFile::exists("pics/logo.ico") ? "pics/logo.ico" : "bin/pics/logo.ico";
    a.setWindowIcon(QIcon(iconPath));

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "test_move_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    Widget w;
    w.setWindowTitle("强化模式偏置调整V0.4.3");
    w.show();
    return a.exec();
}
