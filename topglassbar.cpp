#include "topglassbar.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QMouseEvent>
#include <QWindow>
#include "platformhelper.h"
TopGlassBar::TopGlassBar(QMenu *viewMenu, QMenu *bookmarksMenu, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("TopGlassBar");
    setFixedHeight(Theme::kTopBarHeight);
    installEventFilter(this);

    setStyleSheet(
        QString(
            "#TopGlassBar {"
            "  background-color: rgba(168, 172, 170, 125);"
            "  border-bottom: 1px solid rgba(0, 0, 0, 50);"
            "}"
            "QPushButton#BarMenuBtn {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 5px;"
            "  color: #111111;"
            "  font-family: %1;"
            "  font-size: 12px;"
            "  font-weight: 300;"
            "  padding: 2px 10px;"
            "}"
            "QPushButton#BarMenuBtn:hover {"
            "  background-color: rgba(255, 255, 255, 95);"
            "}"
            "QPushButton#BarMenuBtn::menu-indicator {"
            "  image: none;"
            "  width: 0px;"
            "  height: 0px;"
            "}"
            "QPushButton#WinCtrlBtn, QPushButton#WinCloseBtn {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 5px;"
            "  color: #111111;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "}"
            "QPushButton#WinCtrlBtn:hover {"
            "  background-color: rgba(255, 255, 255, 100);"
            "}"
            "QPushButton#WinCloseBtn:hover {"
            "  background-color: #E81123;"
            "  color: #FFFFFF;"
            "}"
        ).arg(Theme::fontFamily())
    );

    QHBoxLayout *barLayout = new QHBoxLayout(this);
    barLayout->setContentsMargins(10, 0, 8, 0);
    barLayout->setSpacing(4);

    auto addSep = [&]() {
        QFrame *sep = new QFrame(this);
        sep->setFixedSize(1, 13);
        sep->setStyleSheet("background-color: rgba(30, 30, 30, 85);");
        barLayout->addWidget(sep);
    };

    auto addBarButton = [&](const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, this);
        btn->setObjectName("BarMenuBtn");
        btn->setCursor(Qt::PointingHandCursor);
        barLayout->addWidget(btn);
        return btn;
    };

    QPushButton *fileBtn = addBarButton("File");
    QMenu *fileMenu = new QMenu(this);
    fileMenu->addAction("Новая папка", this, &TopGlassBar::newFolderRequested);
    fileMenu->addAction("Новая панель (Ctrl+T)", this, &TopGlassBar::newPaneRequested);
    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &TopGlassBar::exitRequested);
    fileBtn->setMenu(fileMenu);

    addSep();

    QPushButton *editBtn = addBarButton("Edit");
    QMenu *editMenu = new QMenu(this);
    editMenu->addAction("Вверх (Backspace)", this, &TopGlassBar::goUpRequested);
    editMenu->addAction("Домой", this, &TopGlassBar::goHomeRequested);
    editMenu->addSeparator();
    editMenu->addAction("Переместить в корзину (Del)", this, &TopGlassBar::deleteRequested);
    editBtn->setMenu(editMenu);

    addSep();

    QPushButton *viewBtn = addBarButton("View");
    if (viewMenu) viewBtn->setMenu(viewMenu);

    addSep();

    QPushButton *bmBtn = addBarButton("Bookmarks");
    if (bookmarksMenu) bmBtn->setMenu(bookmarksMenu);

    addSep();

    QPushButton *settingsBtn = addBarButton("Settings");
    connect(settingsBtn, &QPushButton::clicked, this, &TopGlassBar::settingsRequested);

    addSep();

    QPushButton *newPaneBtn = addBarButton("New panel");
    connect(newPaneBtn, &QPushButton::clicked, this, &TopGlassBar::newPaneRequested);

    addSep();

    QPushButton *trashBtn = addBarButton("Trash");
    connect(trashBtn, &QPushButton::clicked, this, &TopGlassBar::deleteRequested);

    addSep();

    QPushButton *helpBtn = addBarButton("Help");
    connect(helpBtn, &QPushButton::clicked, this, &TopGlassBar::helpRequested);

    barLayout->addStretch(1);

    QPushButton *minBtn = new QPushButton("—", this);
    minBtn->setObjectName("WinCtrlBtn");
    minBtn->setFixedSize(Theme::kWinBtnWidth, Theme::kWinBtnHeight);
    connect(minBtn, &QPushButton::clicked, this, [this]() {
        if (window()) window()->showMinimized();
    });

    QPushButton *maxBtn = new QPushButton("□", this);
    maxBtn->setObjectName("WinCtrlBtn");
    maxBtn->setFixedSize(Theme::kWinBtnWidth, Theme::kWinBtnHeight);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        if (!window()) return;
        if (window()->isMaximized()) window()->showNormal();
        else window()->showMaximized();
    });

    QPushButton *closeBtn = new QPushButton("×", this);
    closeBtn->setObjectName("WinCloseBtn");
    closeBtn->setFixedSize(Theme::kWinBtnWidth, Theme::kWinBtnHeight);
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        if (window()) window()->close();
    });

    barLayout->addWidget(minBtn);
    barLayout->addWidget(maxBtn);
    barLayout->addWidget(closeBtn);
}

bool TopGlassBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && window() && window()->windowHandle()) {
                PlatformHelper::startWindowMove(window());

                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && window()) {
                if (window()->isMaximized()) window()->showNormal();
                else window()->showMaximized();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
