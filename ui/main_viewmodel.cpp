#include "main_viewmodel.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"
#include "core/data_collector.h"

#include <QCoreApplication>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainViewModel::MainViewModel(QObject *parent)
    : QObject(parent)
{
    m_cache     = new DataCache(this);
    m_alarms    = new AlarmEngine(this);
    m_collector = new DataCollector(m_cache, m_alarms, this);
    m_model     = m_cache->tableModel();

    // forward collector status + alarms to the View
    connect(m_collector, &DataCollector::deviceConnectionChanged,
            this, &MainViewModel::deviceConnectionChanged);
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
    m_collector->connectAll();
}

void MainViewModel::disconnectFromDevice()
{
    m_collector->disconnectAll();
}

void MainViewModel::switchDevice(int deviceIndex)
{
    m_cache->setCurrentDevice(deviceIndex);
}
