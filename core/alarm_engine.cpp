#include "alarm_engine.h"
#include <QDebug>

AlarmEngine::AlarmEngine(QObject *parent)
    : QObject(parent)
{}

void AlarmEngine::checkValue(int deviceIndex, const Channel &ch, double value)
{
    auto &devStates = m_states[deviceIndex];
    TrackedState &st = devStates[ch.regAddr];

    Status candidate;
    if (value > ch.upperLimit) {
        candidate = Status::High;
    } else if (value < ch.lowerLimit) {
        candidate = Status::Low;
    } else {
        candidate = Status::Normal;
    }

    // Already committed to this state → nothing to do.
    if (candidate == st.committed) {
        st.pending = candidate;
        st.consecutive = 0;
        return;
    }

    // A new candidate differs from the committed state. Require it to hold for
    // kDebounceCount consecutive samples before committing, so a value that
    // just flicks across the limit doesn't raise then immediately recover.
    if (candidate != st.pending)
        st.consecutive = 0;   // direction changed → restart the debounce
    st.pending = candidate;
    ++st.consecutive;

    if (st.consecutive < kDebounceCount)
        return;

    st.committed = candidate;
    st.consecutive = 0;

    switch (candidate) {
    case Status::High:
        raise(deviceIndex,
              QStringLiteral("%1 超上限 (%2 %3)")
                  .arg(ch.name).arg(value, 0, 'f', 2).arg(ch.unit),
              AlarmRecord::Critical);
        break;
    case Status::Low:
        raise(deviceIndex,
              QStringLiteral("%1 低于下限 (%2 %3)")
                  .arg(ch.name).arg(value, 0, 'f', 2).arg(ch.unit),
              AlarmRecord::Warning);
        break;
    case Status::Normal:
        raise(deviceIndex, QStringLiteral("%1 恢复正常").arg(ch.name),
              AlarmRecord::Info);
        break;
    }
}

void AlarmEngine::raise(int deviceIndex, const QString &message,
                        AlarmRecord::Severity severity)
{
    AlarmRecord rec;
    rec.timestamp   = QDateTime::currentDateTime();
    rec.deviceIndex = static_cast<qint16>(deviceIndex);
    rec.message     = message;
    rec.severity    = severity;
    rec.acknowledged = false;

    emit newAlarm(rec);

    qInfo() << "[ALARM]" << rec.timestamp.toString("hh:mm:ss.zzz") << message;
}
