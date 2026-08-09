#include "history_store.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

HistoryStore::HistoryStore(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_dbPath(dbPath)
{
    m_flushTimer = new QTimer(this);   // child → moves to worker thread
    m_flushTimer->setInterval(kFlushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &HistoryStore::onFlushTimer);
}

HistoryStore::~HistoryStore()
{
    // Drain whatever is still buffered (may be called on the worker thread
    // when the collector is torn down).
    m_flushTimer->stop();
    flush();

    if (m_dbOpen) {
        const QString conn = connectionName();
        {
            QSqlDatabase db = QSqlDatabase::database(conn, false);
            if (db.isOpen())
                db.close();
        }
        // No QSqlDatabase handle references the connection here, so it can
        // actually be released.
        QSqlDatabase::removeDatabase(conn);
        m_dbOpen = false;
    }
}

// ---------------------------------------------------------------------------
// public API (worker thread)
// ---------------------------------------------------------------------------

void HistoryStore::start()
{
    if (!m_flushTimer->isActive())
        m_flushTimer->start();
}

void HistoryStore::addSample(int deviceIndex, int regAddr, qint64 tsMs,
                             double value)
{
    if (m_failed)
        return;

    m_devIdx.append(deviceIndex);
    m_regAddr.append(regAddr);
    m_ts.append(tsMs);
    m_val.append(value);
    m_totalSamples.fetch_add(1, std::memory_order_relaxed);

    if (m_devIdx.size() >= kFlushThreshold)
        flush();
}

void HistoryStore::addAlarm(const AlarmRecord &rec)
{
    if (m_failed)
        return;

    m_alarmTs.append(rec.timestamp.toMSecsSinceEpoch());
    m_alarmSev.append(static_cast<int>(rec.severity));
    m_alarmDev.append(static_cast<int>(rec.deviceAddr));
    m_alarmMsg.append(rec.message);

    if (m_alarmTs.size() >= kFlushThreshold)
        flush();
}

bool HistoryStore::flush()
{
    return doFlush();
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void HistoryStore::onFlushTimer()
{
    doFlush();
}

// ---------------------------------------------------------------------------
// internals
// ---------------------------------------------------------------------------

QString HistoryStore::connectionName()
{
    return QStringLiteral("dac_scada_history");
}

bool HistoryStore::openDatabase()
{
    if (m_dbOpen)
        return true;

    // Ensure the parent directory exists (e.g. <exe>/data/).
    const QFileInfo fi(m_dbPath);
    if (!QDir().mkpath(fi.absolutePath())) {
        qWarning() << "HistoryStore: cannot create dir" << fi.absolutePath();
        return false;
    }

    const QString conn = connectionName();
    if (QSqlDatabase::contains(conn)) {
        // Drop a stale connection left over by an earlier session.
        QSqlDatabase::removeDatabase(conn);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "HistoryStore: cannot open" << m_dbPath
                   << db.lastError().text();
        return false;
    }

    // Split into separate exec() calls — QSqlQuery::exec only prepares the
    // first statement of a multi-statement string.
    QSqlQuery q(db);
    const bool schemaOk = q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS samples ("
            "  device_index INTEGER NOT NULL,"
            "  reg_addr     INTEGER NOT NULL,"
            "  ts           INTEGER NOT NULL,"
            "  value        REAL    NOT NULL)"))
        && q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_samples_dev_ts "
            "ON samples(device_index, ts)"))
        && q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarms ("
            "  ts           INTEGER NOT NULL,"
            "  severity     INTEGER NOT NULL,"
            "  device_index INTEGER NOT NULL,"
            "  message      TEXT    NOT NULL)"));

    if (!schemaOk) {
        qWarning() << "HistoryStore: schema creation failed:"
                   << q.lastError().text();
        db.close();
        return false;
    }

    m_dbOpen = true;
    qInfo() << "HistoryStore: opened SQLite database at" << m_dbPath;
    return true;
}

bool HistoryStore::doFlush()
{
    if (m_failed)
        return false;
    if (m_devIdx.isEmpty() && m_alarmTs.isEmpty())
        return true;

    if (!m_dbOpen && !openDatabase())
        return false;

    QSqlDatabase db = QSqlDatabase::database(connectionName(), false);
    if (!db.isOpen())
        return false;

    if (!db.transaction()) {
        qWarning() << "HistoryStore: begin transaction failed:"
                   << db.lastError().text();
        m_failed = true;
        return false;
    }

    QSqlQuery q(db);
    bool ok = true;

    if (!m_devIdx.isEmpty()) {
        q.prepare(QStringLiteral(
            "INSERT INTO samples (device_index, reg_addr, ts, value) "
            "VALUES (?, ?, ?, ?)"));
        q.addBindValue(m_devIdx);
        q.addBindValue(m_regAddr);
        q.addBindValue(m_ts);
        q.addBindValue(m_val);
        if (!q.execBatch())
            ok = false;
    }

    if (ok && !m_alarmTs.isEmpty()) {
        q.finish();
        q.prepare(QStringLiteral(
            "INSERT INTO alarms (ts, severity, device_index, message) "
            "VALUES (?, ?, ?, ?)"));
        q.addBindValue(m_alarmTs);
        q.addBindValue(m_alarmSev);
        q.addBindValue(m_alarmDev);
        q.addBindValue(m_alarmMsg);
        if (!q.execBatch())
            ok = false;
    }

    if (ok) {
        db.commit();
        m_devIdx.clear();  m_regAddr.clear();  m_ts.clear();  m_val.clear();
        m_alarmTs.clear(); m_alarmSev.clear(); m_alarmDev.clear(); m_alarmMsg.clear();
        return true;
    }

    db.rollback();
    qWarning() << "HistoryStore: batch insert failed:" << q.lastError().text();
    m_failed = true;   // don't hammer a broken database every flush tick
    return false;
}
