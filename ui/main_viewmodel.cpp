#include "main_viewmodel.h"
#include "core/device_manager.h"
#include "core/data_cache.h"
#include "core/alarm_engine.h"
#include "communication/serialport_comm.h"
#include "communication/modbus_rtu.h"

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

MainViewModel::MainViewModel(QObject *parent)
    : QObject(parent)
{
    // Create Model objects — ViewModel owns their lifecycle.
    m_serial    = new SerialPortComm(this);
    m_modbus    = new ModbusRtu(m_serial, this);
    m_deviceMgr = new DeviceManager(m_modbus, this);
    m_cache     = new DataCache(this);
    m_alarms    = new AlarmEngine(this);

    setupModelConnections();
}

MainViewModel::~MainViewModel() = default;

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

bool MainViewModel::isConnected() const
{
    return m_serial && m_serial->isOpen();
}

QString MainViewModel::portName() const
{
    return m_serial ? m_serial->portName() : QString();
}

void MainViewModel::setPortName(const QString &name)
{
    if (m_serial)
        m_serial->setPortName(name);
    emit portNameChanged();
}

int MainViewModel::activeAlarmCount() const
{
    return m_alarms ? m_alarms->activeAlarms().size() : 0;
}

QString MainViewModel::statusText() const
{
    return isConnected() ? tr("Connected") : tr("Disconnected");
}

QAbstractTableModel *MainViewModel::dataModel() const
{
    return m_cache ? m_cache->tableModel() : nullptr;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void MainViewModel::connectToDevice()
{
    if (!m_serial || m_serial->isOpen())
        return;

    m_serial->setBaudRate(9600);

    if (!m_serial->open()) {
        emit connectionError(tr("Failed to open serial port."));
        return;
    }

    m_modbus->open();
    emit connectedChanged();
    emit statusTextChanged(statusText());
}

void MainViewModel::disconnectFromDevice()
{
    m_modbus->close();
    m_serial->close();
    emit connectedChanged();
    emit statusTextChanged(statusText());
}

void MainViewModel::scanDevices()
{
    m_deviceMgr->scan();
    emit deviceListChanged();
}

void MainViewModel::selectDevice(int index)
{
    m_deviceMgr->setActiveDevice(index);
    m_deviceMgr->startPolling(1000);
}

void MainViewModel::acknowledgeAlarm(int index)
{
    m_alarms->acknowledgeAlarm(index);
    emit activeAlarmCountChanged();
}

// ---------------------------------------------------------------------------
// Internal wiring between Model objects
// ---------------------------------------------------------------------------

void MainViewModel::setupModelConnections()
{
    // Serial state → ViewModel
    connect(m_serial, &CommInterface::stateChanged, this, [this](bool connected) {
        Q_UNUSED(connected)
        emit connectedChanged();
        emit statusTextChanged(statusText());
    });

    // Serial error → ViewModel
    connect(m_serial, &CommInterface::errorOccurred,
            this, &MainViewModel::connectionError);

    // Device data → cache
    connect(m_deviceMgr, &DeviceManager::deviceDataChanged,
            m_cache, &DataCache::updateValue);

    // Device alarms → alarm engine
    connect(m_deviceMgr, &DeviceManager::alarmTriggered,
            m_alarms, &AlarmEngine::onAlarmTriggered);

    // Alarm engine → ViewModel
    connect(m_alarms, &AlarmEngine::newAlarm, this, [this](const AlarmRecord &rec) {
        emit newAlarmTriggered(rec.message);
        emit activeAlarmCountChanged();
    });

    connect(m_alarms, &AlarmEngine::alarmAcknowledged, this, [this](int) {
        emit activeAlarmCountChanged();
    });

    // Device list changed
    connect(m_deviceMgr, &DeviceManager::deviceDiscovered,
            this, [this](const DeviceInfo &) {
                emit deviceListChanged();
            });
}
