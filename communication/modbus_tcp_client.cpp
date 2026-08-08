#include "modbus_tcp_client.h"
#include <QModbusReply>
#include <QVariant>
#include <QDebug>

// ---------------------------------------------------------------------------
// construction / destruction
// ---------------------------------------------------------------------------

ModbusTcpClient::ModbusTcpClient(QObject *parent)
    : QObject(parent)
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

bool ModbusTcpClient::connectTo(const QString &host, quint16 port, int unitId)
{
    if (m_client->state() != QModbusDevice::UnconnectedState)
        m_client->disconnectDevice();

    m_host   = host;
    m_port   = port;
    m_unitId = unitId;

    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, QVariant(port));
    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, QVariant(host));
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
    if (m_pendingReply)
        return; // serialized

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, regAddr, 1);
    unit.setValue(0, value);

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

    if (!reply)
        return;

    if (reply->error() != QModbusDevice::NoError) {
        emit communicationError(tr("Modbus reply error: %1")
                                    .arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    const auto result = reply->result();
    reply->deleteLater();

    if (result.isValid() && result.registerType() == QModbusDataUnit::HoldingRegisters)
        emit registersRead(result);
}

void ModbusTcpClient::onErrorOccurred(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError)
        emit communicationError(m_client->errorString());
}
