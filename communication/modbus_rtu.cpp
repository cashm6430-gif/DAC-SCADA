#include "modbus_rtu.h"
#include "serialport_comm.h"
#include <QVariant>

ModbusRtu::ModbusRtu(SerialPortComm *serial, QObject *parent)
    : CommInterface(parent)
    , m_client(new QModbusRtuSerialClient(this))
    , m_serial(serial)
{
    connect(m_client, &QModbusRtuSerialClient::stateChanged,
            this, [this](QModbusDevice::State state) {
                emit stateChanged(state == QModbusDevice::ConnectedState);
            });
}

ModbusRtu::~ModbusRtu() { close(); }

// ---------------------------------------------------------------------------
// CommInterface
// ---------------------------------------------------------------------------

bool ModbusRtu::open()
{
    if (!m_serial || !m_serial->isOpen())
        return false;

    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                     m_serial->portName());
    // Bypass QModbusClient's own port management; piggyback on our serial.
    return m_client->connectDevice();
}

void ModbusRtu::close()
{
    if (m_client->state() == QModbusDevice::ConnectedState)
        m_client->disconnectDevice();
}

bool ModbusRtu::isOpen() const
{
    return m_client->state() == QModbusDevice::ConnectedState;
}

qint64 ModbusRtu::write(const QByteArray &data)
{
    // Raw-write path delegated to the underlying serial port.
    return m_serial ? m_serial->write(data) : -1;
}

QString ModbusRtu::name() const
{
    return QString("ModbusRTU[%1]").arg(m_serial ? m_serial->portName() : "?");
}

// ---------------------------------------------------------------------------
// Modbus operations
// ---------------------------------------------------------------------------

void ModbusRtu::readHoldingRegisters(int deviceAddr, int startAddr, int count)
{
    if (!isOpen()) return;

    auto *reply = m_client->sendReadRequest(
        QModbusDataUnit(QModbusDataUnit::HoldingRegisters, startAddr, count),
        deviceAddr);

    if (!reply) return;
    connect(reply, &QModbusReply::finished, this, &ModbusRtu::onReadReady);
}

void ModbusRtu::readInputRegisters(int deviceAddr, int startAddr, int count)
{
    if (!isOpen()) return;

    auto *reply = m_client->sendReadRequest(
        QModbusDataUnit(QModbusDataUnit::InputRegisters, startAddr, count),
        deviceAddr);

    if (!reply) return;
    connect(reply, &QModbusReply::finished, this, &ModbusRtu::onReadReady);
}

void ModbusRtu::readCoils(int deviceAddr, int startAddr, int count)
{
    if (!isOpen()) return;

    auto *reply = m_client->sendReadRequest(
        QModbusDataUnit(QModbusDataUnit::Coils, startAddr, count),
        deviceAddr);

    if (!reply) return;
    connect(reply, &QModbusReply::finished, this, &ModbusRtu::onReadReady);
}

void ModbusRtu::writeSingleRegister(int deviceAddr, int regAddr, quint16 value)
{
    if (!isOpen()) return;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, regAddr, {value});
    auto *reply = m_client->sendWriteRequest(unit, deviceAddr);

    if (!reply) return;
    connect(reply, &QModbusReply::finished, this, &ModbusRtu::onWriteReady);
}

void ModbusRtu::writeMultipleRegisters(int deviceAddr, int startAddr,
                                       const QVector<quint16> &values)
{
    if (!isOpen()) return;

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, startAddr, values);
    auto *reply = m_client->sendWriteRequest(unit, deviceAddr);

    if (!reply) return;
    connect(reply, &QModbusReply::finished, this, &ModbusRtu::onWriteReady);
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void ModbusRtu::onReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QModbusDevice::NoError) {
        emit modbusError(reply->serverAddress(), reply->errorString());
        return;
    }

    emit registersRead(reply->serverAddress(), reply->result());
}

void ModbusRtu::onWriteReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QModbusDevice::NoError) {
        emit modbusError(reply->serverAddress(), reply->errorString());
        return;
    }

    // serverAddress() on a write-reply may be unreliable; pass 0.
    emit registerWritten(0, 0);
}
