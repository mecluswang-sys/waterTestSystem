#pragma once

#include <QWidget>
#include <QTimer>

class PumpWidget : public QWidget {
    Q_OBJECT

public:
    explicit PumpWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer* m_timer;
    float m_rotationAngle; // 控制叶轮旋转的角度
};