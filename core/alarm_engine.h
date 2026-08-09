#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <QObject>
#include <QHash>
#include <QDateTime>
#include "types.h"

/// Monitors channel values against their upper/lower limits and emits
/// alarm events when a channel crosses a threshold (and when it recovers).
///
/// Runs on the acquisition (worker) thread together with DataCollector; the
/// GUI thread keeps its own bounded alarm history by appending to it when
/// newAlarm is delivered (queued connection), so the history vector here never
/// needs cross-thread reads.
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    explicit AlarmEngine(QObject *parent = nullptr);

    /// Evaluate a freshly-sampled value from a device/channel.
    void checkValue(int deviceIndex, const Channel &ch, double value);

signals:
    void newAlarm(const AlarmRecord &record);

private:
    enum class Status { Normal, High, Low };

    void raise(int deviceIndex, const QString &message,
               AlarmRecord::Severity severity);

    QHash<int, QHash<int, Status>> m_states;  // deviceIndex → (regAddr → status)
};

#endif // ALARM_ENGINE_H
