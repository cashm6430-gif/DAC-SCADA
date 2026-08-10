#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <QString>
#include <QtGlobal>

/// File log with level filtering and size-based rotation.
///
/// Installs a process-wide Qt message handler that appends timestamped
/// "[LEVEL] message" lines to <logDir>/app.log, thread-safe across the worker
/// thread. When the log file reaches maxBytes it is rotated: app.log → .1 →
/// .2 → …, keeping maxBackups archived copies. Level filtering drops messages
/// below minLevel before they ever touch the file.
///
/// Pure helpers (backupFileName / levelForType / passes) carry no global state
/// and are unit-tested headlessly; the handler itself is installed once from
/// main().
class AppLogger
{
public:
    /// Log levels, ordered so comparisons with `>=` implement "at least this".
    enum class Level {
        Debug = 0,
        Info  = 1,
        Warning = 2,
        Critical = 3,
        Fatal = 4,
    };

    /// Install the global message handler. Creates \a logDir if needed.
    static void install(const QString &logDir,
                        Level minLevel = Level::Info,
                        qint64 maxBytes = 1024 * 1024,
                        int maxBackups = 3);

    /// Raise/lower the filtering threshold at runtime (thread-safe).
    static void setMinLevel(Level level);
    static Level minLevel();

    // ---- pure logic (no global state — unit-tested) ----
    /// Qt message type → log level.
    static Level levelForType(int qtMsgType);
    /// Would a message of \a qtMsgType be recorded at \a minLevel?
    static bool passes(int qtMsgType, Level minLevel);
    /// Name of the \a index-th rotation archive, e.g. "app.log.1" (1 = newest).
    static QString backupFileName(const QString &base, int index);
};

#endif // APP_LOGGER_H
