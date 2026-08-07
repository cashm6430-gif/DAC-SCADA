#ifndef SIMULATED_DEVICE_H
#define SIMULATED_DEVICE_H

#include <QObject>
#include <QVector>
#include <QTimer>

/**
 * @brief 模拟下位机：用 QTimer 周期性更新一组"寄存器"，
 *        产生正弦波 / 随机扰动波形，供上位机读取。
 *
 * 设计目的：
 *  - 在没有真实 PLC 时，验证上位机采集、显示、报警、存储整条链路
 *  - 面试现场可运行，演示效果直观
 *
 * 每个寄存器对应一个 WaveConfig，计算公式：
 *   real = base + amplitude * sin(2π * freq * t) + noise
 *   raw  = clamp(round(real * scale), 0, 65535)
 */
class SimulatedDevice : public QObject
{
    Q_OBJECT

public:
    /// 单个寄存器的波形配置
    struct WaveConfig {
        double base      = 50.0;   // 基准值
        double amplitude = 20.0;   // 振幅
        double frequency = 0.1;    // 频率 (Hz)
        double noise     = 0.5;    // 随机噪声幅度
        double scale     = 100.0;  // 寄存器换算：raw = real * scale
    };

    explicit SimulatedDevice(QObject *parent = nullptr);

    // ---- 配置 ----
    void setRegisterCount(int count);
    void setUpdateInterval(int intervalMs);          // 默认 50ms
    void setWaveConfig(int regAddr, const WaveConfig &cfg);

    // ---- 控制 ----
    void start();
    void stop();
    bool isRunning() const;

    // ---- 读取 ----
    quint16 rawRegister(int addr) const;    // 原始寄存器值（整数，供 Modbus 用）
    double  realValue(int addr) const;      // 真实值（浮点，供显示用）
    int     registerCount() const;

signals:
    void registersUpdated();                // 每个周期更新完成后发出

private:
    void tick();

    QTimer              m_timer;
    QVector<quint16>    m_registers;        // 当前寄存器值
    QVector<WaveConfig> m_waveConfigs;      // 每个寄存器的波形配置
    qint64              m_elapsedMs = 0;    // 已运行时间
};

#endif // SIMULATED_DEVICE_H
