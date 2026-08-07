#include "simulated_modbus_server.h"
#include "simulated_device.h"

#include <QModbusTcpServer>
#include <QModbusDataUnit>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

SimulatedModbusServer::SimulatedModbusServer(QObject *parent)
    : QObject(parent)
{
    m_device = new SimulatedDevice(this);
    m_device->setRegisterCount(8);
    m_device->setUpdateInterval(50);   // 20 Hz

    // 为前几个寄存器配置不同的波形，模拟不同物理量（真实值）
    m_device->setWaveConfig(0, { 25.0, 15.0, 0.05, 1.0, 100.0 });   // 温度 25 ± 15 °C
    m_device->setWaveConfig(1, { 50.0, 20.0, 0.08, 2.0, 100.0 });   // 湿度 50 ± 20 %
    m_device->setWaveConfig(2, { 10.0,  5.0, 0.12, 0.3, 1000.0 });  // 压力 10 ± 5 kPa
    m_device->setWaveConfig(3, {  3.0,  1.0, 0.20, 0.1,  100.0 });  // 流量 3 ± 1 m³/h

    // 每周期把新寄存器值写入 Modbus 数据区
    connect(m_device, &SimulatedDevice::registersUpdated,
            this, &SimulatedModbusServer::pushRegistersToModbus);
}

SimulatedModbusServer::~SimulatedModbusServer()
{
    stop();
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

bool SimulatedModbusServer::start(quint16 port, const QString &host)
{
    if (m_server)
        return m_server->state() == QModbusDevice::ConnectedState;

    m_server = new QModbusTcpServer(this);
    m_server->setServerAddress(1);   // 单元 ID = 1

    // 配置监听地址和端口（Qt 6.8 API）
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter, host);

    if (!m_server->connectDevice()) {
        qWarning() << "SimulatedModbusServer: failed to start:"
                   << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    // 初始化 Holding Registers 表（必须先 setMap 建表，setData 才能写入）
    QModbusDataUnitMap regMap;
    QModbusDataUnit holdingRegs(QModbusDataUnit::HoldingRegisters, 0,
                                m_device->registerCount());
    regMap.insert(QModbusDataUnit::HoldingRegisters, holdingRegs);
    m_server->setMap(regMap);

    m_device->start();

    qInfo() << "Simulated Modbus server listening on"
            << host << ":" << port
            << "(" << m_device->registerCount() << "holding registers )";
    return true;
}

void SimulatedModbusServer::stop()
{
    if (m_device)
        m_device->stop();

    if (m_server) {
        m_server->disconnectDevice();
        delete m_server;
        m_server = nullptr;
    }
}

bool SimulatedModbusServer::isListening() const
{
    return m_server
        && m_server->state() == QModbusDevice::ConnectedState;
}

SimulatedDevice *SimulatedModbusServer::device() const
{
    return m_device;
}

// ---------------------------------------------------------------------------
// internal
// ---------------------------------------------------------------------------

void SimulatedModbusServer::pushRegistersToModbus()
{
    if (!m_server)
        return;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0,
                         m_device->registerCount());
    for (int i = 0; i < m_device->registerCount(); ++i)
        unit.setValue(i, m_device->rawRegister(i));

    m_server->setData(unit);
}
