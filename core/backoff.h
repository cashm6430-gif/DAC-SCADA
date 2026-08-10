#ifndef BACKOFF_H
#define BACKOFF_H

#include <QtGlobal>

/// Exponential backoff: base·2^retryCount, capped at maxMs.
///
/// retryCount is clamped to [0, 12] so the shift can never overflow; the
/// result never exceeds maxMs. Pure helper — unit-testable without a live
/// DataCollector (used by DataCollector::backoffMs).
inline qint64 reconnectBackoffMs(int retryCount, qint64 baseMs, qint64 maxMs)
{
    const int exp = qBound(0, retryCount, 12);
    const qint64 ms = baseMs << exp;
    return qMin(ms, maxMs);
}

#endif // BACKOFF_H
