#include <QtTest/QtTest>
#include "ui/gauge_widget.h"

// Pure dial-geometry tests (no widget instantiation — QWidget needs a GUI
// application, and QTEST_GUILESS_MAIN only provides a core app). These cover
// the math the painter relies on: dial-range derivation and needle angles.
class TestGauge : public QObject
{
    Q_OBJECT

private slots:
    void dialRangeHeadroom();
    void dialRangeDegenerateLimits();
    void angleMapsMinMidMax();
    void angleClampsOutOfRange();
};

void TestGauge::dialRangeHeadroom()
{
    double lo = 0, hi = 0;
    GaugeWidget::computeDialRange(0.0, 100.0, &lo, &hi);
    // span 100 → 50 % headroom each side.
    QCOMPARE(lo, -50.0);
    QCOMPARE(hi, 150.0);
}

void TestGauge::dialRangeDegenerateLimits()
{
    // Equal (or reversed) limits must still yield a usable, ordered range.
    double lo = 0, hi = 0;
    GaugeWidget::computeDialRange(100.0, 100.0, &lo, &hi);
    QVERIFY(hi > lo);
    QCOMPARE(lo, 50.0);
    QCOMPARE(hi, 150.0);

    GaugeWidget::computeDialRange(0.0, 0.0, &lo, &hi);
    QVERIFY(hi > lo);
    QCOMPARE(lo, -0.5);
    QCOMPARE(hi, 0.5);
}

void TestGauge::angleMapsMinMidMax()
{
    // dial [-50, 150] derived from limits [0, 100].
    QVERIFY(qAbs(GaugeWidget::angleFor(-50, -50, 150) - 135.0) < 1e-9);
    QVERIFY(qAbs(GaugeWidget::angleFor(50, -50, 150) - 270.0) < 1e-9);
    QVERIFY(qAbs(GaugeWidget::angleFor(150, -50, 150) - 405.0) < 1e-9);
}

void TestGauge::angleClampsOutOfRange()
{
    // Out-of-range values clamp to the dial ends (never off the arc).
    QVERIFY(qAbs(GaugeWidget::angleFor(-9999, -50, 150) - 135.0) < 1e-9);
    QVERIFY(qAbs(GaugeWidget::angleFor(9999, -50, 150) - 405.0) < 1e-9);

    // Degenerate range (min == max) must not divide by zero → mid angle.
    QVERIFY(qAbs(GaugeWidget::angleFor(5, 5, 5) - 270.0) < 1e-9);
}

QTEST_GUILESS_MAIN(TestGauge)
#include "test_gauge.moc"
