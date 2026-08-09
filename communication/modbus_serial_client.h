#ifndef MODBUS_SERIAL_CLIENT_H
#define MODBUS_SERIAL_CLIENT_H

#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include "modbus_client_interface.h"

class QModbusReply;

/// Modbus RTU master driver over an RS232/485 serial port.
///
/// Wraps QModbusRtuSerialClient. One serial bus (COM port) can carry several
/// slave devices addressed by their Modbus unit address; this client is tied
/// to a single COM port and the unitId used for requests is set per connect.
class ModbusSerialClient : public IModbusClient
{
    Q_OBJECT

public:
    explicit ModbusSerialClient(QObject *parent = nullptr);
    ~ModbusSerialClient() override;

    // ---- IModbusClient ----
    bool connectTo(const DeviceInfo &info) override;
    void disconnectFrom() override;
    bool isConnected() const override;
    void readHoldingRegisters(int startAddr, int count) override;
    void writeSingleRegister(int regAddr, quint16 value) override;

    QString portName() const { return m_portName; }
    int     unitId() const   { return m_unitId; }

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onReplyFinished();
    void onErrorOccurred(QModbusDevice::Error error);

private:
    void sendReadRequest(int startAddr, int count);
    void sendWriteRequest(int regAddr, quint16 value);

    QModbusRtuSerialClient *m_client = nullptr;
    QString  m_portName;
    int      m_baudRate = 9600;
    int      m_unitId = 1;
    QModbusReply *m_pendingReply = nullptr;
    /// What the in-flight request was — a write echo must not be reported as
    /// register data (it would feed the cache/alarm engine with the written
    /// value as if it were a fresh reading).
    bool m_pendingReadRequest = false;
    /// Deferred control write (see ModbusTcpClient for details).
    bool     m_pendingWrite = false;
    int      m_pendingWriteAddr = 0;
    quint16  m_pendingWriteValue = 0;
};

#endif // MODBUS_SERIAL_CLIENT_H
