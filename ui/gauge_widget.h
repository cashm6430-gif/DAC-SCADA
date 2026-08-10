#ifndef GAUGE_WIDGET_H
#define GAUGE_WIDGET_H

#include <QWidget>
#include <QColor>
#include "core/types.h"

/// A single circular dial gauge (colored limit zones + needle + readout).
///
/// Painted entirely with QPainter — no third-party dependency. The dial range
/// is derived from the channel's alarm limits with headroom on both sides so
/// the needle can travel into the danger zones. The pure geometry lives in
/// static helpers so it is unit-testable without instantiating a widget.
class GaugeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GaugeWidget(QWidget *parent = nullptr);

    /// Configure the gauge from a channel (name/unit/limits/color).
    void setChannel(const Channel &ch);

    /// Update the displayed value and schedule a single repaint.
    void setValue(double value);

    /// Pure geometry: derive the dial range from alarm limits.
    /// *dialMin / *dialMax receive the low/high dial ends with 50% headroom.
    static void computeDialRange(double lower, double upper,
                                 double *dialMin, double *dialMax);

    /// Pure geometry: map a value to a needle angle in degrees (Qt convention,
    /// CCW from +x). 135° = dial min (upper-left), 270° = mid (12 o'clock),
    /// 405° = dial max (upper-right). Out-of-range values clamp into range.
    static double angleFor(double value, double dialMin, double dialMax);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_name;
    QString m_unit;
    QColor  m_color = QColor(40, 90, 180);
    double  m_lower  = 0.0;    // 报警下限
    double  m_upper  = 100.0;  // 报警上限
    double  m_dialMin = 0.0;   // 表盘量程低端（含裕量）
    double  m_dialMax = 100.0; // 表盘量程高端（含裕量）
    double  m_value = 0.0;
    bool    m_hasValue = false;
};

#endif // GAUGE_WIDGET_H
