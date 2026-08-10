#include <QtTest/QtTest>
#include <memory>
#include "core/alarm_engine.h"
#include "core/types.h"

// AlarmEngine threshold-crossing + anti-chatter (debounce) semantics.
//
// NOTE: QtTest reuses ONE instance of the test class across all test
// functions, so member state and signal connections would leak between tests.
// init() therefore recreates the engine (fresh state machine) and installs a
// single connection — every test starts deterministic.
class TestAlarmEngine : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<AlarmEngine> m_engine;
    QVector<AlarmRecord> m_alarms;
    Channel m_ch;   // defaults: upperLimit=100, lowerLimit=0

private slots:
    void init();
    void normalValueNoAlarm();
    void highAfterDebounce();
    void flickerDoesNotAlarm();
    void recoversAfterDebounce();
    void lowIsWarning();
    void recoverDoesNotEmitExtraWhenNeverRaised();
};

void TestAlarmEngine::init()
{
    m_alarms.clear();
    m_engine = std::make_unique<AlarmEngine>();   // resets the state machine
    connect(m_engine.get(), &AlarmEngine::newAlarm, this,
            [this](const AlarmRecord &r) { m_alarms.append(r); });
}

void TestAlarmEngine::normalValueNoAlarm()
{
    for (int i = 0; i < 5; ++i)
        m_engine->checkValue(0, m_ch, 50.0);
    QCOMPARE(m_alarms.size(), 0);
}

void TestAlarmEngine::highAfterDebounce()
{
    // 3 consecutive samples above the upper limit → one Critical alarm.
    for (int i = 0; i < 3; ++i)
        m_engine->checkValue(0, m_ch, 120.0);

    QCOMPARE(m_alarms.size(), 1);
    QCOMPARE(m_alarms.at(0).severity, AlarmRecord::Critical);
    QVERIFY(m_alarms.at(0).message.contains("超上限"));
    QCOMPARE(m_alarms.at(0).deviceIndex, static_cast<qint16>(0));

    // Staying high does not re-alarm.
    m_engine->checkValue(0, m_ch, 130.0);
    QCOMPARE(m_alarms.size(), 1);
}

void TestAlarmEngine::flickerDoesNotAlarm()
{
    // A single out-of-range sample that immediately returns does NOT debounce
    // into an alarm: the consecutive counter is reset when the value flips back.
    m_engine->checkValue(0, m_ch, 120.0);   // 1 high
    m_engine->checkValue(0, m_ch, 50.0);    // back to normal (resets)
    m_engine->checkValue(0, m_ch, 120.0);   // 1 high
    m_engine->checkValue(0, m_ch, 120.0);   // 2 high — still below threshold
    QCOMPARE(m_alarms.size(), 0);
}

void TestAlarmEngine::recoversAfterDebounce()
{
    for (int i = 0; i < 3; ++i)
        m_engine->checkValue(0, m_ch, 120.0);   // High alarm
    for (int i = 0; i < 3; ++i)
        m_engine->checkValue(0, m_ch, 50.0);    // debounced recovery

    QCOMPARE(m_alarms.size(), 2);
    QCOMPARE(m_alarms.at(0).severity, AlarmRecord::Critical);
    QCOMPARE(m_alarms.at(1).severity, AlarmRecord::Info);
    QVERIFY(m_alarms.at(1).message.contains("恢复正常"));
}

void TestAlarmEngine::lowIsWarning()
{
    for (int i = 0; i < 3; ++i)
        m_engine->checkValue(0, m_ch, -5.0);
    QCOMPARE(m_alarms.size(), 1);
    QCOMPARE(m_alarms.at(0).severity, AlarmRecord::Warning);
    QVERIFY(m_alarms.at(0).message.contains("低于下限"));
}

void TestAlarmEngine::recoverDoesNotEmitExtraWhenNeverRaised()
{
    // Value starts in-range, so there is no committed alarm to recover from —
    // no spurious "恢复正常" alarm.
    m_engine->checkValue(0, m_ch, 50.0);
    for (int i = 0; i < 5; ++i)
        m_engine->checkValue(0, m_ch, 50.0);
    QCOMPARE(m_alarms.size(), 0);
}

QTEST_GUILESS_MAIN(TestAlarmEngine)
#include "test_alarm_engine.moc"
