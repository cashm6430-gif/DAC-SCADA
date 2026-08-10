#include "gauge_widget.h"

#include <QPainter>
#include <QPen>
#include <QtMath>
#include <cmath>

// Dial sweep in Qt angle convention (CCW from +x): 135°..405° covers 270°, so
// the min sits upper-left, the mid sits at 12 o'clock (270°), the max
// upper-right — the classic open-bottom gauge look.
static constexpr double kStartAngle = 135.0;
static constexpr double kSweepAngle = 270.0;

// ---------------------------------------------------------------------------
// pure geometry (unit-tested without instantiating the widget)
// ---------------------------------------------------------------------------

void GaugeWidget::computeDialRange(double lower, double upper,
                                   double *dialMin, double *dialMax)
{
    double span = upper - lower;
    if (span < 1e-6)
        span = qMax(qAbs(upper), 1.0);   // degenerate / reversed limits
    *dialMin = lower - span * 0.5;
    *dialMax = upper + span * 0.5;
}

double GaugeWidget::angleFor(double value, double dialMin, double dialMax)
{
    const double denom = dialMax - dialMin;
    double f = (denom != 0.0) ? (value - dialMin) / denom : 0.5;
    f = qBound(0.0, f, 1.0);
    return kStartAngle + f * kSweepAngle;
}

// ---------------------------------------------------------------------------
// construction / public API
// ---------------------------------------------------------------------------

GaugeWidget::GaugeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(110, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize GaugeWidget::sizeHint() const { return QSize(150, 160); }
QSize GaugeWidget::minimumSizeHint() const { return QSize(110, 120); }

void GaugeWidget::setChannel(const Channel &ch)
{
    m_name  = ch.name;
    m_unit  = ch.unit;
    m_color = ch.color.isValid() ? ch.color : QColor(40, 90, 180);
    m_lower = ch.lowerLimit;
    m_upper = ch.upperLimit;
    computeDialRange(m_lower, m_upper, &m_dialMin, &m_dialMax);
    m_hasValue = false;
    update();
}

void GaugeWidget::setValue(double value)
{
    m_value = value;
    m_hasValue = true;
    update();
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------

void GaugeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    if (w < 40 || h < 40)
        return;

    // The gauge circle is centered slightly above middle; the bottom band
    // (24 px) is reserved for the channel name.
    const int side = qMin(w, h - 24);
    const QPointF center(w / 2.0, (h - 24) / 2.0);
    const qreal ring = (side - 20) / 2.0;    // zone-ring radius
    if (ring < 8.0)
        return;
    const qreal ringW = 10.0;
    const QRectF ringRect(center.x() - ring, center.y() - ring,
                          2 * ring, 2 * ring);

    // ---- colored limit zones (background, below-limit, normal, above-limit) ----
    const auto drawZone = [&](const QColor &color, double fromDeg, double toDeg) {
        if (toDeg <= fromDeg)
            return;
        p.setPen(QPen(color, ringW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(ringRect, qRound(fromDeg * 16.0),
                  qRound((toDeg - fromDeg) * 16.0));
    };

    const double endDeg = kStartAngle + kSweepAngle;
    drawZone(QColor(214, 214, 214), kStartAngle, endDeg);          // 底色
    const double lowDeg  = angleFor(m_lower, m_dialMin, m_dialMax);
    const double highDeg = angleFor(m_upper, m_dialMin, m_dialMax);
    drawZone(QColor(230, 190, 60), kStartAngle, lowDeg);           // 低于下限（琥珀）
    drawZone(QColor(96, 176, 96), lowDeg, highDeg);                // 正常（绿）
    drawZone(QColor(214, 74, 74), highDeg, endDeg);                // 超上限（红）

    // ---- tick marks: minor every 5 %, major every 10 % ----
    const qreal rInner = ring - ringW / 2.0;
    p.setPen(QPen(QColor(110, 110, 110), 1.0, Qt::SolidLine, Qt::FlatCap));
    for (int i = 0; i <= 20; ++i) {
        const double rad =
            qDegreesToRadians(kStartAngle + (i / 20.0) * kSweepAngle);
        const bool major = (i % 2 == 0);
        const qreal r1 = rInner - (major ? 4.0 : 2.0);
        const qreal r2 = rInner - (major ? 11.0 : 7.0);
        const QPointF p1(center.x() + r1 * std::cos(rad),
                         center.y() + r1 * std::sin(rad));
        const QPointF p2(center.x() + r2 * std::cos(rad),
                         center.y() + r2 * std::sin(rad));
        p.drawLine(p1, p2);
    }

    // ---- numeric labels at min / mid / max ----
    QFont lf = font();
    lf.setPointSizeF(qMax(lf.pointSizeF() - 2.0, 7.0));
    p.setFont(lf);
    p.setPen(QColor(70, 70, 70));
    for (double f : {0.0, 0.5, 1.0}) {
        const double val = m_dialMin + f * (m_dialMax - m_dialMin);
        const double rad = qDegreesToRadians(kStartAngle + f * kSweepAngle);
        const qreal rl = ring + 6.0;
        const QPointF lp(center.x() + rl * std::cos(rad),
                         center.y() + rl * std::sin(rad));
        const QString text =
            QString::number(val, 'f', (qAbs(val) < 100.0 ? 1 : 0));
        p.drawText(QRectF(lp.x() - 24.0, lp.y() - 8.0, 48.0, 16.0),
                   Qt::AlignCenter, text);
    }

    // ---- needle + center hub ----
    const double needleVal =
        m_hasValue ? m_value : (m_dialMin + m_dialMax) / 2.0;   // 无数据时指针居中
    const double rad = qDegreesToRadians(angleFor(needleVal, m_dialMin, m_dialMax));
    const qreal nLen = rInner - 6.0;
    const QPointF tip(center.x() + nLen * 0.76 * std::cos(rad),
                      center.y() + nLen * 0.76 * std::sin(rad));
    p.setPen(QPen(m_color, 3.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(center, tip);
    p.setBrush(m_color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, 5.0, 5.0);

    // ---- digital readout (value + unit) ----
    const QString valText = m_hasValue
        ? QString::number(m_value, 'f', 1)
        : QStringLiteral("--");
    QFont vf = font();
    vf.setBold(true);
    vf.setPointSizeF(qMax(vf.pointSizeF() + 1.0, 11.0));
    p.setFont(vf);
    p.setPen(m_color);
    const qreal roY = center.y() + ring * 0.22;
    p.drawText(QRectF(0.0, roY, w, 20.0), Qt::AlignHCenter | Qt::AlignVCenter,
               valText);

    QFont uf = font();
    uf.setPointSizeF(qMax(uf.pointSizeF() - 2.0, 8.0));
    p.setFont(uf);
    p.setPen(QColor(90, 90, 90));
    p.drawText(QRectF(0.0, roY + 18.0, w, 14.0),
               Qt::AlignHCenter | Qt::AlignVCenter, m_unit);

    // ---- channel name ----
    QFont nf = font();
    nf.setBold(true);
    nf.setPointSizeF(qMax(nf.pointSizeF() - 1.0, 9.0));
    p.setFont(nf);
    p.setPen(QColor(40, 40, 40));
    p.drawText(QRectF(0.0, h - 22.0, w, 20.0), Qt::AlignCenter, m_name);
}
