#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("3DInkPlanner"));
    QApplication::setOrganizationName(QStringLiteral("3DInkPlanner"));

    MainWindow window;
    window.show();

    // 支持命令行直接打开 STL（如: 3DInkPlanner.exe model.stl）。
    if (argc > 1) {
        window.loadStl(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
