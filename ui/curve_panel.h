#ifndef CURVE_PANEL_H
#define CURVE_PANEL_H

#include <QWidget>
#include <QHash>
#include <QSharedPointer>
#include "core/types.h"

class QCustomPlot;
class QCPGraph;
class QCPAxis;
class QCPAxisTicker;

/// Multi-channel real-time curve panel (QCustomPlot).
///
/// One QCPGraph per monitored channel; the X axis is a scrolling time window
/// of the last N seconds. addPoint() only appends data — repaints are
/// deferred to a short GUI timer (onUiTick) that advances the axis window,
/// prunes expired points and replots once, so a high-rate acquisition is
/// amortized into a single redraw per tick.
class CurvePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CurvePanel(QWidget *parent = nullptr);

    /// Create / replace the graph set from the channel configuration.
    void setChannels(const QList<Channel> &channels);

    /// Append a (time, value) sample to the graph of regAddr.
    void addPoint(int regAddr, double value, qint64 tsMs = 0);

    /// Clear all graph data.
    void clear();

    void setWindowSeconds(int seconds) { m_windowSeconds = seconds; }
    int  windowSeconds() const { return m_windowSeconds; }

private slots:
    void onUiTick();

private:
    QCustomPlot *m_plot = nullptr;
    QCPAxis     *m_axisX = nullptr;
    QCPAxis     *m_axisY = nullptr;
    QSharedPointer<QCPAxisTicker> m_yTicker;   // tick count adapted to axis height
    QHash<int, QCPGraph *> m_graphs;   // regAddr → graph
    int m_windowSeconds = 60;
};

#endif // CURVE_PANEL_H
