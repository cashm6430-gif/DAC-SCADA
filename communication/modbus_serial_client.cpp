#include "modbus_serial_client.h"
#include <QModbusReply>
#include <QSerialPort>
#include <QVariant>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

ModbusSerialClient::ModbusSerialClient(QObject *parent)
    : QObject(parent)
{
    m_client = new QModbusRtuSerialClient(this);

    connect(m_client, &QModbusClient::stateChanged,
            this, &ModbusSerialClient::onStateChanged);
    connect(m_client, &QModbusClient::errorOccurred,
            this, &ModbusSerialClient::onErrorOccurred);
}

ModbusSerialClient::~ModbusSerialClient()
{
    disconnectFrom();
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

bool ModbusSerialClient::connectTo(const QString &portName, int baudRate, int unitId)
{
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();

    m_portName = portName;
    m_baudRate = baudRate;
    m_unitId   = unitId;

    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                     QVariant(portName));
    m_client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                     QVariant(baudRate));
    m_client->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                     QVariant(8));
    m_client->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                     QVariant(QSerialPort::NoParity));
    m_client->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                     QVariant(1));
    m_client->setTimeout(1000);
    m_client->setNumberOfRetries(1);

    return m_client->connectDevice();
}

void ModbusSerialClient::disconnectFrom()
{
    if (m_pendingReply) {
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    m_client->disconnectDevice();
}

bool ModbusSerialClient::isConnected() const
{
    return m_client->state() == QModbusDevice::ConnectedState;
}

// ---------------------------------------------------------------------------
// Modbus operations
// ---------------------------------------------------------------------------

void ModbusSerialClient::readHoldingRegisters(int startAddr, int count)
{
    if (!isConnected()) {
        emit communicationError(tr("串口未连接: %1").arg(m_portName));
        return;
    }
    if (m_pendingReply)
        return;   // serialized

    sendReadRequest(startAddr, count);
}

void ModbusSerialClient::sendReadRequest(int startAddr, int count)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddr, count);

    m_pendingReply = m_client->sendReadRequest(unit, m_unitId);
    if (!m_pendingReply) {
        emit communicationError(tr("发送 Modbus 请求失败: %1")
                                    .arg(m_client->errorString()));
        return;
    }
    connect(m_pendingReply, &QModbusReply::finished,
            this, &ModbusSerialClient::onReplyFinished);
}

void ModbusSerialClient::writeSingleRegister(int regAddr, quint16 value)
{
    if (!isConnected()) {
        emit communicationError(tr("串口未连接: %1").arg(m_portName));
        return;
    }
    if (m_pendingReply)
        return;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, regAddr, 1);
    unit.setValue(0, value);

    m_pendingReply = m_client->sendWriteRequest(unit, m_unitId);
    if (!m_pendingReply) {
        emit communicationError(tr("发送 Modbus 写入失败: %1")
                                    .arg(m_client->errorString()));
        return;
    }
    connect(m_pendingReply, &QModbusReply::finished,
            this, &ModbusSerialClient::onReplyFinished);
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void ModbusSerialClient::onStateChanged(QModbusDevice::State state)
{
    const bool connected = (state == QModbusDevice::ConnectedState);
    qInfo() << "ModbusSerialClient::onStateChanged" << static_cast<int>(state)
            << "port=" << m_portName << "connected=" << connected;
    emit connectionStateChanged(connected);
}

void ModbusSerialClient::onReplyFinished()
{
    auto *reply = m_pendingReply;
    m_pendingReply = nullptr;

    if (!reply)
        return;

    if (reply->error() != QModbusDevice::NoError) {
        emit communicationError(tr("Modbus 响应错误: %1")
                                    .arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    const auto result = reply->result();
    reply->deleteLater();

    if (result.isValid() && result.registerType() == QModbusDataUnit::HoldingRegisters)
        emit registersRead(result);
}

void ModbusSerialClient::onErrorOccurred(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError)
        emit communicationError(m_client->errorString());
}
