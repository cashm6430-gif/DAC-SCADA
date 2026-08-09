#include "curve_panel.h"

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChartView>
#include <QDateTime>
#include <QTimer>
#include <QPen>
#include <limits>
#include <utility>

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

CurvePanel::CurvePanel(QWidget *parent)
    : QChartView(parent)
{
    setRenderHint(QPainter::Antialiasing);

    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->setTitle(tr("实时曲线"));

    m_axisX = new QDateTimeAxis;
    m_axisX->setFormat(QStringLiteral("hh:mm:ss"));
    m_axisX->setTitleText(tr("时间"));
    m_axisX->setRange(QDateTime::currentDateTime().addSecs(-m_windowSeconds),
                      QDateTime::currentDateTime());

    m_axisY = new QValueAxis;
    m_axisY->setTitleText(tr("数值"));
    m_axisY->setRange(0, 100);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    setChart(m_chart);

    // Refresh the scrolling time window periodically.
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CurvePanel::refreshAxis);
    timer->start(500);
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void CurvePanel::setChannels(const QList<Channel> &channels)
{
    // Remove existing series
    const auto keys = m_series.keys();
    for (int regAddr : keys) {
        m_chart->removeSeries(m_series.value(regAddr));
        m_series.value(regAddr)->deleteLater();
    }
    m_series.clear();

    for (const Channel &ch : channels) {
        auto *series = new QLineSeries;
        series->setName(QStringLiteral("%1 [%2]").arg(ch.name, ch.unit));
        series->setColor(ch.color.isValid() ? ch.color : QColor(Qt::green));
        series->setPen(QPen(ch.color.isValid() ? ch.color : QColor(Qt::green), 1.5));

        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
        m_series.insert(ch.regAddr, series);
    }
}

void CurvePanel::addPoint(int regAddr, double value)
{
    auto it = m_series.find(regAddr);
    if (it == m_series.end())
        return;

    QLineSeries *series = it.value();
    const qint64 msec = QDateTime::currentMSecsSinceEpoch();

    series->append(static_cast<double>(msec), value);

    // Drop points older than the window. Count the expired ones in a single
    // pass, then remove them in bulk (a per-point points()/remove(0) loop is
    // O(n²) once the window fills up).
    const double cutoff = static_cast<double>(
        QDateTime::currentMSecsSinceEpoch() - m_windowSeconds * 1000);
    const auto pts = series->points();
    int drop = 0;
    while (drop < pts.size() && pts.at(drop).x() < cutoff)
        ++drop;
    if (drop > 0)
        series->removePoints(0, drop);
}

void CurvePanel::clear()
{
    for (auto *series : std::as_const(m_series))
        series->clear();
}

void CurvePanel::refreshAxis()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_axisX->setRange(now.addSecs(-m_windowSeconds), now);

    // Auto-scale Y to the visible data (fall back to 0..100 if empty)
    bool hasData = false;
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (auto *series : std::as_const(m_series)) {
        const auto pts = series->points();
        for (const QPointF &p : pts) {
            hasData = true;
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
    }

    if (hasData && minY < maxY) {
        const double pad = qMax((maxY - minY) * 0.1, 1.0);
        m_axisY->setRange(minY - pad, maxY + pad);
    } else {
        m_axisY->setRange(0, 100);
    }
}
