#include "PumpWidget.h"

#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("PumpWidget 预览"));
    window.resize(960, 640);

    auto *pumpWidget = new PumpWidget(&window);
    window.setCentralWidget(pumpWidget);
    window.show();

    return app.exec();
}