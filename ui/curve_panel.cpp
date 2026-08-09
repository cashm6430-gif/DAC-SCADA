#include "curve_panel.h"

#include <qcustomplot.h>
#include <QTimer>
#include <QVBoxLayout>
#include <QPen>
#include <QBrush>
#include <QDateTime>
#include <limits>
#include <utility>

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

CurvePanel::CurvePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    // X axis = seconds since epoch, rendered as clock time (date-time ticker).
    m_axisX = m_plot->xAxis;
    QSharedPointer<QCPAxisTickerDateTime> ticker(new QCPAxisTickerDateTime);
    ticker->setDateTimeFormat(QStringLiteral("hh:mm:ss"));
    m_axisX->setTicker(ticker);
    m_axisX->setLabel(tr("时间"));
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_axisX->setRange(now - m_windowSeconds, now);

    m_axisY = m_plot->yAxis;
    m_axisY->setLabel(tr("数值"));
    m_axisY->setRange(0, 100);

    m_plot->legend->setVisible(true);
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 220)));
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // Refresh the scrolling time window on a short tick.
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CurvePanel::onUiTick);
    timer->start(50);
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void CurvePanel::setChannels(const QList<Channel> &channels)
{
    // Remove existing graphs (this also unregisters their legend items).
    const auto keys = m_graphs.keys();
    for (int regAddr : keys)
        m_plot->removeGraph(m_graphs.value(regAddr));
    m_graphs.clear();

    for (const Channel &ch : channels) {
        m_plot->addGraph();
        QCPGraph *graph = m_plot->graph(m_plot->graphCount() - 1);
        const QColor color = ch.color.isValid() ? ch.color : QColor(Qt::green);
        graph->setName(QStringLiteral("%1 [%2]").arg(ch.name, ch.unit));
        graph->setPen(QPen(color, 1.5));
        m_graphs.insert(ch.regAddr, graph);
    }

    m_plot->replot();
}

void CurvePanel::addPoint(int regAddr, double value, qint64 tsMs)
{
    // Defensive: callers that don't carry a real timestamp (tsMs == 0) fall
    // back to "now" so the point lands inside the scrolling window.
    if (tsMs <= 0)
        tsMs = QDateTime::currentMSecsSinceEpoch();

    const auto it = m_graphs.constFind(regAddr);
    if (it == m_graphs.constEnd())
        return;
    it.value()->addData(tsMs / 1000.0, value);   // QCPGraph X = seconds
}

void CurvePanel::clear()
{
    for (auto *graph : std::as_const(m_graphs))
        graph->data()->clear();
    m_plot->replot();
}

// ---------------------------------------------------------------------------
// private slots
// ---------------------------------------------------------------------------

void CurvePanel::onUiTick()
{
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    // Scroll the visible time window.
    m_axisX->setRange(now - m_windowSeconds, now);

    // Drop points older than the window so the buffers stay bounded.
    const double cutoff = now - m_windowSeconds - 1.0;
    for (auto *graph : std::as_const(m_graphs))
        graph->data()->removeBefore(cutoff);

    // Auto-scale Y to the visible data (fall back to 0..100 if empty).
    bool hasData = false;
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (auto *graph : std::as_const(m_graphs)) {
        auto it = graph->data()->findBegin(now - m_windowSeconds);
        const auto end = graph->data()->constEnd();
        for (; it != end; ++it) {
            hasData = true;
            minY = qMin(minY, it->value);
            maxY = qMax(maxY, it->value);
        }
    }

    if (hasData && minY < maxY) {
        const double pad = qMax((maxY - minY) * 0.1, 1.0);
        m_axisY->setRange(minY - pad, maxY + pad);
    } else {
        m_axisY->setRange(0, 100);
    }

    // Batch the repaint to the next event-loop pass.
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}
