#include <QApplication>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("DAC-SCADA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DAC-SCADA");

    MainWindow window;
    window.show();

    return app.exec();
}
