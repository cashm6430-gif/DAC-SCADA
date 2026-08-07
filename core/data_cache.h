#ifndef DATA_CACHE_H
#define DATA_CACHE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QAbstractTableModel>
#include "types.h"

class DataCache;

// ---------------------------------------------------------------------------
// Table model exposed to the UI — shows one row per monitored channel.
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

    /// Called by DataCache whenever the value set changes.
    void notifyRowsReset();

private:
    DataCache *m_cache;   // non-owning
};

// ---------------------------------------------------------------------------
// Main cache — single source of truth for current channel values.
// ---------------------------------------------------------------------------

class DataCache : public QObject
{
    Q_OBJECT

public:
    explicit DataCache(QObject *parent = nullptr);

    /// Install the channel configuration (from DataCollector).
    void setChannels(const QList<Channel> &channels);
    const QList<Channel> &channels() const { return m_channels; }

    /// Current real value of a register, or 0 if unknown.
    double value(int regAddr) const;
    bool   hasValue(int regAddr) const;

    DataCacheModel *tableModel() const { return m_model; }

public slots:
    /// Update a channel value (already converted to the real value).
    void updateValue(int regAddr, double value);

signals:
    void valueChanged(int regAddr, double value);
    /// Emitted when the channel configuration is (re)installed.
    void channelsChanged();

private:
    QList<Channel>   m_channels;
    QHash<int, double> m_values;
    DataCacheModel   *m_model;
};

#endif // DATA_CACHE_H
