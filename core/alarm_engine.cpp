#include "alarm_engine.h"
#include <QDebug>

AlarmEngine::AlarmEngine(QObject *parent)
    : QObject(parent)
{}

void AlarmEngine::checkValue(int deviceIndex, const Channel &ch, double value)
{
    auto &devStates = m_states[deviceIndex];
    const auto it = devStates.constFind(ch.regAddr);
    const Status prev = (it != devStates.constEnd()) ? it.value() : Status::Normal;

    Status now;
    if (value > ch.upperLimit) {
        now = Status::High;
    } else if (value < ch.lowerLimit) {
        now = Status::Low;
    } else {
        now = Status::Normal;
    }

    if (now == prev)
        return;

    devStates[ch.regAddr] = now;

    switch (now) {
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
    rec.timestamp  = QDateTime::currentDateTime();
    rec.deviceAddr = static_cast<qint16>(deviceIndex);
    rec.message    = message;
    rec.severity   = severity;
    rec.acknowledged = false;

    emit newAlarm(rec);

    qInfo() << "[ALARM]" << rec.timestamp.toString("hh:mm:ss.zzz") << message;
}
