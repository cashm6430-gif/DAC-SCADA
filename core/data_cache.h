#ifndef DATA_CACHE_H
#define DATA_CACHE_H

#include <QObject>
#include <QHash>
#include <QAbstractTableModel>

/// Key uniquely identifying a data point: (device, register).
struct DataKey {
    int deviceAddr;
    int registerAddr;

    bool operator==(const DataKey &o) const {
        return deviceAddr == o.deviceAddr && registerAddr == o.registerAddr;
    }
};

inline size_t qHash(const DataKey &k, size_t seed = 0) {
    return qHash(k.deviceAddr, seed) ^ qHash(k.registerAddr, seed);
}

// ---------------------------------------------------------------------------
// Table model exposed to the UI
// ---------------------------------------------------------------------------

class DataCacheModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DataCacheModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    void setData(const QList<QPair<DataKey, double>> &rows);
};

// ---------------------------------------------------------------------------
// Main cache
// ---------------------------------------------------------------------------

class DataCache : public QObject
{
    Q_OBJECT

public:
    explicit DataCache(QObject *parent = nullptr);

    DataCacheModel *tableModel() const;

public slots:
    void updateValue(int deviceAddr, int registerAddr, double value);

signals:
    void valueChanged(DataKey key, double value);

private:
    QHash<DataKey, double> m_values;
    DataCacheModel *m_model;
};

#endif // DATA_CACHE_H
