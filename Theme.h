#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>

namespace Theme {

// Размеры и отступы интерфейса (строго по SVG-макету)
constexpr int kTopBarHeight = 28;
constexpr int kWinBtnWidth = 28;
constexpr int kWinBtnHeight = 22;
constexpr int kOuterMargin = 8;
constexpr int kPaneSpacing = 8;
constexpr int kResizeBorder = 6;
constexpr int kDefaultTreeWidth = 230;

// Размеры и тайминги анимации отрыва карточек
constexpr int kDragThresholdPx = 24;
constexpr int kGhostCardWidth = 240;
constexpr int kGhostCardHeight = 175;
constexpr int kGhostShadowPad = 20;
constexpr int kShrinkAnimMs = 150;
constexpr int kExpandAnimMs = 220;

// Цвета стекла и карточек
inline QColor windowGlassTint() { return QColor(42, 46, 44, 145); }
inline QColor windowOuterBorder() { return QColor(0, 0, 0, 75); }
inline QString paneCardBg() { return QStringLiteral("#F4F3EF"); }
inline QString paneHeaderBg() { return QStringLiteral("#EAE8E1"); }
inline QString accentBlue() { return QStringLiteral("#007AFF"); }

// Кроссплатформенный стек шрифтов (Windows + Linux + macOS)
inline QString fontFamily() { return QStringLiteral("'Segoe UI', 'Inter', 'Ubuntu', 'Noto Sans', sans-serif"); }

} // namespace Theme

#endif // THEME_H
