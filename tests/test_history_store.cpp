#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/history_store.h"
#include "core/types.h"

// HistoryStore write→flush→query round-trip against a real (temp-file) SQLite
// database. No threads involved: everything runs on the test thread, so the
// query signals fire synchronously and can be captured by reference.
class TestHistoryStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString dbPath() const { return m_dir.filePath("test.db"); }

private slots:
    void init();
    void samplesRoundTrip();
    void samplesFilterByRegAddr();
    void alarmsRoundTrip();
    void queryBeforeAnyWriteReturnsEmpty();
};

void TestHistoryStore::init()
{
    // QtTest reuses one instance across test functions; each test starts with
    // a freshly removed database file so no rows leak between tests.
    QFile::remove(dbPath());
}

void TestHistoryStore::samplesRoundTrip()
{
    HistoryStore store(dbPath());
    store.start();   // harmless — the test flushes manually
    store.addSample(0, 10, 1000, 12.5);
    store.addSample(0, 11, 1001, 13.5);
    store.addSample(1, 10, 2000, 99.0);
    QVERIFY(store.flush());

    HistoryResult res;
    connect(&store, &HistoryStore::samplesReady, this,
            [&res](int, int, const HistoryResult &r) { res = r; });

    HistoryQuery q;
    q.startMs = 0;
    q.endMs   = 999999999;
    store.querySamples(7, 0, q);

    QCOMPARE(res.requestId, 7);
    QCOMPARE(res.deviceIndex, 0);
    QCOMPARE(res.rows.size(), 2);                 // device 0 only
    QCOMPARE(res.rows.at(0).regAddr, 10);         // ORDER BY ts ASC
    QCOMPARE(res.rows.at(0).value, 12.5);
    QCOMPARE(res.rows.at(1).regAddr, 11);
    QCOMPARE(res.rows.at(1).tsMs, qint64(1001));
}

void TestHistoryStore::samplesFilterByRegAddr()
{
    HistoryStore store(dbPath());
    store.addSample(0, 10, 1000, 12.5);
    store.addSample(0, 11, 1001, 13.5);
    QVERIFY(store.flush());

    HistoryResult res;
    connect(&store, &HistoryStore::samplesReady, this,
            [&res](int, int, const HistoryResult &r) { res = r; });

    HistoryQuery q;
    q.regAddrs = QVector<int>{11};
    q.startMs  = 0;
    q.endMs    = 999999999;
    store.querySamples(8, 0, q);

    QCOMPARE(res.rows.size(), 1);
    QCOMPARE(res.rows.at(0).regAddr, 11);
    QCOMPARE(res.rows.at(0).value, 13.5);
}

void TestHistoryStore::alarmsRoundTrip()
{
    HistoryStore store(dbPath());
    AlarmRecord a;
    a.timestamp   = QDateTime::fromMSecsSinceEpoch(5000);
    a.deviceIndex = 2;
    a.severity    = AlarmRecord::Critical;
    a.message     = QStringLiteral("温度超上限");
    store.addAlarm(a);
    QVERIFY(store.flush());

    QVector<AlarmRecord> got;
    connect(&store, &HistoryStore::alarmsReady, this,
            [&got](int, int, const QVector<AlarmRecord> &v) { got = v; });

    store.queryAlarms(3, 2, 0, 999999999);

    QCOMPARE(got.size(), 1);
    QCOMPARE(got.at(0).deviceIndex, static_cast<qint16>(2));
    QCOMPARE(got.at(0).severity, AlarmRecord::Critical);
    QCOMPARE(got.at(0).message, QStringLiteral("温度超上限"));
    QCOMPARE(got.at(0).timestamp.toMSecsSinceEpoch(), qint64(5000));
}

void TestHistoryStore::queryBeforeAnyWriteReturnsEmpty()
{
    // A fresh store with no rows must answer a query with an empty result
    // (not an error) — the GUI treats empty as "no history yet".
    HistoryStore store(dbPath());

    HistoryResult res;
    connect(&store, &HistoryStore::samplesReady, this,
            [&res](int, int, const HistoryResult &r) { res = r; });

    HistoryQuery q;
    q.startMs = 0;
    q.endMs   = 100;
    store.querySamples(1, 0, q);

    QCOMPARE(res.rows.size(), 0);
    QCOMPARE(res.requestId, 1);
}

QTEST_GUILESS_MAIN(TestHistoryStore)
#include "test_history_store.moc"
