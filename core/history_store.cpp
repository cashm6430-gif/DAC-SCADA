#include "history_store.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
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
    m_alarmDev.append(static_cast<int>(rec.deviceIndex));
    m_alarmMsg.append(rec.message);

    if (m_alarmTs.size() >= kFlushThreshold)
        flush();
}

bool HistoryStore::flush()
{
    return doFlush();
}

// ---------------------------------------------------------------------------
// read-back API (worker thread)
// ---------------------------------------------------------------------------

void HistoryStore::querySamples(int requestId, int deviceIndex,
                                const HistoryQuery &q)
{
    HistoryResult result;
    result.requestId = requestId;
    result.deviceIndex = deviceIndex;

    if ((!m_dbOpen && !openDatabase()) || !QSqlDatabase::database(
            connectionName(), false).isOpen()) {
        // No history has been written yet — an empty result is the correct
        // answer, not an error.
        emit samplesReady(requestId, deviceIndex, result);
        return;
    }

    QString sql = QStringLiteral(
        "SELECT reg_addr, ts, value FROM samples "
        "WHERE device_index = :dev AND ts >= :start AND ts <= :end");
    if (!q.regAddrs.isEmpty()) {
        QStringList ph;
        for (int i = 0; i < q.regAddrs.size(); ++i)
            ph << QStringLiteral(":a%1").arg(i);
        sql += QStringLiteral(" AND reg_addr IN (%1)").arg(ph.join(QLatin1Char(',')));
    }
    sql += QStringLiteral(" ORDER BY ts ASC LIMIT %1").arg(kMaxQueryRows);

    QSqlDatabase db = QSqlDatabase::database(connectionName(), false);
    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        qWarning() << "HistoryStore::querySamples: prepare failed:"
                   << query.lastError().text();
        emit samplesReady(requestId, deviceIndex, result);
        return;
    }

    query.bindValue(QStringLiteral(":dev"), deviceIndex);
    query.bindValue(QStringLiteral(":start"), q.startMs);
    query.bindValue(QStringLiteral(":end"), q.endMs);
    for (int i = 0; i < q.regAddrs.size(); ++i)
        query.bindValue(QStringLiteral(":a%1").arg(i), q.regAddrs.at(i));

    if (!query.exec()) {
        qWarning() << "HistoryStore::querySamples: exec failed:"
                   << query.lastError().text();
        emit samplesReady(requestId, deviceIndex, result);
        return;
    }

    result.rows.reserve(1024);
    while (query.next()) {
        HistoryRow row;
        row.regAddr = query.value(0).toInt();
        row.tsMs    = query.value(1).toLongLong();
        row.value   = query.value(2).toDouble();
        result.rows.append(row);
    }
    emit samplesReady(requestId, deviceIndex, result);
}

void HistoryStore::queryAlarms(int requestId, int deviceIndex,
                               qint64 startMs, qint64 endMs)
{
    QVector<AlarmRecord> alarms;
    if ((!m_dbOpen && !openDatabase()) || !QSqlDatabase::database(
            connectionName(), false).isOpen()) {
        emit alarmsReady(requestId, deviceIndex, alarms);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName(), false);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT ts, severity, device_index, message FROM alarms "
        "WHERE device_index = :dev AND ts >= :start AND ts <= :end "
        "ORDER BY ts ASC LIMIT %1").arg(kMaxQueryRows));
    query.bindValue(QStringLiteral(":dev"), deviceIndex);
    query.bindValue(QStringLiteral(":start"), startMs);
    query.bindValue(QStringLiteral(":end"), endMs);

    if (!query.exec()) {
        qWarning() << "HistoryStore::queryAlarms: exec failed:"
                   << query.lastError().text();
        emit alarmsReady(requestId, deviceIndex, alarms);
        return;
    }

    while (query.next()) {
        AlarmRecord a;
        a.timestamp  = QDateTime::fromMSecsSinceEpoch(query.value(0).toLongLong());
        a.severity   = static_cast<AlarmRecord::Severity>(query.value(1).toInt());
        a.deviceIndex = static_cast<qint16>(query.value(2).toInt());
        a.message    = query.value(3).toString();
        a.acknowledged = false;
        alarms.append(a);
    }
    emit alarmsReady(requestId, deviceIndex, alarms);
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

    // ---- tuning + retention ----
    // WAL smooths the batched inserts; auto_vacuum keeps the file from
    // growing forever. VACUUM only runs once (when the DB was created without
    // auto_vacuum) to migrate it — it is deliberately not run every startup.
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    QSqlQuery pragma(db);
    if (pragma.exec(QStringLiteral("PRAGMA auto_vacuum")) && pragma.next()
        && pragma.value(0).toInt() == 0) {
        q.exec(QStringLiteral("PRAGMA auto_vacuum=INCREMENTAL"));
        q.exec(QStringLiteral("VACUUM"));
    }

    // Purge rows older than the retention window, then reclaim the freed pages.
    const qint64 cutoffMs = QDateTime::currentMSecsSinceEpoch()
        - static_cast<qint64>(kRetentionDays) * 24 * 3600 * 1000LL;
    if (q.exec(QStringLiteral("DELETE FROM samples WHERE ts < %1").arg(cutoffMs))
        && q.exec(QStringLiteral("DELETE FROM alarms WHERE ts < %1").arg(cutoffMs))) {
        q.exec(QStringLiteral("PRAGMA incremental_vacuum"));
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
