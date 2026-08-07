#ifndef SIMULATED_MODBUS_SERVER_H
#define SIMULATED_MODBUS_SERVER_H

#include <QObject>

class QModbusServer;
class SimulatedDevice;

/**
 * @brief 模拟 Modbus TCP 服务端
 *
 * 将 SimulatedDevice 产生的寄存器值暴露为一个标准的 Modbus TCP 服务端，
 * 这样上位机的 Modbus TCP 客户端可以直接连接并读取数据，
 * 完整验证"客户端 → TCP → 服务端 → 寄存器"整条链路。
 *
 * 默认监听 127.0.0.1:1502（Modbus 标准端口 502 需要管理员权限）。
 */
class SimulatedModbusServer : public QObject
{
    Q_OBJECT

public:
    explicit SimulatedModbusServer(QObject *parent = nullptr);
    ~SimulatedModbusServer() override;

    /// 启动监听。成功返回 true。
    bool start(quint16 port = 1502, const QString &host = QStringLiteral("192.168.1.100"));
    void stop();
    bool isListening() const;

    /// 访问内部的数据生成器（可进一步调整波形）。
    SimulatedDevice *device() const;

private:
    void pushRegistersToModbus();

    SimulatedDevice *m_device = nullptr;
    QModbusServer   *m_server = nullptr;
};

#endif // SIMULATED_MODBUS_SERVER_H
