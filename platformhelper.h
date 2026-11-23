#ifndef PLATFORMHELPER_H
#define PLATFORMHELPER_H

#include <QWidget>
#include <QString>
#include <cstdint>

namespace PlatformHelper {

// Включает размытие/прозрачность стекла на Windows и безопасно компилируется на Linux
void enableGlassBlur(QWidget *window, std::uint32_t winAbgrTint = 0x18252926);

// Определяет края окна для нативного изменения размера
Qt::Edges hitTestResizeEdges(const QWidget *window, const QPoint &localPos, int borderWidth);

// Перемещает файл или папку в системную Корзину (Windows Recycle Bin / Linux FreeDesktop Trash)
bool moveToTrash(const QString &path);

void startWindowMove(QWidget *window);
void startWindowResize(QWidget *window, Qt::Edges edges);

} // namespace PlatformHelper

#endif // PLATFORMHELPER_H
