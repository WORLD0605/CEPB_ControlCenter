#include <QApplication>
#include "ui/mainwindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("CEPB_ControlCenter");
    app.setApplicationVersion("0.2.0");

    MainWindow w;
    w.show();

    return app.exec();
}
