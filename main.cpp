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

    // 命令行 --sim：自动启动内置模拟下位机（Modbus TCP @ 127.0.0.1:1502）
    if (app.arguments().contains(QStringLiteral("--sim")))
        window.startSimulatorAutomatically();

    return app.exec();
}
