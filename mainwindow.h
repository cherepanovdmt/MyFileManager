#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QList>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include "filepane.h"
#include "topglassbar.h"

class ThumbnailIconProvider;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QParallelAnimationGroup;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath = QString(), QWidget *parent = nullptr);
    ~MainWindow();

    void dockPaneFromExternal(const QString &path);
    bool canAcceptDockAt(const QPoint &globalPos) const;
    static void launchExpandedWindow(const QString &path, const QRect &startRect);

private slots:
    void on_action_2_triggered();
    void on_action_3_triggered();
    void on_action_5_triggered();
    void on_action_7_triggered();
    void on_action_8_triggered();

    void addNewPane(const QString &path = QString());

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QParallelAnimationGroup *activeReflowGroup = nullptr;

    Ui::MainWindow *ui;
    QFileSystemModel *dirModel;
    QFileSystemModel *fileModel;
    ThumbnailIconProvider *iconProvider = nullptr;

    QList<FilePane*> panes;
    FilePane *activePane = nullptr;
    QWidget *searchContainer = nullptr;
    void updateSearchBarGeometry();
    void highlightTreeMatches(const QString &query);

    QStringList bookmarks;
    QMenu *bookmarksMenu = nullptr;

    void setActivePane(FilePane *pane);
    void removePane(FilePane *pane);
    void equalizePaneSizes();
    void layoutPanesAnimated(bool animate = true);


    void openPathInActivePane(const QString &path);
    void syncTreeWithPath(const QString &path);
    void showPaneContextMenu(FilePane *pane, const QPoint &pos);

    void setupBookmarks();
    void loadBookmarks();
    void saveBookmarks();
    void updateBookmarksMenu();
    void addBookmark(const QString &path);
    void removeBookmark(const QString &path);
    void updatePaneHighlights();
    void refreshDriveIcons();

    FilePane::ViewMode savedViewMode = FilePane::TableMode;
    int savedIconSize = 64;

    void setupViewMenu();
    void loadViewSettings();
    void saveViewSettings(FilePane::ViewMode mode, int iconSize);
    void applyPersistentView(FilePane::ViewMode mode, int iconSize);

    TopGlassBar *topGlassBar = nullptr;
    QWidget *bottomGlassBar = nullptr;
    QMenu *viewMenu = nullptr;
    void setupTopGlassBar();
    void setupBottomGlassBar();

    // Левый бар (Bookmarks + линия 160px + Mounted devices + дерево)
    QWidget *leftSidebarCard = nullptr;
    QWidget *leftBookmarksSection = nullptr;
    QVBoxLayout *leftBookmarksListLayout = nullptr;
    void setupLeftSidebar();
    void rebuildLeftBookmarks();

    // Хлебные крошки
    QWidget *breadcrumbContainer = nullptr;
    QHBoxLayout *breadcrumbLayout = nullptr;
    QLineEdit *manualPathEdit = nullptr;
    void setupBreadcrumbs();
    void updateBreadcrumbs(const QString &path);

    // Правая рабочая область
    QWidget *rightAreaContainer = nullptr;
    QVBoxLayout *rightAreaLayout = nullptr;
    QWidget *panesHolder = nullptr;
    QWidget *bookmarksBoard = nullptr;
    QGridLayout *bookmarksGrid = nullptr;

    void setupBookmarksBoard();
    void rebuildBookmarksBoard();
    void updateWorkspaceState(bool animatePanes = false);
};

#endif // MAINWINDOW_H
