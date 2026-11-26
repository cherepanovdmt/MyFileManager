#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>

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

} // namespace Theme

#endif // THEME_H
