#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);
    a.setStyleSheet(
        /* Вертикальный скроллбар */
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 8px;"
        "  margin: 4px 2px 4px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(0, 0, 0, 55);"
        "  min-height: 28px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: rgba(0, 0, 0, 105);"
        "}"
        "QScrollBar::handle:vertical:pressed {"
        "  background: rgba(0, 0, 0, 140);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "  background: none;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"

        /* Горизонтальный скроллбар */
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 8px;"
        "  margin: 0px 4px 2px 4px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: rgba(0, 0, 0, 55);"
        "  min-width: 28px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: rgba(0, 0, 0, 105);"
        "}"
        "QScrollBar::handle:horizontal:pressed {"
        "  background: rgba(0, 0, 0, 140);"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0px;"
        "  background: none;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: none;"
        "}"
    );

    MainWindow w;
    w.show();
    return a.exec();
}
