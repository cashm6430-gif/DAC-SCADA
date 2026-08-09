#ifndef HISTORY_STORE_H
#define HISTORY_STORE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QVariant>
#include <atomic>
#include "core/types.h"

class QSqlDatabase;
class QSqlQuery;

/// Persists acquired samples (and alarms) to SQLite with batched writes.
///
/// Lives on the acquisition (worker) thread (it is a child of DataCollector,
/// so it moves together with the collector). Rows are buffered in memory and
/// flushed inside a single transaction once the buffer reaches a size
/// threshold or a flush interval elapses — committing one row at a time would
/// be roughly two orders of magnitude slower.
class HistoryStore : public QObject
{
    Q_OBJECT

public:
    explicit HistoryStore(const QString &dbPath, QObject *parent = nullptr);
    ~HistoryStore() override;

    /// Start the periodic flush timer. Must be called from the worker thread
    /// (the timer belongs to this object, hence to the worker thread).
    void start();

    /// Buffer one sample row (called from the acquisition thread).
    void addSample(int deviceIndex, int regAddr, qint64 tsMs, double value);

    /// Buffer one alarm row (called from the acquisition thread).
    void addAlarm(const AlarmRecord &rec);

    /// Total samples buffered+flushed so far. Atomic — safe to read from the
    /// GUI thread (used by --selftest to prove history is being written).
    qint64 totalSamples() const
    { return m_totalSamples.load(std::memory_order_relaxed); }

    /// Force a flush of the buffered rows. Returns false on a hard DB error.
    bool flush();

private slots:
    void onFlushTimer();

private:
    bool openDatabase();
    bool doFlush();
    static QString connectionName();

    QString m_dbPath;
    /// Flush timer — must be a QObject CHILD (not a value member) so that
    /// moveToThread() carries it to the worker thread with this store.
    QTimer *m_flushTimer = nullptr;
    bool    m_dbOpen = false;
    bool    m_failed = false;   // after a hard DB error, stop retrying
    std::atomic<qint64> m_totalSamples{0};

    // pending sample rows (column-parallel vectors → QSqlQuery::execBatch)
    QVector<QVariant> m_devIdx;
    QVector<QVariant> m_regAddr;
    QVector<QVariant> m_ts;
    QVector<QVariant> m_val;

    // pending alarm rows
    QVector<QVariant> m_alarmTs;
    QVector<QVariant> m_alarmSev;
    QVector<QVariant> m_alarmDev;
    QVector<QVariant> m_alarmMsg;

    /// Flush the buffer when it grows past this many rows.
    static constexpr int kFlushThreshold = 512;
    /// Flush cadence even when the buffer stays below the threshold.
    static constexpr int kFlushIntervalMs = 1000;
};

#endif // HISTORY_STORE_H
