#ifndef CURVE_PANEL_H
#define CURVE_PANEL_H

#include <QChartView>
#include <QHash>
#include <QList>
#include "core/types.h"

QT_BEGIN_NAMESPACE
class QLineSeries;
class QChart;
class QDateTimeAxis;
class QValueAxis;
QT_END_NAMESPACE

/// Multi-channel real-time curve panel.
///
/// One QLineSeries per monitored channel; the X axis is a scrolling time
/// window (last N seconds). New points are appended on the GUI thread via
/// addPoint(), the axis window is refreshed on a short timer.
class CurvePanel : public QChartView
{
    Q_OBJECT

public:
    explicit CurvePanel(QWidget *parent = nullptr);

    /// Create / replace the series set from the channel configuration.
    void setChannels(const QList<Channel> &channels);

    /// Append a (time, value) sample to the series of regAddr.
    void addPoint(int regAddr, double value);

    /// Clear all series data.
    void clear();

    void setWindowSeconds(int seconds) { m_windowSeconds = seconds; }
    int  windowSeconds() const { return m_windowSeconds; }

private slots:
    void refreshAxis();

private:
    QChart        *m_chart;
    QDateTimeAxis *m_axisX;
    QValueAxis    *m_axisY;
    QHash<int, QLineSeries *> m_series;   // regAddr → series
    int m_windowSeconds = 60;
};

#endif // CURVE_PANEL_H
