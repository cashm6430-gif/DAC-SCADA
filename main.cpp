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

    // 命令行 --sim：自动启动模拟下位机，并稍后自动连接，便于演示/自动化测试。
    // 各自检模式会在内部自行连接，故此处跳过，避免重复 connectAll()。
    const bool selftest          = app.arguments().contains(QStringLiteral("--selftest"));
    const bool selftestReconnect = app.arguments().contains(QStringLiteral("--selftest-reconnect"));

    if (app.arguments().contains(QStringLiteral("--sim"))
        && !selftest && !selftestReconnect) {
        window.startSimulatorAutomatically();
        QTimer::singleShot(500, &window, &MainWindow::connectToDevice);
    }

    // 命令行 --selftest：端到端自检，采集结果写入文件后退出
    if (selftest) {
        window.runSelfTest(QCoreApplication::applicationDirPath()
                           + QStringLiteral("/selftest_result.txt"));
    }
    // 命令行 --selftest-reconnect：断线自动重连自检（停/启 TCP 模拟器）
    else if (selftestReconnect) {
        window.runSelfTestReconnect(QCoreApplication::applicationDirPath()
                                    + QStringLiteral("/reconnect_result.txt"));
    }

    return app.exec();
}
