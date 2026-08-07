#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QDateTime>
#include "types.h"

/// Monitors channel values against their upper/lower limits and emits
/// alarm events when a channel crosses a threshold (and when it recovers).
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    explicit AlarmEngine(QObject *parent = nullptr);

    /// Evaluate a freshly-sampled channel value. Emits newAlarm() when the
    /// state transitions (normal → high/low, or high/low → normal).
    void checkValue(const Channel &ch, double value);

    const QVector<AlarmRecord> &history() const { return m_history; }

signals:
    void newAlarm(const AlarmRecord &record);

private:
    enum class Status { Normal, High, Low };

    void raise(const Channel &ch, const QString &message,
               AlarmRecord::Severity severity);

    QHash<int, Status> m_states;   // regAddr → last status
    QVector<AlarmRecord> m_history;
};

#endif // ALARM_ENGINE_H
