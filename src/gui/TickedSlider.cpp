#include "gui/TickedSlider.h"

#include <QPainter>
#include <QStyleOptionSlider>
#include <QPen>
#include <QPalette>

namespace WaterTest
{

    TickedSlider::TickedSlider(Qt::Orientation orientation, QWidget *parent)
        : QSlider(orientation, parent)
    {
    }

    void TickedSlider::paintEvent(QPaintEvent *event)
    {
        QSlider::paintEvent(event);

        if (orientation() != Qt::Horizontal)
            return;

        const int minimumValue = minimum();
        const int maximumValue = maximum();
        const int range = maximumValue - minimumValue;
        if (range <= 0)
            return;

        constexpr int kMinorStep = 50;
        constexpr int kMajorStep = 100;

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        if (!groove.isValid())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        QPen pen(palette().color(QPalette::Text));
        pen.setWidth(1);
        painter.setPen(pen);

        const int tickTop = qMin(height() - 10, groove.bottom() + 6);
        const int shortTickBottom = qMin(height() - 6, tickTop + 4);
        const int longTickBottom = qMin(height() - 4, tickTop + 8);

        for (int value = minimumValue; value <= maximumValue; value += kMinorStep)
        {
            const qreal progress = static_cast<qreal>(value - minimumValue) / static_cast<qreal>(range);
            const int x = groove.left() + qRound(progress * groove.width());
            const bool majorTick = (value == minimumValue) || (value == maximumValue) || (((value - minimumValue) % kMajorStep) == 0);
            painter.drawLine(QPointF(x, tickTop), QPointF(x, majorTick ? longTickBottom : shortTickBottom));
        }
    }

} // namespace WaterTest
