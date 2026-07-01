#include "inkcanvas.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

InkCanvas::InkCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::Image);
    setOpaquePainting(false);
    setFillColor(Qt::transparent);
    setAntialiasing(m_antialias);
    setMipmap(false);
}

void InkCanvas::setPenWidth(qreal w)
{
    if (qFuzzyCompare(m_penWidth, w)) {
        return;
    }
    m_penWidth = w;
    emit penWidthChanged();
}

void InkCanvas::setColor(const QColor &c)
{
    if (m_color == c) {
        return;
    }
    m_color = c;
    emit colorChanged();
}

void InkCanvas::setAntialias(bool on)
{
    if (m_antialias == on) {
        return;
    }
    m_antialias = on;
    setAntialiasing(on);
    emit antialiasChanged();
    update();
}

void InkCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    setContentsSize(newGeometry.size().toSize());
}

QRectF InkCanvas::segmentBounds(const Segment &s) const
{
    // Half the stroke width, plus a margin that covers the round cap and the
    // extra fringe pixels antialiasing adds.
    const qreal m = s.width / 2.0 + (m_antialias ? 2.0 : 1.0);
    const qreal minX = std::min(s.a.x(), s.b.x()) - m;
    const qreal minY = std::min(s.a.y(), s.b.y()) - m;
    const qreal maxX = std::max(s.a.x(), s.b.x()) + m;
    const qreal maxY = std::max(s.a.y(), s.b.y()) + m;
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void InkCanvas::moveTo(qreal x, qreal y)
{
    m_last = QPointF(x, y);
    m_hasLast = true;
}

void InkCanvas::lineTo(qreal x, qreal y, qreal width)
{
    const QPointF p(x, y);
    if (!m_hasLast) {
        m_last = p;
        m_hasLast = true;
        return;
    }

    const Segment seg{m_last, p, width < 0.0 ? m_penWidth : width};
    m_segments.append(seg);
    m_last = p;

    // Only invalidate the segment's bounding box so the epaper backend refreshes
    // the smallest possible region of the panel.
    update(segmentBounds(seg).toAlignedRect());
}

void InkCanvas::clear()
{
    m_segments.clear();
    m_hasLast = false;
    update();
}

void InkCanvas::eraseAt(qreal x, qreal y, qreal radius)
{
    // Remove every segment that passes within `radius` pixels of (x, y).
    const QPointF p(x, y);
    const qreal r2 = radius * radius;

    QRectF dirty;
    bool any = false;

    auto pointToSegmentDist2 = [](QPointF p, QPointF a, QPointF b) -> qreal {
        const QPointF ab = b - a;
        const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
        if (len2 < 1e-10) {
            QPointF d = p - a;
            return d.x() * d.x() + d.y() * d.y();
        }
        const qreal t = qBound(0.0,
            ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2,
            1.0);
        const QPointF proj(a.x() + t * ab.x(), a.y() + t * ab.y());
        const QPointF d = p - proj;
        return d.x() * d.x() + d.y() * d.y();
    };

    QList<Segment> kept;
    kept.reserve(m_segments.size());

    for (const Segment &seg : m_segments) {
        if (pointToSegmentDist2(p, seg.a, seg.b) < r2) {
            const QRectF sb = segmentBounds(seg);
            dirty = any ? dirty.united(sb) : sb;
            any = true;
        } else {
            kept.append(seg);
        }
    }

    if (any) {
        m_segments = std::move(kept);
        const QRectF eraserRect(x - radius, y - radius, radius * 2, radius * 2);
        dirty = dirty.united(eraserRect);
        update(dirty.adjusted(-1, -1, 1, 1).toAlignedRect());
    }

    m_hasLast = false;
}

void InkCanvas::paint(QPainter *painter)
{
    if (m_segments.isEmpty()) {
        return;
    }

    QRectF clip = painter->clipBoundingRect();
    const bool haveClip = clip.isValid() && !clip.isEmpty();

    painter->setRenderHint(QPainter::Antialiasing, m_antialias);
    QPen pen(m_color);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    for (const Segment &seg : m_segments) {
        if (haveClip && !segmentBounds(seg).intersects(clip)) {
            continue;
        }
        pen.setWidthF(seg.width);
        painter->setPen(pen);
        painter->drawLine(seg.a, seg.b);
    }
}
