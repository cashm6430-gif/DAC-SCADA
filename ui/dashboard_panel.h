#ifndef DASHBOARD_PANEL_H
#define DASHBOARD_PANEL_H

#include <QWidget>
#include <QHash>
#include "core/types.h"

class QLabel;
class QGridLayout;
class GaugeWidget;

/// Dashboard — a grid of circular gauges, one per channel of the currently
/// selected device. Shares the same data path as the curve panel: configured
/// on device/config change, fed by DataCache::valueChanged.
class DashboardPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPanel(QWidget *parent = nullptr);

    /// Rebuild the gauge grid for a device's channels.
    void setChannels(const QList<Channel> &channels, const QString &deviceName);

    /// Update one gauge by register address (ignored if not currently shown).
    void updateValue(int regAddr, double value);

private:
    QLabel      *m_title = nullptr;
    QWidget     *m_gridHost = nullptr;
    QGridLayout *m_grid = nullptr;
    QHash<int, GaugeWidget *> m_gauges;   // regAddr → gauge
};

#endif // DASHBOARD_PANEL_H
