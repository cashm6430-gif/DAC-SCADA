#ifndef SIMULATED_SERIAL_SERVER_H
#define SIMULATED_SERIAL_SERVER_H

#include <QObject>
#include <QHash>

class QModbusRtuSerialServer;
class SimulatedDevice;

/// Simulated Modbus RTU slave on a serial port.
///
/// Uses the same SimulatedDevice waveform generator as the TCP simulator but
/// exposes the registers over RS232/485 (via QModbusRtuSerialServer). It must
/// listen on the *peer* port of a virtual-serial-pair (e.g. com0com creates
/// COM5<->COM6; the host computer connects to COM5, this server opens COM6).
class SimulatedSerialServer : public QObject
{
    Q_OBJECT

public:
    explicit SimulatedSerialServer(QObject *parent = nullptr);
    ~SimulatedSerialServer() override;

    /// Start serving on \a portName (e.g. "COM6"). Returns true on success.
    bool start(const QString &portName, int baudRate = 9600);
    void stop();
    bool isListening() const;

    /// Access the internal data generator (tweak waveforms, etc.).
    SimulatedDevice *device() const;

private:
    void pushRegistersToModbus();

    SimulatedDevice        *m_device = nullptr;
    QModbusRtuSerialServer *m_server = nullptr;
    /// Host-written registers hold their value until the server restarts.
    QHash<int, quint16> m_overrides;
    /// Reentrancy guard — setData() emits dataWritten(); see TCP simulator.
    bool m_pushing = false;
};

#endif // SIMULATED_SERIAL_SERVER_H
