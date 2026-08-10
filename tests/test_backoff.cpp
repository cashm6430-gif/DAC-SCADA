#include <QtTest/QtTest>
#include "core/backoff.h"

// Unit tests for the pure reconnect-backoff math (no QObject, no event loop).
class TestBackoff : public QObject
{
    Q_OBJECT

private slots:
    void zeroRetriesIsBase();
    void doublesEachRetry();
    void capsAtMax();
    void clampsExtremes();
};

void TestBackoff::zeroRetriesIsBase()
{
    QCOMPARE(reconnectBackoffMs(0, 500, 30000), qint64(500));
}

void TestBackoff::doublesEachRetry()
{
    QCOMPARE(reconnectBackoffMs(1, 500, 30000), qint64(1000));
    QCOMPARE(reconnectBackoffMs(2, 500, 30000), qint64(2000));
    QCOMPARE(reconnectBackoffMs(3, 500, 30000), qint64(4000));
    QCOMPARE(reconnectBackoffMs(4, 500, 30000), qint64(8000));
}

void TestBackoff::capsAtMax()
{
    // 500<<6 = 32000 > 30000 → clamped to the cap.
    QCOMPARE(reconnectBackoffMs(6, 500, 30000), qint64(30000));
    QCOMPARE(reconnectBackoffMs(12, 500, 30000), qint64(30000));
}

void TestBackoff::clampsExtremes()
{
    // Negative retry counts behave like zero (no underflow of the shift).
    QCOMPARE(reconnectBackoffMs(-3, 500, 30000), qint64(500));
    // A huge retry count cannot overflow: capped at 30 s.
    QCOMPARE(reconnectBackoffMs(1000, 500, 30000), qint64(30000));
}

QTEST_GUILESS_MAIN(TestBackoff)
#include "test_backoff.moc"
