#include "data_cache.h"
#include <QColor>

// ---------------------------------------------------------------------------
// DataCacheModel
// ---------------------------------------------------------------------------

DataCacheModel::DataCacheModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int DataCacheModel::rowCount(const QModelIndex &) const { return 0; }
int DataCacheModel::columnCount(const QModelIndex &) const { return 3; }

QVariant DataCacheModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return QString("Device-%1").arg(index.row() + 1);
        case 1: return QString("Reg-%1").arg(index.row());
        case 2: return 0.0;
        }
    }
    return {};
}

QVariant DataCacheModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return tr("Device");
    case 1: return tr("Register");
    case 2: return tr("Value");
    }
    return {};
}

void DataCacheModel::setData(const QList<QPair<DataKey, double>> &rows)
{
    Q_UNUSED(rows)
    // Stub — real implementation would update internal storage and emit
    // dataChanged / layoutChanged.
}

// ---------------------------------------------------------------------------
// DataCache
// ---------------------------------------------------------------------------

DataCache::DataCache(QObject *parent)
    : QObject(parent)
    , m_model(new DataCacheModel(this))
{}

DataCacheModel *DataCache::tableModel() const { return m_model; }

void DataCache::updateValue(int deviceAddr, int registerAddr, double value)
{
    DataKey key{deviceAddr, registerAddr};
    m_values[key] = value;
    emit valueChanged(key, value);
}
