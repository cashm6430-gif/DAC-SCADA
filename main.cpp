#include <QApplication>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include "core/app_logger.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("DAC-SCADA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DAC-SCADA");

    // 文件日志：分级过滤 + 按大小轮转（默认 INFO，1 MB × 3 份归档）。
    AppLogger::install(QCoreApplication::applicationDirPath()
                       + QStringLiteral("/data"));

    // 命令行 --log-level=debug|info|warning|critical：覆盖日志过滤级别。
    for (const QString &arg : app.arguments()) {
        if (arg.startsWith(QLatin1String("--log-level="))) {
            const QString level = arg.mid(12).toLower();
            if (level == QLatin1String("debug"))
                AppLogger::setMinLevel(AppLogger::Level::Debug);
            else if (level == QLatin1String("info"))
                AppLogger::setMinLevel(AppLogger::Level::Info);
            else if (level == QLatin1String("warning"))
                AppLogger::setMinLevel(AppLogger::Level::Warning);
            else if (level == QLatin1String("critical"))
                AppLogger::setMinLevel(AppLogger::Level::Critical);
            else
                qWarning() << "未知 --log-level:" << arg;
            break;
        }
    }

    MainWindow window;

    // 加载通道配置（可执行文件旁的 config，由 post-build 自动拷贝）
    window.loadConfiguration(QStringLiteral("config/devices.json"));

    window.show();

    // 命令行 --sim：自动启动模拟下位机，并稍后自动连接，便于演示/自动化测试。
    // 各自检模式会在内部自行连接，故此处跳过，避免重复 connectAll()。
    const bool selftest          = app.arguments().contains(QStringLiteral("--selftest"));
    const bool selftestReconnect = app.arguments().contains(QStringLiteral("--selftest-reconnect"));
    const bool selftestHistory   = app.arguments().contains(QStringLiteral("--selftest-history"));
    const bool selftestWrite     = app.arguments().contains(QStringLiteral("--selftest-write"));

    if (app.arguments().contains(QStringLiteral("--sim"))
        && !selftest && !selftestReconnect && !selftestHistory && !selftestWrite) {
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
    // 命令行 --selftest-history：历史读路径自检（采集→查询→断言 rows>0）
    else if (selftestHistory) {
        window.runSelfTestHistory(QCoreApplication::applicationDirPath()
                                  + QStringLiteral("/history_result.txt"));
    }
    // 命令行 --selftest-write：遥控写寄存器自检（写入→断言值保持）
    else if (selftestWrite) {
        window.runSelfTestWrite(QCoreApplication::applicationDirPath()
                                + QStringLiteral("/write_result.txt"));
    }

    return app.exec();
}
