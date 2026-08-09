#include "modbus_tcp_client.h"
#include <QModbusReply>
#include <QVariant>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

ModbusTcpClient::ModbusTcpClient(QObject *parent)
    : IModbusClient(parent)
{
    m_client = new QModbusTcpClient(this);

    connect(m_client, &QModbusClient::stateChanged,
            this, &ModbusTcpClient::onStateChanged);
    connect(m_client, &QModbusClient::errorOccurred,
            this, &ModbusTcpClient::onErrorOccurred);
}

ModbusTcpClient::~ModbusTcpClient()
{
    disconnectFrom();
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

bool ModbusTcpClient::connectTo(const DeviceInfo &info)
{
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();

    m_host   = info.ip;
    m_port   = info.port;
    m_unitId = info.address;

    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, QVariant(m_port));
    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, QVariant(m_host));
    m_client->setTimeout(1000);
    m_client->setNumberOfRetries(1);

    return m_client->connectDevice();
}

void ModbusTcpClient::disconnectFrom()
{
    if (m_pendingReply) {
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    m_pendingWrite = false;   // a stale deferred write must not cross a reconnect
    m_client->disconnectDevice();
}

bool ModbusTcpClient::isConnected() const
{
    return m_client->state() == QModbusDevice::ConnectedState;
}

// ---------------------------------------------------------------------------
// Modbus operations
// ---------------------------------------------------------------------------

void ModbusTcpClient::readHoldingRegisters(int startAddr, int count)
{
    if (!isConnected()) {
        emit communicationError(tr("Not connected to Modbus server"));
        return;
    }

    // Serialize: if a request is already in flight, drop this one.
    // The poller re-issues on each tick, so skipping is safe.
    if (m_pendingReply)
        return;

    sendReadRequest(startAddr, count);
}

void ModbusTcpClient::sendReadRequest(int startAddr, int count)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddr, count);

    m_pendingReadRequest = true;
    m_pendingReply = m_client->sendReadRequest(unit, m_unitId);
    if (!m_pendingReply) {
        emit communicationError(tr("Failed to send Modbus request: %1")
                                    .arg(m_client->errorString()));
        return;
    }

    connect(m_pendingReply, &QModbusReply::finished,
            this, &ModbusTcpClient::onReplyFinished);
}

void ModbusTcpClient::writeSingleRegister(int regAddr, quint16 value)
{
    if (!isConnected()) {
        emit communicationError(tr("Not connected to Modbus server"));
        return;
    }
    if (m_pendingReply) {
        // The bus is busy with a read — the poller re-issues reads every tick,
        // but a user's remote-control write must NOT be dropped. Stash it and
        // flush it as soon as the current request completes.
        m_pendingWrite = true;
        m_pendingWriteAddr = regAddr;
        m_pendingWriteValue = value;
        return;
    }
    sendWriteRequest(regAddr, value);
}

void ModbusTcpClient::sendWriteRequest(int regAddr, quint16 value)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, regAddr, 1);
    unit.setValue(0, value);

    m_pendingReadRequest = false;
    m_pendingReply = m_client->sendWriteRequest(unit, m_unitId);
    if (!m_pendingReply) {
        emit communicationError(tr("Failed to send Modbus write: %1")
                                    .arg(m_client->errorString()));
        return;
    }
    connect(m_pendingReply, &QModbusReply::finished,
            this, &ModbusTcpClient::onReplyFinished);
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void ModbusTcpClient::onStateChanged(QModbusDevice::State state)
{
    const bool connected = (state == QModbusDevice::ConnectedState);
    qInfo() << "ModbusTcpClient::onStateChanged" << static_cast<int>(state)
            << "connected=" << connected << m_host << m_port;
    emit connectionStateChanged(connected);
}

void ModbusTcpClient::onReplyFinished()
{
    auto *reply = m_pendingReply;
    m_pendingReply = nullptr;

    if (reply) {
        if (reply->error() != QModbusDevice::NoError) {
            emit communicationError(tr("Modbus reply error: %1")
                                        .arg(reply->errorString()));
        } else {
            const auto result = reply->result();
            if (result.isValid() && m_pendingReadRequest
                && result.registerType() == QModbusDataUnit::HoldingRegisters)
                emit registersRead(result);
        }
        reply->deleteLater();
    }

    // The bus is free — flush a deferred control write if one was queued while
    // the previous request was in flight.
    if (m_pendingWrite && isConnected()) {
        const int    addr = m_pendingWriteAddr;
        const quint16 val = m_pendingWriteValue;
        m_pendingWrite = false;
        sendWriteRequest(addr, val);
    }
}

void ModbusTcpClient::onErrorOccurred(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError)
        emit communicationError(m_client->errorString());
}
