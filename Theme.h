#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>
#include <QMenu>

namespace Theme {

// Точные размеры и отступы из ТЗ (1 в 1)
constexpr int kTopBarHeight = 28;//высота верхнего бара
constexpr int kBottomBarHeight = 28;//высота нижнего бара
constexpr int kWinBtnWidth = 24; //размеры развернуть/свернуть/закрыть окно
constexpr int kWinBtnHeight = 24;//размеры развернуть/свернуть/закрыть окно

constexpr int kOuterMargin = 8;         // Ровно 8px по всем краям и между блоками
constexpr int kPaneSpacing = 8;         // Ровно 8px между папками
constexpr int kCrumbSpacing = 4;        // Ровно 4px между хлебными крошками
constexpr int kCardRadius = 12;         // Радиус скругления 12px везде
constexpr int kBreadcrumbHeight = 32;
constexpr int kResizeBorder = 6;
constexpr int kDefaultTreeWidth = 220;

// Отступы внутри левого бара
constexpr int kLeftBarPadding = 16;
constexpr int kLeftDividerWidth = 160;

// Тайминги и размеры анимаций
constexpr int kDragThresholdPx = 24;
constexpr int kGhostCardWidth = 240;
constexpr int kGhostCardHeight = 175;
constexpr int kGhostShadowPad = 20;
constexpr int kReflowAnimMs = 240;      // Плавное заполнение оставшихся папок
constexpr int kExpandAnimMs = 200;      //кривая движения

// Цвета Figma (#B8B8B8 @ 90%, #0F0F0F @ 21%, #D9D9D9 @ 50%, #000000 @ 35%)
inline QColor windowUnderlayColor() { return QColor(15, 15, 15, 54); }      // #0F0F0F 21%
inline QColor surfaceColor()        { return QColor(184, 184, 184, 230); }  // #B8B8B8 90%
inline QColor searchColor()         { return QColor(217, 217, 217, 128); }  // #D9D9D9 50%
inline QColor watermarkColor()      { return QColor(0, 0, 0, 89); }         // #000000 35%

inline QString surfaceCss()         { return QStringLiteral("rgba(184, 184, 184, 230)"); }
inline QString searchCss()          { return QStringLiteral("rgba(217, 217, 217, 128)"); }
inline QString fontFamily()         { return QStringLiteral("'Segoe UI', 'Inter', 'Ubuntu', 'Noto Sans', sans-serif"); }
inline QString contextMenuStyleSheet()
{
    return QString(
        "QMenu {"
        "  background-color: rgba(215, 215, 218, 225);"
        "  border: 1px solid rgba(255, 255, 255, 110);"
        "  border-radius: 12px;"
        "  padding: 4px 0px;"
        "  font-family: '%1';"
        "  font-size: 12px;"
        "  color: #1C1C1E;"
        "}"
        "QMenu::item {"
        "  background-color: transparent;"
        "  padding: 3px 14px 3px 10px;"
        "  margin: 1px 5px;"
        "  border-radius: 6px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #0A84FF;"
        "  color: #FFFFFF;"
        "}"
        "QMenu::item:disabled {"
        "  color: rgba(28, 28, 30, 95);"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: rgba(15, 15, 15, 45);"
        "  margin: 3px 10px;"
        "}"
        "QMenu::right-arrow {"
        "  width: 7px;"
        "  height: 7px;"
        "  padding-right: 6px;"
        "}"
    ).arg(fontFamily());
}


inline void applyGlassMenuStyle(QMenu *menu)
{
    if (!menu) return;
    menu->setAttribute(Qt::WA_TranslucentBackground, true);
    menu->setStyleSheet(contextMenuStyleSheet());
}


} // namespace Theme

#endif // THEME_H
