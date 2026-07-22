#include "device_manager.h"
#include "communication/modbus_rtu.h"
#include <QModbusDataUnit>

DeviceManager::DeviceManager(ModbusRtu *modbus, QObject *parent)
    : QObject(parent)
    , m_modbus(modbus)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &DeviceManager::onPollTimeout);
    connect(m_modbus, &ModbusRtu::registersRead,
            this, &DeviceManager::onRegistersRead);
}

DeviceManager::~DeviceManager()
{
    stopPolling();
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void DeviceManager::scan()
{
    m_devices.clear();

    // Quick address sweep (Modbus addresses 1-247).
    for (int addr = 1; addr <= 10; ++addr) {
        DeviceInfo info;
        info.address = addr;
        info.name    = QString("Device-%1").arg(addr);
        info.type    = "generic";
        info.online  = false;
        m_devices.append(info);
        emit deviceDiscovered(info);
    }
}

void DeviceManager::setActiveDevice(int index)
{
    if (index < 0 || index >= m_devices.size())
        return;
    m_activeIndex = index;
}

void DeviceManager::startPolling(int intervalMs)
{
    m_pollTimer.start(intervalMs);
}

void DeviceManager::stopPolling()
{
    m_pollTimer.stop();
}

const QVector<DeviceInfo> &DeviceManager::devices() const
{
    return m_devices;
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void DeviceManager::onPollTimeout()
{
    if (m_activeIndex < 0 || !m_modbus || !m_modbus->isOpen())
        return;

    const DeviceInfo &dev = m_devices.at(m_activeIndex);
    // Read holding registers 0-7 as a typical scan window.
    m_modbus->readHoldingRegisters(dev.address, 0, 8);
}

void DeviceManager::onRegistersRead(int deviceAddr, const QModbusDataUnit &unit)
{
    const auto values = unit.values();
    for (int i = 0; i < values.size(); ++i) {
        const int regAddr = unit.startAddress() + i;
        emit deviceDataChanged(deviceAddr, regAddr, values.at(i));

        // Simple threshold alarm example
        if (values.at(i) > 30000) {
            emit alarmTriggered(deviceAddr,
                QString("High value %1 on reg %2").arg(values.at(i)).arg(regAddr));
        }
    }
}
