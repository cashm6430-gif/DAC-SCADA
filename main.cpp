#include <QApplication>
#include <QTimer>
#include <QString>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QMessageLogContext>
#include <cstdlib>
#include "ui/mainwindow.h"

namespace {

// ---------------------------------------------------------------------------
// File log handler — with WIN32_EXECUTABLE there is no console, so qInfo/
// qWarning/qCritical would otherwise be invisible. Everything is appended to
// <exe>/data/app.log (same directory as the history database), timestamped and
// serialized across threads (collector runs on a worker thread).
// ---------------------------------------------------------------------------

QFile g_logFile;
QMutex g_logMutex;

void messageHandler(QtMsgType type, const QMessageLogContext &ctx,
                    const QString &msg)
{
    Q_UNUSED(ctx)
    QMutexLocker lock(&g_logMutex);

    if (!g_logFile.isOpen()) {
        const QString dir =
            QCoreApplication::applicationDirPath() + QStringLiteral("/data");
        QDir().mkpath(dir);
        g_logFile.setFileName(dir + QStringLiteral("/app.log"));
        g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    }

    if (g_logFile.isOpen()) {
        QString level;
        switch (type) {
        case QtDebugMsg:    level = QStringLiteral("DEBUG"); break;
        case QtInfoMsg:     level = QStringLiteral("INFO");  break;
        case QtWarningMsg:  level = QStringLiteral("WARN");  break;
        case QtCriticalMsg: level = QStringLiteral("CRIT");  break;
        case QtFatalMsg:    level = QStringLiteral("FATAL"); break;
        }

        QTextStream ts(&g_logFile);
        ts << QDateTime::currentDateTime().toString(
                  QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
           << QLatin1Char(' ') << '[' << level << "] " << msg << '\n';
        ts.flush();
    }

    // Preserve Qt's fatal behavior (abort after logging).
    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("DAC-SCADA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DAC-SCADA");

    qInstallMessageHandler(messageHandler);

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
