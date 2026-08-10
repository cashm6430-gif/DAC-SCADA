#include <QtTest/QtTest>
#include "core/sample_queue.h"

// Tests the bounded producer-consumer FIFO (SampleQueue) semantics.
class TestSampleQueue : public QObject
{
    Q_OBJECT

private slots:
    void emptyInitially();
    void pushPopRoundTrip();
    void boundedDropsOldest();
    void sizeReflectsItems();
    void tryPopOnEmptyReturnsFalse();
};

void TestSampleQueue::emptyInitially()
{
    SampleQueue q;
    QCOMPARE(q.size(), 0);

    Sample s;
    QVERIFY(!q.tryPop(s));
}

void TestSampleQueue::pushPopRoundTrip()
{
    SampleQueue q;
    for (int i = 0; i < 5; ++i) {
        Sample s;
        s.deviceIndex = 0;
        s.regAddr     = i;
        s.value       = i * 10.0;
        q.push(s);
    }

    Sample out;
    int seen = 0;
    while (q.tryPop(out)) {
        QCOMPARE(out.regAddr, seen);
        QCOMPARE(out.value, seen * 10.0);
        ++seen;
    }
    QCOMPARE(seen, 5);
    QCOMPARE(q.size(), 0);
}

void TestSampleQueue::boundedDropsOldest()
{
    SampleQueue q(3);   // tiny capacity
    for (int i = 0; i < 10; ++i) {
        Sample s;
        s.deviceIndex = i;   // tag with push order
        q.push(s);
    }

    // Only the 3 newest survive (7, 8, 9); 0..6 were dropped.
    QCOMPARE(q.size(), 3);
    Sample s;
    QVERIFY(q.tryPop(s));
    QCOMPARE(s.deviceIndex, 7);
    QVERIFY(q.tryPop(s));
    QCOMPARE(s.deviceIndex, 8);
    QVERIFY(q.tryPop(s));
    QCOMPARE(s.deviceIndex, 9);
    QVERIFY(!q.tryPop(s));
}

void TestSampleQueue::sizeReflectsItems()
{
    SampleQueue q;
    Sample s;
    for (int i = 0; i < 4; ++i) {
        q.push(s);
        QCOMPARE(q.size(), i + 1);
    }
    QVERIFY(q.tryPop(s));
    QCOMPARE(q.size(), 3);
}

void TestSampleQueue::tryPopOnEmptyReturnsFalse()
{
    SampleQueue q;
    Sample s;
    QVERIFY(!q.tryPop(s));
    QVERIFY(!q.tryPop(s));   // still empty after a failed pop
}

QTEST_GUILESS_MAIN(TestSampleQueue)
#include "test_sample_queue.moc"
