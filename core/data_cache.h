#ifndef DATA_CACHE_H
#define DATA_CACHE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QAbstractTableModel>
#include "types.h"

class DataCache;

// ---------------------------------------------------------------------------
// Table model exposed to the UI — shows the *current device's* channels.
// ---------------------------------------------------------------------------

class DataCacheModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColName   = 0,   // 通道名
        ColValue  = 1,   // 当前真实值
        ColUnit   = 2,   // 单位
        ColStatus = 3,   // 状态（正常/超上限/低于下限）
        ColCount
    };

    explicit DataCacheModel(DataCache *cache, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    void notifyRowsReset();

private:
    DataCache *m_cache;   // non-owning
};

// ---------------------------------------------------------------------------
// Main cache — holds values for ALL devices, exposes the current device.
// ---------------------------------------------------------------------------

class DataCache : public QObject
{
    Q_OBJECT

public:
    explicit DataCache(QObject *parent = nullptr);

    /// Install the full device + channel configuration.
    void setDevices(const QList<DeviceInfo> &devices);

    /// Switch which device the table/curve display.
    void setCurrentDevice(int deviceIndex);
    int  currentDevice() const { return m_currentDevice; }

    const QList<DeviceInfo> &devices() const { return m_devices; }
    const QList<Channel> &currentChannels() const;

    /// Current real value of a register on a specific device.
    double value(int deviceIndex, int regAddr) const;
    bool   hasValue(int deviceIndex, int regAddr) const;

    DataCacheModel *tableModel() const { return m_model; }

public slots:
    void updateValue(int deviceIndex, int regAddr, double value);

signals:
    void devicesChanged();
    void currentDeviceChanged(int deviceIndex);
    void valueChanged(int deviceIndex, int regAddr, double value);

private:
    QList<DeviceInfo> m_devices;
    QHash<int, QHash<int, double>> m_values;  // deviceIndex → (regAddr → value)
    int  m_currentDevice = 0;
    DataCacheModel *m_model;
};

#endif // DATA_CACHE_H
