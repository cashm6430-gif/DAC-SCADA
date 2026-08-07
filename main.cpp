#include <QApplication>
#include <QTimer>
#include <QString>
#include <QCoreApplication>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("DAC-SCADA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DAC-SCADA");

    MainWindow window;

    // 加载通道配置（可执行文件旁的 config，由 post-build 自动拷贝）
    window.loadConfiguration(QStringLiteral("config/devices.json"));

    window.show();

    // 命令行 --sim：自动启动模拟下位机，并稍后自动连接，便于演示/自动化测试
    if (app.arguments().contains(QStringLiteral("--sim"))) {
        window.startSimulatorAutomatically();
        QTimer::singleShot(500, &window, &MainWindow::connectToDevice);
    }

    // 命令行 --selftest：端到端自检，采集结果写入文件后退出
    if (app.arguments().contains(QStringLiteral("--selftest"))) {
        window.runSelfTest(QCoreApplication::applicationDirPath()
                           + QStringLiteral("/selftest_result.txt"));
    }

    return app.exec();
}
