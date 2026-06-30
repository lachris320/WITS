#include "mainwindow.h"
#include "theme.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    int exitCode;
    do {
        QApplication a(argc, argv);
        a.setPalette(WitsTheme::lightPalette());
        a.setStyleSheet(WitsTheme::loadStyleSheet());
        MainWindow w;
        w.show();
        exitCode = a.exec();
    } while (exitCode == 1000); // Restart if exit code 1000

    return exitCode;
}
