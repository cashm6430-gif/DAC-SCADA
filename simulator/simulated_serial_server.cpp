#include "simulated_serial_server.h"
#include "simulated_device.h"

#include <QModbusRtuSerialServer>
#include <QModbusServer>
#include <QModbusDataUnit>
#include <QSerialPort>
#include <QVariant>
#include <QDebug>

SimulatedSerialServer::SimulatedSerialServer(QObject *parent)
    : QObject(parent)
{
    m_device = new SimulatedDevice(this);
    m_device->setRegisterCount(8);
    m_device->setUpdateInterval(50);

    // Waveforms similar to the TCP simulator (real values)
    m_device->setWaveConfig(0, { 50.0, 20.0, 0.06, 1.0, 100.0 });   // 50 ± 20
    m_device->setWaveConfig(1, { 25.0, 10.0, 0.10, 0.5, 100.0 });   // 25 ± 10
    m_device->setWaveConfig(2, {  8.0,  4.0, 0.15, 0.3, 100.0 });   // 8 ± 4
    m_device->setWaveConfig(3, { 100.0, 50.0, 0.08, 2.0, 100.0 });  // 100 ± 50

    connect(m_device, &SimulatedDevice::registersUpdated,
            this, &SimulatedSerialServer::pushRegistersToModbus);
}

SimulatedSerialServer::~SimulatedSerialServer()
{
    stop();
}

bool SimulatedSerialServer::start(const QString &portName, int baudRate)
{
    if (m_server)
        return m_server->state() == QModbusDevice::ConnectedState;

    m_server = new QModbusRtuSerialServer(this);
    m_server->setServerAddress(1);

    m_server->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                     QVariant(portName));
    m_server->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                     QVariant(baudRate));
    m_server->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                     QVariant(8));
    m_server->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                     QVariant(QSerialPort::NoParity));
    m_server->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                     QVariant(1));

    if (!m_server->connectDevice()) {
        qWarning() << "SimulatedSerialServer: failed to open" << portName
                   << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    // Initialize the Holding Registers table
    QModbusDataUnitMap regMap;
    QModbusDataUnit holdingRegs(QModbusDataUnit::HoldingRegisters, 0,
                                m_device->registerCount());
    regMap.insert(QModbusDataUnit::HoldingRegisters, holdingRegs);
    m_server->setMap(regMap);

    // Remote-control writes hold their value (waveform does not overwrite them).
    m_overrides.clear();
    connect(m_server, &QModbusServer::dataWritten, this,
            [this](QModbusDataUnit::RegisterType table, int address, int size) {
                if (m_pushing)
                    return;   // our own waveform push — not a host write
                if (table != QModbusDataUnit::HoldingRegisters)
                    return;
                for (int i = 0; i < size; ++i) {
                    quint16 v = 0;
                    m_server->data(table, static_cast<quint16>(address + i), &v);
                    m_overrides.insert(address + i, v);
                }
            });

    m_device->start();
    qInfo() << "Simulated serial slave serving on" << portName << "@" << baudRate;
    return true;
}

void SimulatedSerialServer::stop()
{
    if (m_device)
        m_device->stop();
    if (m_server) {
        m_server->disconnectDevice();
        delete m_server;
        m_server = nullptr;
    }
}

bool SimulatedSerialServer::isListening() const
{
    return m_server
        && m_server->state() == QModbusDevice::ConnectedState;
}

SimulatedDevice *SimulatedSerialServer::device() const
{
    return m_device;
}

void SimulatedSerialServer::pushRegistersToModbus()
{
    if (!m_server)
        return;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0,
                         m_device->registerCount());
    for (int i = 0; i < m_device->registerCount(); ++i) {
        const auto it = m_overrides.constFind(i);
        unit.setValue(i, it != m_overrides.constEnd()
                             ? it.value() : m_device->rawRegister(i));
    }
    m_pushing = true;   // this setData() emits dataWritten() — don't self-record
    m_server->setData(unit);
    m_pushing = false;
}
