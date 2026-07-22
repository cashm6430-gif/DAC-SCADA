#include "alarm_engine.h"

AlarmEngine::AlarmEngine(QObject *parent)
    : QObject(parent) {}

const QVector<AlarmRecord> &AlarmEngine::activeAlarms() const { return m_active; }
const QVector<AlarmRecord> &AlarmEngine::history() const { return m_history; }

void AlarmEngine::onAlarmTriggered(int deviceAddr, const QString &message)
{
    AlarmRecord rec;
    rec.timestamp  = QDateTime::currentDateTime();
    rec.deviceAddr = deviceAddr;
    rec.message    = message;
    rec.severity   = AlarmRecord::Warning;

    m_active.append(rec);
    m_history.append(rec);
    emit newAlarm(rec);
}

void AlarmEngine::acknowledgeAlarm(int index)
{
    if (index < 0 || index >= m_active.size())
        return;
    m_active[index].acknowledged = true;
    emit alarmAcknowledged(index);
    m_active.removeAt(index);
}
