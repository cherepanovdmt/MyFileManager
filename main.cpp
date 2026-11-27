#include "mainwindow.h"
#include <QApplication>
#include <QScreen>
#include <private/qabstractanimation_p.h>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>

#endif

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);
    int refreshHz = 60;
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        refreshHz = qRound(scr->refreshRate());
    }
    // Для 200 Гц -> 5 мс (200 FPS), для 144 Гц -> 7 мс, для 60 Гц -> 16 мс
    int frameIntervalMs = qBound(4, qRound(1000.0 / qMax(60, refreshHz)), 16);
    QUnifiedTimer::instance()->setTimingInterval(frameIntervalMs);


#ifdef Q_OS_WIN
    timeBeginPeriod(1);
#endif

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
    a.installEventFilter(&w);

    w.show();
    int res = a.exec();

#ifdef Q_OS_WIN
    timeEndPeriod(1);
#endif

    return res;
}
