#include "main_viewmodel.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"
#include "core/data_collector.h"
#include "communication/modbus_tcp_client.h"

#include <QCoreApplication>
#include <QDir>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainViewModel::MainViewModel(QObject *parent)
    : QObject(parent)
{
    m_client    = new ModbusTcpClient(this);
    m_cache     = new DataCache(this);
    m_alarms    = new AlarmEngine(this);
    m_collector = new DataCollector(m_client, m_cache, m_alarms, this);
    m_model     = m_cache->tableModel();

    // forward collector status + alarms to the View
    connect(m_collector, &DataCollector::connectionStateChanged,
            this, &MainViewModel::connectedChanged);
    connect(m_collector, &DataCollector::statusMessage,
            this, &MainViewModel::statusTextChanged);
    connect(m_alarms, &AlarmEngine::newAlarm,
            this, &MainViewModel::newAlarm);
}

MainViewModel::~MainViewModel() = default;

// ---------------------------------------------------------------------------
// Models
// ---------------------------------------------------------------------------

QAbstractTableModel *MainViewModel::dataModel() const
{
    return m_model;
}

const QVector<AlarmRecord> &MainViewModel::alarms() const
{
    return m_alarms->history();
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MainViewModel::loadConfig(const QString &jsonPath)
{
    // Try a few candidate locations so it works both in the repo and after
    // deployment (where config/ sits next to the executable).
    QStringList candidates;
    candidates << jsonPath
               << QCoreApplication::applicationDirPath() + "/config/devices.json"
               << QCoreApplication::applicationDirPath() + "/devices.json";

    for (const QString &p : candidates) {
        if (m_collector->loadConfig(p))
            return;
    }
    emit statusTextChanged(tr("未找到配置文件 devices.json"));
}

void MainViewModel::connectToDevice()
{
    m_collector->connectDevice();
}

void MainViewModel::disconnectFromDevice()
{
    m_collector->disconnectDevice();
    emit connectedChanged(false);
}
