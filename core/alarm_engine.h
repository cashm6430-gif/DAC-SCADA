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
///
/// Anti-chatter: a threshold crossing is only committed after the new state is
/// observed for kDebounceCount consecutive samples (at 10 Hz poll that is a
/// ~300 ms debounce), so a noisy signal hugging the limit does not spam
/// High→Normal→High alarm pairs.
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

    /// Per-register anti-chatter state.
    struct TrackedState {
        Status committed = Status::Normal;   // the state currently reported
        Status pending   = Status::Normal;   // candidate awaiting debounce
        int    consecutive = 0;              // consecutive samples in pending
    };

    QHash<int, QHash<int, TrackedState>> m_states;  // deviceIndex → (regAddr → state)

    /// Consecutive samples in a state before its transition is committed.
    static constexpr int kDebounceCount = 3;
};

#endif // ALARM_ENGINE_H
