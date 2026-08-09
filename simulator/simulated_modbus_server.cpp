#include "simulated_modbus_server.h"
#include "simulated_device.h"

#include <QModbusTcpServer>
#include <QModbusServer>
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

    // 重启后波形重新接管所有寄存器（清除上一次运行被遥控写入的值）。
    m_overrides.clear();
    // 记录上位机通过 Modbus 写入的寄存器，push 时不再用波形覆盖它们，
    // 让"写寄存器"这个动作能在曲线上稳定看到。注意 dataWritten 也会在
    // 我们自己的 setData()（push 每 50ms）时发出——用 m_pushing 区分。
    connect(m_server, &QModbusServer::dataWritten, this,
            [this](QModbusDataUnit::RegisterType table, int address, int size) {
                if (m_pushing)
                    return;   // 我们自己的波形 push，忽略
                if (table != QModbusDataUnit::HoldingRegisters)
                    return;
                // 信号不带值——从服务器内部数据区读回刚写入的值。
                for (int i = 0; i < size; ++i) {
                    quint16 v = 0;
                    m_server->data(table, static_cast<quint16>(address + i), &v);
                    m_overrides.insert(address + i, v);
                }
            });

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
    for (int i = 0; i < m_device->registerCount(); ++i) {
        const auto it = m_overrides.constFind(i);
        // Host-written registers hold their value; everything else follows the
        // simulated waveform.
        unit.setValue(i, it != m_overrides.constEnd()
                             ? it.value() : m_device->rawRegister(i));
    }

    m_pushing = true;   // this setData() emits dataWritten() — don't self-record
    m_server->setData(unit);
    m_pushing = false;
}
