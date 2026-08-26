#pragma once

#include <QGraphicsItem>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSvgRenderer>

#include <cmath>

namespace WaterTest
{
    class SvgPipeAnimationDriver : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(qreal offset READ offset WRITE setOffset NOTIFY offsetChanged)

    public:
        explicit SvgPipeAnimationDriver(QObject *parent = nullptr)
            : QObject(parent),
              m_animation(this, "offset")
        {
            m_animation.setStartValue(0.0);
            m_animation.setEndValue(20.0);
            m_animation.setDuration(800);
            m_animation.setLoopCount(-1);
            m_animation.setDirection(QAbstractAnimation::Forward);
            m_animation.setEasingCurve(QEasingCurve::Linear);
            m_animation.start();
        }

        qreal offset() const { return m_offset; }

        void setOffset(qreal offset)
        {
            if (qFuzzyCompare(m_offset, offset))
                return;
            m_offset = offset;
            emit offsetChanged(m_offset);
        }

        void setRunning(bool running)
        {
            if (running)
                m_animation.resume();
            else
                m_animation.setPaused(true);
        }

    signals:
        void offsetChanged(qreal offset);

    private:
        qreal m_offset = 0.0;
        QPropertyAnimation m_animation;
    };

    class SvgPipeFlowItem : public QGraphicsItem
    {
    public:
        SvgPipeFlowItem(const QPointF &start, const QPointF &end, QSvgRenderer *renderer)
            : m_length(std::hypot(end.x() - start.x(), end.y() - start.y())),
              m_renderer(renderer),
              m_animationDriver()
        {
            setPos(start);
            setRotation(std::atan2(end.y() - start.y(), end.x() - start.x()) * 180.0 / M_PI);
            setZValue(3);
            setVisible(false);

            QObject::connect(&m_animationDriver, &SvgPipeAnimationDriver::offsetChanged, [this](qreal offset) {
                m_flowOffset = offset;
                update();
            });
        }

        QRectF boundingRect() const override
        {
            return QRectF(0.0, -7.0, m_length, 14.0);
        }

        void setFlowOffset(qreal offset)
        {
            m_animationDriver.setOffset(offset);
        }

        void setFlowing(bool flowing)
        {
            if (flowing)
                m_animationDriver.setRunning(true);
            else
                m_animationDriver.setRunning(false);
            update();
        }

        void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
        {
            if (!p || !m_renderer || !m_renderer->isValid() || m_length <= 0.01)
                return;

            constexpr qreal tileLength = 100.0;
            const qreal phase = -std::fmod(m_flowOffset, tileLength);
            p->save();
            p->setClipRect(QRectF(0.0, -7.0, m_length, 14.0), Qt::IntersectClip);
            for (qreal x = phase - tileLength; x < m_length + tileLength; x += tileLength)
                m_renderer->render(p, QRectF(x, -7.0, tileLength, 14.0));
            p->restore();
        }

    private:
        qreal m_length;
        qreal m_flowOffset = 0.0;
        QSvgRenderer *m_renderer;
        SvgPipeAnimationDriver m_animationDriver;
    };
}
