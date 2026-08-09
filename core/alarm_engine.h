#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QDateTime>
#include "types.h"

/// Monitors channel values against their upper/lower limits and emits
/// alarm events when a channel crosses a threshold (and when it recovers).
/// State is tracked per (deviceIndex, regAddr).
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    explicit AlarmEngine(QObject *parent = nullptr);

    /// Evaluate a freshly-sampled value from a device/channel.
    void checkValue(int deviceIndex, const Channel &ch, double value);

    const QVector<AlarmRecord> &history() const { return m_history; }

signals:
    void newAlarm(const AlarmRecord &record);

private:
    enum class Status { Normal, High, Low };

    void raise(int deviceIndex, const QString &message,
               AlarmRecord::Severity severity);

    QHash<int, QHash<int, Status>> m_states;  // deviceIndex → (regAddr → status)
    QVector<AlarmRecord> m_history;

    /// Keep the history bounded so a long-running process doesn't grow
    /// without limit (oldest records are dropped beyond this count).
    static constexpr int kMaxHistory = 1000;
};

#endif // ALARM_ENGINE_H
