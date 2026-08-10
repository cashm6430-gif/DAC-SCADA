#include "app_logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QMessageLogContext>
#include <cstdlib>

namespace {

QFile g_logFile;
QMutex g_mutex;                        // serializes writes across threads
AppLogger::Level g_minLevel = AppLogger::Level::Info;
qint64 g_maxBytes   = 1024 * 1024;
int    g_maxBackups = 3;

QString levelName(AppLogger::Level level)
{
    switch (level) {
    case AppLogger::Level::Debug:    return QStringLiteral("DEBUG");
    case AppLogger::Level::Info:     return QStringLiteral("INFO");
    case AppLogger::Level::Warning:  return QStringLiteral("WARN");
    case AppLogger::Level::Critical: return QStringLiteral("CRIT");
    case AppLogger::Level::Fatal:    return QStringLiteral("FATAL");
    }
    return QStringLiteral("?");
}

/// Rotate the current log: drop the oldest archive (.N), shift .(N-1)→.N … .1→.2,
/// move the current file to .1, and leave it closed for a fresh reopen.
/// Caller must hold g_mutex.
void rotate()
{
    const QString base = g_logFile.fileName();
    QFile::remove(AppLogger::backupFileName(base, g_maxBackups));   // 丢弃最旧
    for (int i = g_maxBackups - 1; i >= 1; --i) {
        const QString src = AppLogger::backupFileName(base, i);
        if (QFile::exists(src))
            QFile::rename(src, AppLogger::backupFileName(base, i + 1));
    }
    g_logFile.close();
    QFile::rename(base, AppLogger::backupFileName(base, 1));
}

void handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    if (!AppLogger::passes(static_cast<int>(type), g_minLevel))
        return;

    QMutexLocker lock(&g_mutex);

    if (!g_logFile.isOpen()) {
        QDir().mkpath(QFileInfo(g_logFile.fileName()).absolutePath());
        g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    }

    if (g_logFile.isOpen()) {
        if (g_logFile.size() >= g_maxBytes) {
            rotate();                          // 关闭并归档
            g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);   // 重建新文件
        }
        if (g_logFile.isOpen()) {
            QTextStream ts(&g_logFile);
            ts << QDateTime::currentDateTime().toString(
                      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
               << QLatin1Char(' ') << '['
               << levelName(AppLogger::levelForType(static_cast<int>(type)))
               << QLatin1String("] ") << msg << '\n';
            ts.flush();
        }
    }

    // Preserve Qt's fatal behavior (abort after logging).
    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void AppLogger::install(const QString &logDir, Level minLevel,
                        qint64 maxBytes, int maxBackups)
{
    g_minLevel   = minLevel;
    g_maxBytes   = qMax<qint64>(maxBytes, 1024);
    g_maxBackups = qBound(1, maxBackups, 16);
    g_logFile.setFileName(logDir + QStringLiteral("/app.log"));
    qInstallMessageHandler(&handler);
}

void AppLogger::setMinLevel(Level level) { g_minLevel = level; }
AppLogger::Level AppLogger::minLevel() { return g_minLevel; }

AppLogger::Level AppLogger::levelForType(int qtMsgType)
{
    switch (static_cast<QtMsgType>(qtMsgType)) {
    case QtDebugMsg:    return Level::Debug;
    case QtInfoMsg:     return Level::Info;
    case QtWarningMsg:  return Level::Warning;
    case QtCriticalMsg: return Level::Critical;
    case QtFatalMsg:    return Level::Fatal;
    }
    return Level::Info;
}

bool AppLogger::passes(int qtMsgType, Level minLevel)
{
    return static_cast<int>(levelForType(qtMsgType)) >= static_cast<int>(minLevel);
}

QString AppLogger::backupFileName(const QString &base, int index)
{
    return base + QLatin1Char('.') + QString::number(index);
}
