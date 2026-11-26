#ifndef FILEPANE_H
#define FILEPANE_H

#include <QFrame>
#include <QTableView>
#include <QListView>
#include <QAbstractItemView>
#include <QFileSystemModel>
#include <QLabel>
#include <QPushButton>
#include <QPoint>
#include <QRect>
#include <QPixmap>
#include <QPropertyAnimation>

// Плавающая миниатюра карточки при перетаскивании
class FloatingPaneGhost : public QWidget
{
    Q_OBJECT
public:
    FloatingPaneGhost(const QString &folderTitle, const QRect &startRect, const QPoint &cursorPos);
    void updateCursorPos(const QPoint &cursorPos);
    QRect currentCardRect() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QPoint m_anchorOffset;
    qreal m_progress = 0.0;
    QPixmap m_folderIcon;
};

class FilePane : public QFrame
{
    Q_OBJECT

public:
    enum ViewMode {
        TableMode = 0,
        IconMode = 1
    };

    explicit FilePane(QFileSystemModel *model, QWidget *parent = nullptr);

    QAbstractItemView* currentView() const { return m_currentView; }
    QString currentPath() const { return m_currentPath; }
    void setCurrentPath(const QString &path);
    void setActive(bool active);
    void setClosable(bool closable);
    void setContentFrozen(bool frozen);

    void highlightMatches(const QString &query);
    void copySelectionToClipboard(bool cut = false);
    void pasteFromClipboard();
    void renameSelectedItem();


    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }

    void setIconSizePx(int size);
    int iconSizePx() const { return m_iconSizePx; }
    bool isDetached() const { return m_isDetached; }

signals:
    void activated(FilePane *pane);
    void pathChanged(const QString &newPath);
    void paneDraggedOut(FilePane *pane, const QPoint &globalPos, const QRect &ghostRect);
    void contextMenuRequested(FilePane *pane, const QPoint &pos);
    void closeRequested(FilePane *pane);
    void paneDragStateChanged(FilePane *pane, bool isDetached);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget *m_headerBar;
    QLabel *m_titleLabel;
    QPushButton *m_closeBtn;

    QTableView *m_tableView;
    QListView *m_iconView;
    QAbstractItemView *m_currentView;
    QFileSystemModel *m_model;
    QString m_currentPath;

    ViewMode m_viewMode = TableMode;
    int m_iconSizePx = 64;
    bool m_isActive = false;
    bool m_isDetached = false;
bool m_dragEmitted = false;

    // Перетаскивание самой панели с плавающей миниатюрой
    bool m_isDraggingPane = false;
    bool m_ghostSpawned = false;
    QPoint m_dragStartPos;
    FloatingPaneGhost *m_ghost = nullptr;

    // Перетаскивание файлов (Drag & Drop)
    bool m_isDraggingFile = false;
    QString m_draggedFilePath;

    void setupView(QAbstractItemView *view);
    void updateGridMetrics();
    void updateCardStyle();

    void startPaneDrag(const QPoint &globalPos);
    void updatePaneDrag(const QPoint &globalPos);
    void endPaneDrag(const QPoint &globalPos);

    void performFileDrop(const QList<QUrl> &urls, const QString &targetDir, bool isCopy);
    bool copyRecursively(const QString &srcPath, const QString &dstPath);
};

#endif // FILEPANE_H
