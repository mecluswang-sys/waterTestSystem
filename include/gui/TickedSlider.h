#pragma once

#include <QSlider>

namespace WaterTest
{

    class TickedSlider : public QSlider
    {
        Q_OBJECT

    public:
        explicit TickedSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

    protected:
        void paintEvent(QPaintEvent *event) override;
    };

} // namespace WaterTest
