#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QQuickPaintedItem>

// Ink surface built on QQuickPaintedItem — the draw path the reMarkable epaper
// scene-graph backend is designed for (EPContext::createPainterNode) and the one
// the native handwriting UI uses. Unlike a QML Canvas (which renders to an
// offscreen FBO/texture and never hits the epaper painter node), a
// QQuickPaintedItem maps to that node, so each per-segment update(smallRect)
// refreshes just that tiny region of the e-ink panel, matching native
// handwriting speed.
//
// Pair it in QML with a ScreenModeItem in Pen mode (import xofm.libs.epaper) to
// select the fast handwriting waveform for the same region.
//
// Registered by the AppLoad xovi extension as `import net.asivery.Ink 1.0`.
class InkCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(bool antialias READ antialias WRITE setAntialias NOTIFY antialiasChanged)

public:
    explicit InkCanvas(QQuickItem *parent = nullptr);

    qreal penWidth() const { return m_penWidth; }
    void setPenWidth(qreal w);

    QColor color() const { return m_color; }
    void setColor(const QColor &c);

    bool antialias() const { return m_antialias; }
    void setAntialias(bool on);

    Q_INVOKABLE void moveTo(qreal x, qreal y);
    // width < 0 falls back to penWidth; pass a per-stroke width (e.g. derived
    // from pen pressure) to vary line thickness within a stroke.
    Q_INVOKABLE void lineTo(qreal x, qreal y, qreal width = -1.0);
    Q_INVOKABLE void eraseAt(qreal x, qreal y, qreal radius);
    Q_INVOKABLE void clear();

    void paint(QPainter *painter) override;

signals:
    void penWidthChanged();
    void colorChanged();
    void antialiasChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Segment {
        QPointF a;
        QPointF b;
        qreal width;
    };

    QRectF segmentBounds(const Segment &s) const;

    qreal m_penWidth = 3.0;
    QColor m_color = Qt::black;
    bool m_antialias = true;

    QList<Segment> m_segments;
    QPointF m_last;
    bool m_hasLast = false;
};
