#ifndef MODBUS_CLIENT_INTERFACE_H
#define MODBUS_CLIENT_INTERFACE_H

#include <QObject>
#include <QString>
#include <QModbusDataUnit>
#include "core/types.h"

/// Abstract Modbus client — implemented by both the TCP and the serial
/// (Modbus RTU) drivers. The acquisition layer (DataCollector) programs
/// against this interface, so adding a new transport needs no changes to
/// the upper layers.
class IModbusClient : public QObject
{
    Q_OBJECT

public:
    explicit IModbusClient(QObject *parent = nullptr) : QObject(parent) {}
    ~IModbusClient() override = default;

    // ---- lifecycle ----
    /// Establish the link described by \a info (ip:port for TCP,
    /// serialPort/baudRate for serial). Returns false if the link could not
    /// be started.
    virtual bool connectTo(const DeviceInfo &info) = 0;
    virtual void disconnectFrom() = 0;
    virtual bool isConnected() const = 0;

    // ---- Modbus operations ----
    virtual void readHoldingRegisters(int startAddr, int count) = 0;
    virtual void writeSingleRegister(int regAddr, quint16 value) = 0;

signals:
    void connectionStateChanged(bool connected);
    void registersRead(const QModbusDataUnit &unit);
    void communicationError(const QString &message);
};

#endif // MODBUS_CLIENT_INTERFACE_H
