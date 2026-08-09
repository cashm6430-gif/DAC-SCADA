#ifndef SAMPLE_QUEUE_H
#define SAMPLE_QUEUE_H

#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <deque>
#include <cstddef>
#include <utility>

/// One acquired sample flowing from the acquisition thread to the GUI thread.
struct Sample {
    int    deviceIndex = -1;
    int    regAddr     = 0;
    double value       = 0.0;
    qint64 tsMs        = 0;   // sample timestamp (ms since epoch)
};

/// Thread-safe, bounded FIFO between the acquisition thread (producer) and the
/// GUI thread (consumer) — the "queue" of the producer-consumer pattern.
///
/// Bounded so a slow UI can never grow memory without limit: when the buffer
/// is full the oldest sample is dropped, which is exactly right for a
/// real-time curve (only the freshest data matters).
class SampleQueue
{
public:
    explicit SampleQueue(std::size_t capacity = 4096)
        : m_capacity(capacity)
    {}

    /// Producer side (acquisition thread).
    void push(const Sample &s);

    /// Consumer side (GUI thread). Returns false when empty.
    bool tryPop(Sample &out);

    int size() const;

private:
    mutable QMutex m_mutex;
    std::deque<Sample> m_items;
    std::size_t m_capacity;
};

inline void SampleQueue::push(const Sample &s)
{
    QMutexLocker lock(&m_mutex);
    if (m_items.size() >= m_capacity)
        m_items.pop_front();          // bounded: drop the oldest
    m_items.push_back(s);
}

inline bool SampleQueue::tryPop(Sample &out)
{
    QMutexLocker lock(&m_mutex);
    if (m_items.empty())
        return false;
    out = std::move(m_items.front());
    m_items.pop_front();
    return true;
}

inline int SampleQueue::size() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_items.size());
}

#endif // SAMPLE_QUEUE_H
