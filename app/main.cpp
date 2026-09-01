#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("3DInkPlanner"));
    QApplication::setOrganizationName(QStringLiteral("3DInkPlanner"));

    MainWindow window;
    window.show();

    return app.exec();
}
