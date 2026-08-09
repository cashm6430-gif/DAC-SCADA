#ifndef MODBUS_TCP_CLIENT_H
#define MODBUS_TCP_CLIENT_H

#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include "modbus_client_interface.h"

class QModbusReply;

/// Modbus TCP master driver — wraps QModbusTcpClient.
///
/// Connects to a Modbus TCP server (e.g. the built-in SimulatedModbusServer
/// or a real PLC) and issues read/write requests. All requests are
/// serialized internally (one at a time) to keep Modbus semantics safe.
class ModbusTcpClient : public IModbusClient
{
    Q_OBJECT

public:
    explicit ModbusTcpClient(QObject *parent = nullptr);
    ~ModbusTcpClient() override;

    // ---- IModbusClient ----
    bool connectTo(const DeviceInfo &info) override;
    void disconnectFrom() override;
    bool isConnected() const override;
    void readHoldingRegisters(int startAddr, int count) override;
    void writeSingleRegister(int regAddr, quint16 value) override;

    QString host() const   { return m_host; }
    quint16 port() const   { return m_port; }
    int     unitId() const { return m_unitId; }

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onReplyFinished();
    void onErrorOccurred(QModbusDevice::Error error);

private:
    void sendReadRequest(int startAddr, int count);

    QModbusTcpClient *m_client = nullptr;
    QString  m_host;
    quint16  m_port   = 502;
    int      m_unitId = 1;
    QModbusReply *m_pendingReply = nullptr;
};

#endif // MODBUS_TCP_CLIENT_H
