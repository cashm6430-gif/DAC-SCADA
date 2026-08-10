#include "modbus_serial_client.h"
#include <QModbusReply>
#include <QSerialPort>
#include <QVariant>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

ModbusSerialClient::ModbusSerialClient(QObject *parent)
    : IModbusClient(parent)
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

bool ModbusSerialClient::connectTo(const DeviceInfo &info)
{
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();

    m_portName = info.serialPort;
    m_baudRate = info.baudRate;
    m_unitId   = info.address;

    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                     QVariant(m_portName));
    m_client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                     QVariant(m_baudRate));
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
    m_pendingWrite = false;   // a stale deferred write must not cross a reconnect
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

    m_pendingReadRequest  = true;
    m_pendingWriteRequest = false;
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
        const QString err = tr("串口未连接: %1").arg(m_portName);
        emit writeFailed(regAddr, err);
        emit communicationError(err);
        return;
    }
    if (m_pendingReply) {
        // Bus busy with a read — stash the write and flush it when free (the
        // poller re-issues reads every tick, but a control write must not drop).
        m_pendingWrite = true;
        m_pendingWriteAddr = regAddr;
        m_pendingWriteValue = value;
        return;
    }
    sendWriteRequest(regAddr, value);
}

void ModbusSerialClient::sendWriteRequest(int regAddr, quint16 value)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, regAddr, 1);
    unit.setValue(0, value);

    m_pendingReadRequest  = false;
    m_pendingWriteRequest = true;
    m_pendingWriteAddr    = regAddr;
    m_pendingReply = m_client->sendWriteRequest(unit, m_unitId);
    if (!m_pendingReply) {
        const QString err = tr("发送 Modbus 写入失败: %1")
                                .arg(m_client->errorString());
        emit writeFailed(regAddr, err);
        emit communicationError(err);
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
    const bool wasWrite  = m_pendingWriteRequest;
    const int  writeAddr = m_pendingWriteAddr;
    m_pendingReply = nullptr;

    if (reply) {
        if (reply->error() != QModbusDevice::NoError) {
            // Reply-level failures (timeout / protocol error) are reported here
            // so the collector's failCount / offline detection works — Qt only
            // emits errorOccurred for connection-level errors, NOT request
            // timeouts, so there is no double counting to dedupe.
            const QString err = tr("Modbus 响应错误: %1")
                                    .arg(reply->errorString());
            emit communicationError(err);
            if (wasWrite)
                emit writeFailed(writeAddr, err);
        } else if (wasWrite) {
            emit writeSucceeded(writeAddr);
        } else {
            const auto result = reply->result();
            if (result.isValid() && m_pendingReadRequest
                && result.registerType() == QModbusDataUnit::HoldingRegisters)
                emit registersRead(result);
        }
        reply->deleteLater();
    }

    // The bus is free — flush a deferred control write if one was queued.
    if (m_pendingWrite && isConnected()) {
        const int    addr = m_pendingWriteAddr;
        const quint16 val = m_pendingWriteValue;
        m_pendingWrite = false;
        sendWriteRequest(addr, val);
    }
}

void ModbusSerialClient::onErrorOccurred(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError)
        emit communicationError(m_client->errorString());
}
