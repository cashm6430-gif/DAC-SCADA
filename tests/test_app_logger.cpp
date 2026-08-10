#include <QtTest/QtTest>
#include "core/app_logger.h"

// Pure log-filter/rotation-name logic (no file I/O, no global message handler).
class TestAppLogger : public QObject
{
    Q_OBJECT

private slots:
    void backupNames();
    void levelMapping();
    void levelFilter();
};

void TestAppLogger::backupNames()
{
    QCOMPARE(AppLogger::backupFileName(QStringLiteral("app.log"), 1),
             QStringLiteral("app.log.1"));
    QCOMPARE(AppLogger::backupFileName(QStringLiteral("app.log"), 3),
             QStringLiteral("app.log.3"));
    QCOMPARE(AppLogger::backupFileName(QStringLiteral("C:/x/app.log"), 2),
             QStringLiteral("C:/x/app.log.2"));
}

void TestAppLogger::levelMapping()
{
    QCOMPARE(AppLogger::levelForType(QtDebugMsg),    AppLogger::Level::Debug);
    QCOMPARE(AppLogger::levelForType(QtInfoMsg),     AppLogger::Level::Info);
    QCOMPARE(AppLogger::levelForType(QtWarningMsg),  AppLogger::Level::Warning);
    QCOMPARE(AppLogger::levelForType(QtCriticalMsg), AppLogger::Level::Critical);
    QCOMPARE(AppLogger::levelForType(QtFatalMsg),    AppLogger::Level::Fatal);
}

void TestAppLogger::levelFilter()
{
    // minLevel = Info: Debug filtered, Info/Warning/Critical pass.
    QVERIFY(!AppLogger::passes(QtDebugMsg, AppLogger::Level::Info));
    QVERIFY(AppLogger::passes(QtInfoMsg, AppLogger::Level::Info));
    QVERIFY(AppLogger::passes(QtWarningMsg, AppLogger::Level::Info));
    QVERIFY(AppLogger::passes(QtCriticalMsg, AppLogger::Level::Info));

    // minLevel = Warning: Info now filtered.
    QVERIFY(!AppLogger::passes(QtInfoMsg, AppLogger::Level::Warning));
    QVERIFY(AppLogger::passes(QtWarningMsg, AppLogger::Level::Warning));

    // minLevel = Debug: everything passes.
    QVERIFY(AppLogger::passes(QtDebugMsg, AppLogger::Level::Debug));
    QVERIFY(AppLogger::passes(QtFatalMsg, AppLogger::Level::Debug));
}

QTEST_GUILESS_MAIN(TestAppLogger)
#include "test_app_logger.moc"
