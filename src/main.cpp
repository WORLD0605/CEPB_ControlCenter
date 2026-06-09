#include <QApplication>
#include <QIcon>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CEPB_ControlCenter");
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/cepb_control_center_icon.png")));

    MainWindow w;
    w.show();

    return app.exec();
}
