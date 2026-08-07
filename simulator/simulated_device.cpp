#include "simulated_device.h"

#include <QRandomGenerator>
#include <QtMath>

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

SimulatedDevice::SimulatedDevice(QObject *parent)
    : QObject(parent)
{
    setRegisterCount(8);           // 默认 8 个寄存器
    m_timer.setInterval(50);       // 默认 20 Hz
    connect(&m_timer, &QTimer::timeout, this, &SimulatedDevice::tick);
}

// ---------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------

void SimulatedDevice::setRegisterCount(int count)
{
    if (count <= 0)
        return;

    m_registers.fill(0, count);
    m_waveConfigs.clear();

    for (int i = 0; i < count; ++i) {
        WaveConfig cfg;
        cfg.base      = 50.0;
        cfg.amplitude = 20.0;
        cfg.frequency = 0.1 + i * 0.03;   // 每通道频率略不同，便于区分
        cfg.noise     = 0.5;
        cfg.scale     = 100.0;
        m_waveConfigs.append(cfg);
    }
}

void SimulatedDevice::setUpdateInterval(int intervalMs)
{
    if (intervalMs > 0)
        m_timer.setInterval(intervalMs);
}

void SimulatedDevice::setWaveConfig(int regAddr, const WaveConfig &cfg)
{
    if (regAddr >= 0 && regAddr < m_waveConfigs.size())
        m_waveConfigs[regAddr] = cfg;
}

// ---------------------------------------------------------------------------
// control
// ---------------------------------------------------------------------------

void SimulatedDevice::start()
{
    m_elapsedMs = 0;
    m_timer.start();
}

void SimulatedDevice::stop()
{
    m_timer.stop();
}

bool SimulatedDevice::isRunning() const
{
    return m_timer.isActive();
}

// ---------------------------------------------------------------------------
// read access
// ---------------------------------------------------------------------------

quint16 SimulatedDevice::rawRegister(int addr) const
{
    if (addr < 0 || addr >= m_registers.size())
        return 0;
    return m_registers[addr];
}

double SimulatedDevice::realValue(int addr) const
{
    if (addr < 0 || addr >= m_registers.size())
        return 0.0;
    return m_registers[addr] / m_waveConfigs[addr].scale;
}

int SimulatedDevice::registerCount() const
{
    return m_registers.size();
}

// ---------------------------------------------------------------------------
// internal: advance the simulation one tick
// ---------------------------------------------------------------------------

void SimulatedDevice::tick()
{
    m_elapsedMs += m_timer.interval();
    const double t = m_elapsedMs / 1000.0;

    for (int i = 0; i < m_registers.size(); ++i) {
        const WaveConfig &cfg = m_waveConfigs[i];

        const double phase = 2.0 * M_PI * cfg.frequency * t;

        // 正弦波 + 随机噪声（模拟真实信号抖动）
        const double noise =
            (QRandomGenerator::global()->generateDouble() - 0.5) * 2.0 * cfg.noise;
        double real = cfg.base + cfg.amplitude * std::sin(phase) + noise;

        // 换算成寄存器整数（钳位到 16 位无符号范围）
        double raw = real * cfg.scale;
        if (raw < 0.0)
            raw = 0.0;
        if (raw > 65535.0)
            raw = 65535.0;
        m_registers[i] = static_cast<quint16>(std::round(raw));
    }

    emit registersUpdated();
}
