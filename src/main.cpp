#include <QApplication>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CEPB_ControlCenter");
    app.setApplicationVersion("0.2.0");

    MainWindow w;
    w.show();

    return app.exec();
}
