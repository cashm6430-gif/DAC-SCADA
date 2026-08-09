#include "main_viewmodel.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"
#include "core/data_collector.h"
#include "core/history_store.h"
#include "core/sample_queue.h"

#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QMetaObject>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainViewModel::MainViewModel(QObject *parent)
    : QObject(parent)
{
    m_cache     = new DataCache(this);
    m_collector = new DataCollector(nullptr);   // no parent — moved to worker
    m_alarms    = m_collector->alarmEngine();
    m_model     = m_cache->tableModel();

    // ---- acquisition worker thread (producer side) ----
    m_worker = new QThread(this);
    m_collector->moveToThread(m_worker);
    connect(m_worker, &QThread::finished,
            m_collector, &QObject::deleteLater);
    m_worker->start();

    // ---- forward collector status to the View ----
    connect(m_collector, &DataCollector::deviceConnectionChanged,
            this, [this](int idx, bool c) {
                if (idx >= 0 && idx < m_status.size())
                    m_status[idx].connected = c;
                emit deviceConnectionChanged(idx, c);
            });
    connect(m_collector, &DataCollector::deviceOnlineChanged,
            this, [this](int idx, bool o) {
                if (idx >= 0 && idx < m_status.size())
                    m_status[idx].online = o;
                emit deviceOnlineChanged(idx, o);
            });
    connect(m_collector, &DataCollector::deviceFailCountChanged,
            this, [this](int idx, int n) {
                if (idx >= 0 && idx < m_status.size())
                    m_status[idx].failCount = n;
            });
    connect(m_collector, &DataCollector::pollingStateChanged,
            this, [this](bool a) { m_pollingActive = a; });
    connect(m_collector, &DataCollector::statusMessage,
            this, &MainViewModel::statusTextChanged);

    // ---- config parsed on the worker → install it in the GUI-side cache ----
    connect(m_collector, &DataCollector::configLoaded, this, [this]() {
        const auto &devs = m_collector->devices();
        m_status.resize(devs.size());
        m_cache->setDevices(devs);
    });

    // ---- alarm history stays on the GUI thread (worker emits the record) ----
    connect(m_alarms, &AlarmEngine::newAlarm, this,
            [this](const AlarmRecord &rec) {
                m_alarmHistory.append(rec);
                if (m_alarmHistory.size() > kMaxAlarmHistory)
                    m_alarmHistory.remove(0,
                        m_alarmHistory.size() - kMaxAlarmHistory);
                emit newAlarm(rec);
            });

    // ---- forward history query results (store lives on the worker thread) ----
    connect(m_collector->historyStore(), &HistoryStore::samplesReady,
            this, &MainViewModel::historySamplesReady);
    connect(m_collector->historyStore(), &HistoryStore::alarmsReady,
            this, &MainViewModel::historyAlarmsReady);

    // ---- forward remote-write outcome ----
    connect(m_collector, &DataCollector::writeFinished,
            this, &MainViewModel::writeFinished);

    // ---- consumer side: drain the producer-consumer queue in batches ----
    m_drainTimer = new QTimer(this);
    connect(m_drainTimer, &QTimer::timeout, this, &MainViewModel::drainSamples);
    m_drainTimer->start(kDrainIntervalMs);
}

MainViewModel::~MainViewModel()
{
    if (m_drainTimer)
        m_drainTimer->stop();

    if (m_worker && m_collector) {
        // Tear the transports down on the worker thread before it stops; the
        // collector (and its clients/history store) is deleted on that thread
        // via the finished → deleteLater connection.
        QMetaObject::invokeMethod(m_collector, "disconnectAll",
                                  Qt::BlockingQueuedConnection);
        m_worker->quit();
        m_worker->wait();
    }
}

// ---------------------------------------------------------------------------
// Models
// ---------------------------------------------------------------------------

QAbstractTableModel *MainViewModel::dataModel() const
{
    return m_model;
}

const QVector<AlarmRecord> &MainViewModel::alarms() const
{
    return m_alarmHistory;
}

// ---------------------------------------------------------------------------
// Status snapshot (GUI thread)
// ---------------------------------------------------------------------------

bool MainViewModel::deviceConnected(int deviceIndex) const
{
    return deviceIndex >= 0 && deviceIndex < m_status.size()
        && m_status.at(deviceIndex).connected;
}

bool MainViewModel::deviceOnline(int deviceIndex) const
{
    return deviceIndex >= 0 && deviceIndex < m_status.size()
        && m_status.at(deviceIndex).online;
}

int MainViewModel::deviceFailureCount(int deviceIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= m_status.size())
        return 0;
    return m_status.at(deviceIndex).failCount;
}

qint64 MainViewModel::historySamples() const
{
    return m_collector->historySamples();
}

// ---------------------------------------------------------------------------
// consumer: batch-drain the acquisition queue into the cache
// ---------------------------------------------------------------------------

void MainViewModel::drainSamples()
{
    Sample s;
    while (m_collector->queue()->tryPop(s))
        m_cache->updateValue(s.deviceIndex, s.regAddr, s.value, s.tsMs);
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MainViewModel::loadConfig(const QString &jsonPath)
{
    QStringList candidates;
    candidates << jsonPath
               << QCoreApplication::applicationDirPath() + "/config/devices.json"
               << QCoreApplication::applicationDirPath() + "/devices.json";

    bool ok = false;
    for (const QString &p : candidates) {
        // Config parsing runs on the worker thread (collector affinity); we
        // block briefly to learn whether to try the next fallback path.
        QMetaObject::invokeMethod(m_collector, "loadConfig",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, ok),
                                  Q_ARG(QString, p));
        if (ok)
            break;
    }
    if (!ok)
        emit statusTextChanged(tr("未找到配置文件 devices.json"));
}

void MainViewModel::connectToDevice()
{
    QMetaObject::invokeMethod(m_collector, "connectAll", Qt::QueuedConnection);
}

void MainViewModel::disconnectFromDevice()
{
    QMetaObject::invokeMethod(m_collector, "disconnectAll", Qt::QueuedConnection);
}

void MainViewModel::switchDevice(int deviceIndex)
{
    m_cache->setCurrentDevice(deviceIndex);
}

int MainViewModel::queryHistory(int deviceIndex, const HistoryQuery &q)
{
    const int requestId = ++m_historyRequestSeq;
    HistoryStore *store = m_collector->historyStore();
    // Queued so the SELECT runs on the worker thread; the result comes back
    // on historySamplesReady without ever blocking the GUI.
    QMetaObject::invokeMethod(store, "querySamples", Qt::QueuedConnection,
                              Q_ARG(int, requestId),
                              Q_ARG(int, deviceIndex),
                              Q_ARG(HistoryQuery, q));
    return requestId;
}

int MainViewModel::queryAlarms(int deviceIndex, qint64 startMs, qint64 endMs)
{
    const int requestId = ++m_historyRequestSeq;
    HistoryStore *store = m_collector->historyStore();
    QMetaObject::invokeMethod(store, "queryAlarms", Qt::QueuedConnection,
                              Q_ARG(int, requestId),
                              Q_ARG(int, deviceIndex),
                              Q_ARG(qint64, startMs),
                              Q_ARG(qint64, endMs));
    return requestId;
}

void MainViewModel::writeRegister(int deviceIndex, int regAddr, quint16 value)
{
    // Runs on the worker thread (collector affinity); the outcome is reported
    // back through the queued writeFinished signal.
    QMetaObject::invokeMethod(m_collector, "writeRegister", Qt::QueuedConnection,
                              Q_ARG(int, deviceIndex),
                              Q_ARG(int, regAddr),
                              Q_ARG(quint16, value));
}
