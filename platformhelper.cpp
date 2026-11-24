#include <QDir>
#include <string>
#include "platformhelper.h"
#include <QFile>
#include <QFileInfo>
#include <QWindow>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

namespace {
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using pfnSetWindowCompositionAttribute = BOOL(WINAPI *)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
} // namespace
#endif

namespace PlatformHelper {
void openSystemSettings()
{
#ifdef Q_OS_WIN
    QDesktopServices::openUrl(QUrl("ms-settings:"));
#elif defined(Q_OS_LINUX)
    if (!QProcess::startDetached("gnome-control-center", {})) {
        if (!QProcess::startDetached("systemsettings", {})) {
            if (!QProcess::startDetached("systemsettings5", {})) {
                QProcess::startDetached("xfce4-settings-manager", {});
            }
        }
    }
#endif
}


void enableGlassBlur(QWidget *window, std::uint32_t winAbgrTint)
{
    if (!window) return;

#ifdef Q_OS_WIN
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return;

    auto setWCA = reinterpret_cast<pfnSetWindowCompositionAttribute>(
        GetProcAddress(hUser, "SetWindowCompositionAttribute")
    );
    if (!setWCA) return;

    ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, static_cast<DWORD>(winAbgrTint), 0 };
    WINDOWCOMPOSITIONATTRIBDATA data = { 19, &policy, sizeof(policy) };
    setWCA(reinterpret_cast<HWND>(window->winId()), &data);
#else
    Q_UNUSED(winAbgrTint);
#endif
}

Qt::Edges hitTestResizeEdges(const QWidget *window, const QPoint &localPos, int borderWidth)
{
    if (!window || window->isMaximized()) {
        return Qt::Edges();
    }

    Qt::Edges edges;
    if (localPos.x() <= borderWidth) edges |= Qt::LeftEdge;
    if (localPos.x() >= window->width() - borderWidth) edges |= Qt::RightEdge;
    if (localPos.y() <= borderWidth) edges |= Qt::TopEdge;
    if (localPos.y() >= window->height() - borderWidth) edges |= Qt::BottomEdge;
    return edges;
}

bool moveToTrash(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QFile::moveToTrash(path);
#elif defined(Q_OS_WIN)
    QString nativePath = QDir::toNativeSeparators(path);
    std::wstring doubleNullPath = nativePath.toStdWString();
    doubleNullPath.push_back(L'\0');
    doubleNullPath.push_back(L'\0');

    SHFILEOPSTRUCTW fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = doubleNullPath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    return (SHFileOperationW(&fileOp) == 0);
#else
    QFileInfo info(path);
    if (info.isDir()) {
        return QDir(path).removeRecursively();
    }
    return QFile::remove(path);
#endif
}

void startWindowMove(QWidget *window)
{
    if (!window || !window->windowHandle()) return;

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    window->windowHandle()->startSystemMove();
#elif defined(Q_OS_WIN)
    ReleaseCapture();
    SendMessageW(reinterpret_cast<HWND>(window->winId()), WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
#endif
}

void startWindowResize(QWidget *window, Qt::Edges edges)
{
    if (!window || !window->windowHandle() || edges == Qt::Edges()) return;

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    window->windowHandle()->startSystemResize(edges);
#elif defined(Q_OS_WIN)
    WPARAM dir = 0;
    if (edges == (Qt::TopEdge | Qt::LeftEdge))          dir = 4; // WMSZ_TOPLEFT
    else if (edges == (Qt::TopEdge | Qt::RightEdge))    dir = 5; // WMSZ_TOPRIGHT
    else if (edges == (Qt::BottomEdge | Qt::LeftEdge))  dir = 7; // WMSZ_BOTTOMLEFT
    else if (edges == (Qt::BottomEdge | Qt::RightEdge)) dir = 8; // WMSZ_BOTTOMRIGHT
    else if (edges & Qt::LeftEdge)                      dir = 1; // WMSZ_LEFT
    else if (edges & Qt::RightEdge)                     dir = 2; // WMSZ_RIGHT
    else if (edges & Qt::TopEdge)                       dir = 3; // WMSZ_TOP
    else if (edges & Qt::BottomEdge)                    dir = 6; // WMSZ_BOTTOM

    if (dir != 0) {
        ReleaseCapture();
        SendMessageW(reinterpret_cast<HWND>(window->winId()), WM_SYSCOMMAND, SC_SIZE | dir, 0);
    }
#endif
}
} // namespace PlatformHelper
