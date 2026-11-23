#include "thumbnailiconprovider.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Theme.h"
#include "platformhelper.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QToolButton>
#include <QShortcut>
#include <QSettings>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <QGraphicsDropShadowEffect>
#include <QFileIconProvider>
#include <QImageReader>
#include <QPixmap>
#include <QHash>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QActionGroup>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <functional>

// ==========================================
// Конструктор и управление панелями MainWindow
// ==========================================
MainWindow::MainWindow(const QString &initialPath, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Файловый менеджер");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    setMouseTracking(true);

    if (ui->tableView) {
        ui->tableView->hide();
        ui->tableView->deleteLater();
        ui->tableView = nullptr;
    }

    if (ui->centralwidget->layout()) {
        ui->centralwidget->layout()->setContentsMargins(Theme::kOuterMargin, Theme::kOuterMargin, Theme::kOuterMargin, Theme::kOuterMargin);
        ui->centralwidget->layout()->setSpacing(Theme::kPaneSpacing);
    }

    dirModel = new QFileSystemModel(this);
    dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    dirModel->setRootPath("");

    ui->treeView->setModel(dirModel);
    for (int i = 1; i < dirModel->columnCount(); ++i) {
        ui->treeView->hideColumn(i);
    }
    ui->treeView->setHeaderHidden(true);
    ui->treeView->setUniformRowHeights(true);
    ui->treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    fileModel = new QFileSystemModel(this);
    fileModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    fileModel->setRootPath("");

    iconProvider = new ThumbnailIconProvider();
    dirModel->setIconProvider(iconProvider);
    fileModel->setIconProvider(iconProvider);

    QShortcut *newPaneShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    connect(newPaneShortcut, &QShortcut::activated, this, [this]() {
        QString path = activePane ? activePane->currentPath() : QDir::homePath();
        addNewPane(path);
    });
    QShortcut *upShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(upShortcut, &QShortcut::activated, this, &MainWindow::on_action_7_triggered);

    QShortcut *delShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delShortcut, &QShortcut::activated, this, &MainWindow::on_action_2_triggered);

    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, [this]() {
        if (activePane) activePane->copySelectionToClipboard(false);
    });

    QShortcut *cutShortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cutShortcut, &QShortcut::activated, this, [this]() {
        if (activePane) activePane->copySelectionToClipboard(true);
    });

    QShortcut *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, [this]() {
        if (activePane) activePane->pasteFromClipboard();
    });

    QShortcut *renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(renameShortcut, &QShortcut::activated, this, [this]() {
        if (activePane) activePane->renameSelectedItem();
    });


    menuBar()->hide();
    ui->toolBar->hide();
    setupBookmarks();
    loadViewSettings();
    setupViewMenu();
    setupTopGlassBar();
    setupBreadcrumbs();
    setupBookmarksBoard();

    searchContainer = new QWidget(this);
    searchContainer->setObjectName("SearchCapsule");
    searchContainer->setFixedHeight(34);
    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(10, 0, 6, 0);
    searchLayout->setSpacing(4);
    QLineEdit *searchEdit = new QLineEdit(searchContainer);
    searchEdit->setPlaceholderText("Enter to search...");
    QPushButton *clearSearchBtn = new QPushButton("×", searchContainer);
    clearSearchBtn->setFixedSize(22, 22);
    clearSearchBtn->setCursor(Qt::PointingHandCursor);
    clearSearchBtn->hide();
    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(clearSearchBtn);
    connect(searchEdit, &QLineEdit::textChanged, this, [this, clearSearchBtn](const QString &text) {
    clearSearchBtn->setVisible(!text.isEmpty());
    for (FilePane *pane : panes) {
    pane->highlightMatches(text);
    }
    highlightTreeMatches(text);
    });
    connect(clearSearchBtn, &QPushButton::clicked, this, [searchEdit]() {
    searchEdit->clear();
    searchEdit->setFocus();
    });
    QShortcut *findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(findShortcut, &QShortcut::activated, this, [searchEdit]() {
    searchEdit->setFocus();
    searchEdit->selectAll();
    });
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    PlatformHelper::enableGlassBlur(this);
    ui->centralwidget->setAttribute(Qt::WA_TranslucentBackground);
    ui->centralwidget->setStyleSheet("#centralwidget { background: transparent; }");
    ui->treeView->setStyleSheet(
    QString(
    "QTreeView {"
    "  background-color: rgba(235, 237, 236, 225);"
    "  border: 1px solid rgba(0, 0, 0, 40);"
    "  border-radius: 12px;"
    "  padding: 6px;"
    "  color: #1C1C1E;"
    "  font-family: %1;"
    "  outline: none;"
    "}"
    "QTreeView::item {"
    "  height: 24px;"
    "  border-radius: 6px;"
    "  padding-left: 4px;"
    "}"
    "QTreeView::item:selected {"
    "  background-color: rgba(165, 203, 238, 195);"
    "  color: #111111;"
    "}"
    "QTreeView::item:hover:!selected {"
    "  background-color: rgba(255, 255, 255, 95);"
    "}"
    ).arg(Theme::fontFamily())
    );
    searchContainer->setStyleSheet(
    QString(
    "#SearchCapsule {"
    "  background-color: rgba(228, 228, 226, 230);"
    "  border: 1px solid rgba(0, 0, 0, 55);"
    "  border-radius: 10px;"
    "}"
    "QLineEdit {"
    "  border: none;"
    "  background: transparent;"
    "  color: #1C1C1E;"
    "  font-family: %1;"
    "  font-size: 13px;"
    "  padding: 2px 8px;"
    "}"
    "QPushButton {"
    "  border: none;"
    "  background: transparent;"
    "  color: #555555;"
    "  font-weight: bold;"
    "  border-radius: 6px;"
    "}"
    "QPushButton:hover {"
    "  background-color: rgba(0, 0, 0, 20);"
    "}"
    ).arg(Theme::fontFamily())
    );
    QGraphicsDropShadowEffect *searchShadow = new QGraphicsDropShadowEffect(searchContainer);
    searchShadow->setBlurRadius(18);
    searchShadow->setOffset(0, 4);
    searchShadow->setColor(QColor(0, 0, 0, 45));
    searchContainer->setGraphicsEffect(searchShadow);
    ui->splitter->setHandleWidth(Theme::kPaneSpacing);
    ui->splitter->setStyleSheet("QSplitter::handle { background: transparent; }");
    connect(ui->treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
    if (!index.isValid()) return;
    if (property("isExpanding").toBool()) return;
    QString clickedPath = dirModel->filePath(index);
    if (!activePane || panes.isEmpty()) {
    addNewPane(clickedPath);
    } else {
    openPathInActivePane(clickedPath);
    }
    });
    if (initialPath.isEmpty()) {
    updateWorkspaceState();
    } else {
    addNewPane(initialPath);
    }
    QTimer::singleShot(0, this, [this]() {
    equalizePaneSizes();
    updateSearchBarGeometry();
    });
    }
    MainWindow::~MainWindow()
    {
    delete ui;
    }
    void MainWindow::addNewPane(const QString &path)
    {
    FilePane *pane = new FilePane(fileModel, rightAreaContainer);
    pane->setViewMode(savedViewMode);
    pane->setIconSizePx(savedIconSize);
    panes.append(pane);
    if (panesRowLayout) {
    panesRowLayout->addWidget(pane, 1);
    }
    connect(pane, &FilePane::activated, this, [this](FilePane *clickedPane) {
    setActivePane(clickedPane);
    });
    connect(pane, &FilePane::closeRequested, this, [this](FilePane *targetPane) {
    removePane(targetPane);
    });
    connect(pane, &FilePane::paneDragStateChanged, this, [this](FilePane *, bool) {
    updateWorkspaceState();
    });
    connect(pane, &FilePane::pathChanged, this, [this, pane](const QString &newPath) {
    if (pane == activePane) {
    updateBreadcrumbs(newPath);
    syncTreeWithPath(newPath);
    if (iconProvider) {
    iconProvider->setActivePath(newPath);
    refreshDriveIcons();
    }
    }
    });
    connect(pane, &FilePane::contextMenuRequested, this, [this](FilePane *targetPane, const QPoint &pos) {
    showPaneContextMenu(targetPane, pos);
    });
    connect(pane, &FilePane::paneDraggedOut, this, [this](FilePane *draggedPane, const QPoint &globalPos, const QRect &ghostRect) {
    QString targetPath = draggedPane->currentPath();
    MainWindow *targetWin = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
    MainWindow *mw = qobject_cast<MainWindow*>(w);
    if (!mw || mw == this || !mw->isVisible() || mw->isMinimized()) {
    continue;
    }
    QRect winRect(mw->mapToGlobal(QPoint(0, 0)), mw->size());
    if (winRect.contains(globalPos)) {
    targetWin = mw;
    break;
    }
    }
    if (targetWin) {
    targetWin->dockPaneFromExternal(targetPath);
    targetWin->raise();
    targetWin->activateWindow();
    removePane(draggedPane);
    return;
    }
    QRect myRect(mapToGlobal(QPoint(0, 0)), size());
    if (myRect.contains(globalPos)) {
    updateWorkspaceState();
    return;
    }
    removePane(draggedPane);
    MainWindow *newWin = new MainWindow(targetPath);
    newWin->setAttribute(Qt::WA_DeleteOnClose);
    newWin->playExpandFromRect(ghostRect);
    newWin->raise();
    newWin->activateWindow();
    });
    setActivePane(pane);
    pane->setCurrentPath(path);
    updateWorkspaceState();
    }
    void MainWindow::setActivePane(FilePane *pane)
    {
    if (!pane) return;
    activePane = pane;
    updatePaneHighlights();
    QString path = activePane->currentPath();
    updateBreadcrumbs(path);
    syncTreeWithPath(path);
    if (iconProvider) {
    iconProvider->setActivePath(path);
    refreshDriveIcons();
    }
    }
    void MainWindow::refreshDriveIcons()
    {
    int driveCount = dirModel->rowCount(QModelIndex());
    if (driveCount > 0) {
    QModelIndex firstDrive = dirModel->index(0, 0, QModelIndex());
    QModelIndex lastDrive = dirModel->index(driveCount - 1, 0, QModelIndex());
    emit dirModel->dataChanged(firstDrive, lastDrive, {Qt::DecorationRole});
    }
    int fileDriveCount = fileModel->rowCount(QModelIndex());
    if (fileDriveCount > 0) {
    QModelIndex firstFileDrive = fileModel->index(0, 0, QModelIndex());
    QModelIndex lastFileDrive = fileModel->index(fileDriveCount - 1, 0, QModelIndex());
    emit fileModel->dataChanged(firstFileDrive, lastFileDrive, {Qt::DecorationRole});
    }
    }
    void MainWindow::updatePaneHighlights()
    {
    bool showHighlight = (panes.size() >= 2);
    for (FilePane *p : panes) {
    p->setActive(showHighlight && (p == activePane));
    p->setClosable(!panes.isEmpty());
    }
    }
    void MainWindow::removePane(FilePane *pane)
    {
    if (!pane) return;
    int idx = panes.indexOf(pane);
    panes.removeOne(pane);
    if (panesRowLayout) {
    panesRowLayout->removeWidget(pane);
    }
    pane->hide();
    pane->deleteLater();
    if (panes.isEmpty()) {
    activePane = nullptr;
    updateWorkspaceState();
    return;
    }
    if (activePane == pane) {
    activePane = nullptr;
    FilePane *nextPane = panes.value(qMax(0, idx - 1));
    setActivePane(nextPane);
    }
    updateWorkspaceState();
    }
    bool MainWindow::canAcceptDockAt(const QPoint &globalPos) const
    {
    return frameGeometry().contains(globalPos);
    }
    void MainWindow::dockPaneFromExternal(const QString &path)
    {
    addNewPane(path);
    }
    void MainWindow::playExpandFromRect(const QRect &startRect)
    {
    setProperty("isExpanding", true);
    QSize targetSize(980, 640);
    QPoint targetTopLeft = startRect.topLeft() - QPoint(240, 60);
    targetTopLeft.setX(qMax(20, targetTopLeft.x()));
    targetTopLeft.setY(qMax(20, targetTopLeft.y()));
    QRect endRect(targetTopLeft, targetSize);
    setGeometry(startRect);
    show();
    QPropertyAnimation *expandAnim = new QPropertyAnimation(this, "geometry", this);
    expandAnim->setDuration(Theme::kExpandAnimMs);
    expandAnim->setStartValue(startRect);
    expandAnim->setEndValue(endRect);
    expandAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(expandAnim, &QPropertyAnimation::finished, this, [this]() {
    setProperty("isExpanding", false);
    });
    expandAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    void MainWindow::equalizePaneSizes()
    {
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    int treeWidth = ui->treeView->width();
    if (treeWidth < 180 || treeWidth > 320) {
    treeWidth = Theme::kDefaultTreeWidth;
    }
    int totalWidth = ui->splitter->width();
    if (totalWidth <= treeWidth) totalWidth = width() - Theme::kOuterMargin * 2;
    int rightWidth = qMax(200, totalWidth - treeWidth);
    ui->splitter->setSizes({treeWidth, rightWidth});
    if (panesRowLayout) {
    for (int i = 0; i < panes.size(); ++i) {
    FilePane *p = panes[i];
    if (p->isDetached()) {
    p->setMaximumWidth(0);
    panesRowLayout->setStretch(i, 0);
    } else {
    p->setMaximumWidth(16777215);
    panesRowLayout->setStretch(i, 1);
    }
    }
    }
    updateSearchBarGeometry();
    }
    void MainWindow::openPathInActivePane(const QString &path)
    {
        if (!activePane) {
            addNewPane(path.isEmpty() ? QDir::homePath() : path);
            return;
        }

        if (path.isEmpty()) {
            activePane->setCurrentPath("");
            return;
        }

        QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            QMessageBox::warning(this, "Ошибка", "Указанный путь не существует или не является папкой.");
            return;
        }

        activePane->setCurrentPath(info.absoluteFilePath());
    }

    void MainWindow::syncTreeWithPath(const QString &path)
    {
        if (path.isEmpty()) {
            ui->treeView->clearSelection();
            return;
        }
        QModelIndex treeIdx = dirModel->index(path);
        if (treeIdx.isValid()) {
            ui->treeView->setCurrentIndex(treeIdx);
            ui->treeView->scrollTo(treeIdx);
        }
    }

    void MainWindow::on_action_2_triggered()
    {
        if (!activePane) return;
        QAbstractItemView *tv = activePane->currentView();
        QModelIndex index = tv->currentIndex();
        if (!index.isValid()) return;

        QString path = fileModel->filePath(index);
        QFileInfo info(path);

        auto reply = QMessageBox::question(this, "Переместить в корзину",
                                           QString("Переместить «%1» в корзину?").arg(info.fileName()),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (!PlatformHelper::moveToTrash(path)) {
                QMessageBox::warning(this, "Ошибка", "Не удалось переместить объект в корзину.");
            }
        }
    }

    void MainWindow::on_action_3_triggered()
    {
        close();
    }

    void MainWindow::on_action_5_triggered()
    {
        if (!activePane) return;
        QString currentPath = activePane->currentPath();
        if (currentPath.isEmpty()) return;

        bool ok = false;
        QString folderName = QInputDialog::getText(this, "Новая папка", "Введите имя папки:",
                                                   QLineEdit::Normal, "Новая папка", &ok);
        if (ok && !folderName.trimmed().isEmpty()) {
            QModelIndex parentIdx = fileModel->index(currentPath);
            fileModel->mkdir(parentIdx, folderName.trimmed());
        }
    }

    void MainWindow::on_action_7_triggered()
    {
        if (!activePane) return;
        QString currentPath = activePane->currentPath();
        if (currentPath.isEmpty()) return;

        QDir dir(currentPath);
        if (dir.cdUp()) {
            openPathInActivePane(dir.absolutePath());
        } else {
            openPathInActivePane("");
        }
    }

    void MainWindow::on_action_8_triggered()
    {
        openPathInActivePane(QDir::homePath());
    }

    void MainWindow::showPaneContextMenu(FilePane *pane, const QPoint &pos)
    {
        QAbstractItemView *tv = pane->currentView();
        QModelIndex index = tv->indexAt(pos);
        QMenu menu(this);

        if (index.isValid()) {
            QString path = fileModel->filePath(index);
            QFileInfo info(path);

            QAction *favAction = menu.addAction("Добавить в избранное");
            connect(favAction, &QAction::triggered, this, [this, path]() {
                addBookmark(path);
            });
            menu.addSeparator();

            QAction *copyAction = menu.addAction("Копировать\tCtrl+C");
            connect(copyAction, &QAction::triggered, this, [pane]() {
                pane->copySelectionToClipboard(false);
            });

            QAction *cutAction = menu.addAction("Вырезать\tCtrl+X");
            connect(cutAction, &QAction::triggered, this, [pane]() {
                pane->copySelectionToClipboard(true);
            });

            QAction *pasteAction = menu.addAction("Вставить\tCtrl+V");
            connect(pasteAction, &QAction::triggered, this, [pane]() {
                pane->pasteFromClipboard();
            });

            menu.addSeparator();

            QAction *renameAction = menu.addAction("Переименовать\tF2");
            connect(renameAction, &QAction::triggered, this, [pane]() {
                pane->renameSelectedItem();
            });

            QAction *delAction = menu.addAction("Переместить в корзину\tDel");
            connect(delAction, &QAction::triggered, this, &MainWindow::on_action_2_triggered);
        } else {
            QAction *pasteAction = menu.addAction("Вставить\tCtrl+V");
            connect(pasteAction, &QAction::triggered, this, [pane]() {
                pane->pasteFromClipboard();
            });
            menu.addSeparator();

            QAction *newFolderAction = menu.addAction("Создать папку");
            connect(newFolderAction, &QAction::triggered, this, &MainWindow::on_action_5_triggered);

            QString currentPath = pane->currentPath();
            if (!currentPath.isEmpty()) {
                QAction *favCurrentAction = menu.addAction("Добавить текущую папку в избранное");
                connect(favCurrentAction, &QAction::triggered, this, [this, currentPath]() {
                    addBookmark(currentPath);
                });
            }
        }

        menu.addSeparator();
        QAction *closePaneAction = menu.addAction("Закрыть эту панель");
        connect(closePaneAction, &QAction::triggered, this, [this, pane]() {
            removePane(pane);
        });

        menu.exec(tv->viewport()->mapToGlobal(pos));
    }

    // ==========================================
    // Система закладок (Избранное)
    // ==========================================
    void MainWindow::setupBookmarks()
    {
        bookmarksMenu = new QMenu("Bookmarks", this);
        loadBookmarks();
        updateBookmarksMenu();
    }

    void MainWindow::loadBookmarks()
    {
        QSettings settings("MyFileManager", "Bookmarks");
        bookmarks = settings.value("paths").toStringList();
    }

    void MainWindow::saveBookmarks()
    {
        QSettings settings("MyFileManager", "Bookmarks");
        settings.setValue("paths", bookmarks);
    }

    void MainWindow::addBookmark(const QString &path)
    {
        QString cleanPath = QDir::cleanPath(path);
        if (!cleanPath.isEmpty() && !bookmarks.contains(cleanPath)) {
            bookmarks.append(cleanPath);
            saveBookmarks();
            updateBookmarksMenu();
            rebuildBookmarksBoard();
        }
    }

    void MainWindow::removeBookmark(const QString &path)
    {
        QString cleanPath = QDir::cleanPath(path);
        if (bookmarks.removeOne(cleanPath)) {
            saveBookmarks();
            updateBookmarksMenu();
            rebuildBookmarksBoard();
        }
    }

    void MainWindow::updateBookmarksMenu()
    {
        if (!bookmarksMenu) return;
        bookmarksMenu->clear();

        QAction *addCurrentAct = bookmarksMenu->addAction("+ Добавить текущую папку");
        connect(addCurrentAct, &QAction::triggered, this, [this]() {
            if (activePane && !activePane->currentPath().isEmpty()) {
                addBookmark(activePane->currentPath());
            }
        });

        if (bookmarks.isEmpty()) return;

        bookmarksMenu->addSeparator();
        for (const QString &path : bookmarks) {
            QString label = QFileInfo(path).fileName();
            if (label.isEmpty()) label = path;

            QAction *bmAct = bookmarksMenu->addAction(QString("%1  (%2)").arg(label, QDir::toNativeSeparators(path)));
            connect(bmAct, &QAction::triggered, this, [this, path]() {
                QFileInfo fi(path);
                if (fi.isDir()) {
                    if (!activePane || panes.isEmpty()) addNewPane(path);
                    else openPathInActivePane(path);
                } else {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
            });
        }

        bookmarksMenu->addSeparator();
        QAction *clearAct = bookmarksMenu->addAction("Очистить избранное");
        connect(clearAct, &QAction::triggered, this, [this]() {
            bookmarks.clear();
            saveBookmarks();
            updateBookmarksMenu();
            rebuildBookmarksBoard();
        });
    }

    // ==========================================
    // Меню «Вид»
    // ==========================================
    void MainWindow::loadViewSettings()
    {
        QSettings settings("MyFileManager", "ViewSettings");
        int modeInt = settings.value("viewMode", static_cast<int>(FilePane::TableMode)).toInt();
        savedViewMode = static_cast<FilePane::ViewMode>(modeInt);
        savedIconSize = settings.value("iconSize", 64).toInt();
    }

    void MainWindow::saveViewSettings(FilePane::ViewMode mode, int iconSize)
    {
        savedViewMode = mode;
        savedIconSize = iconSize;
        QSettings settings("MyFileManager", "ViewSettings");
        settings.setValue("viewMode", static_cast<int>(mode));
        settings.setValue("iconSize", iconSize);
    }

    void MainWindow::applyPersistentView(FilePane::ViewMode mode, int iconSize)
    {
        saveViewSettings(mode, iconSize);
        for (FilePane *pane : panes) {
            pane->setViewMode(mode);
            pane->setIconSizePx(iconSize);
        }
    }

    void MainWindow::setupViewMenu()
    {
        viewMenu = new QMenu("View", this);
        QActionGroup *viewGroup = new QActionGroup(this);
        viewGroup->setExclusive(true);

        auto addViewOption = [&](const QString &title, FilePane::ViewMode mode, int size) {
            QAction *act = viewMenu->addAction(title);
            act->setCheckable(true);
            viewGroup->addAction(act);

            if (savedViewMode == mode && (mode == FilePane::TableMode || savedIconSize == size)) {
                act->setChecked(true);
            }

            connect(act, &QAction::triggered, this, [this, mode, size]() {
                applyPersistentView(mode, size);
            });
        };

        addViewOption("Таблица", FilePane::TableMode, 64);
        viewMenu->addSeparator();
        addViewOption("Мелкие значки", FilePane::IconMode, 48);
        addViewOption("Средние значки", FilePane::IconMode, 80);
        addViewOption("Крупные значки", FilePane::IconMode, 128);
    }

    // ==========================================
    // Единый полупрозрачный верхний бар (28px по SVG-макету)
    // ==========================================
    void MainWindow::setupTopGlassBar()
    {
        topGlassBar = new TopGlassBar(viewMenu, bookmarksMenu, this);
        connect(topGlassBar, &TopGlassBar::newFolderRequested, this, &MainWindow::on_action_5_triggered);
        connect(topGlassBar, &TopGlassBar::newPaneRequested, this, [this]() {
            addNewPane(activePane ? activePane->currentPath() : QDir::homePath());
        });
        connect(topGlassBar, &TopGlassBar::exitRequested, this, &MainWindow::on_action_3_triggered);
        connect(topGlassBar, &TopGlassBar::goUpRequested, this, &MainWindow::on_action_7_triggered);
        connect(topGlassBar, &TopGlassBar::goHomeRequested, this, &MainWindow::on_action_8_triggered);
        connect(topGlassBar, &TopGlassBar::deleteRequested, this, &MainWindow::on_action_2_triggered);
        connect(topGlassBar, &TopGlassBar::settingsRequested, this, [this]() {
            QMessageBox::information(this, "Settings", "Настройки внешнего вида проводника.");
        });
        connect(topGlassBar, &TopGlassBar::helpRequested, this, [this]() {
            QMessageBox::information(this, "Справка",
                "Горячие клавиши:\n"
                "• Ctrl + T — новая панель\n"
                "• Ctrl + F — быстрый поиск\n"
                "• Backspace — на уровень вверх\n"
                "• Delete — в корзину\n"
                "• Ctrl + Колесо мыши — масштаб иконок");
        });

        setMenuWidget(topGlassBar);
    }

                // ==========================================
                // Интерактивные «хлебные крошки»
                // ==========================================
                void MainWindow::setupBreadcrumbs()
                {
                if (ui->lineEdit) ui->lineEdit->hide();
                if (ui->pushButton) ui->pushButton->hide();
                if (ui->label) ui->label->hide();
                breadcrumbContainer = new QWidget(this);
                breadcrumbContainer->setObjectName("BreadcrumbBar");
                breadcrumbContainer->setFixedHeight(30);
                breadcrumbContainer->setStyleSheet("#BreadcrumbBar { background: transparent; }");
                breadcrumbContainer->installEventFilter(this);
                breadcrumbLayout = new QHBoxLayout(breadcrumbContainer);
                breadcrumbLayout->setContentsMargins(0, 0, 0, 2);
                breadcrumbLayout->setSpacing(6);
                breadcrumbLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                manualPathEdit = new QLineEdit(breadcrumbContainer);
                manualPathEdit->setFixedHeight(26);
                manualPathEdit->setPlaceholderText("Введите путь и нажмите Enter...");
                manualPathEdit->setStyleSheet(
                "QLineEdit {"
                "  background-color: #F2F2EE;"
                "  border: 1px solid #5B9BD5;"
                "  border-radius: 7px;"
                "  color: #1C1C1E;"
                "  font-size: 12px;"
                "  padding: 2px 10px;"
                "}"
                );
                manualPathEdit->hide();
                connect(manualPathEdit, &QLineEdit::returnPressed, this, [this]() {
                QString typed = manualPathEdit->text().trimmed();
                manualPathEdit->hide();
                openPathInActivePane(typed);
                });
                connect(manualPathEdit, &QLineEdit::editingFinished, this, [this]() {
                manualPathEdit->hide();
                updateBreadcrumbs(activePane ? activePane->currentPath() : QString());
                });
                }
                void MainWindow::updateBreadcrumbs(const QString &path)
                {
                if (!breadcrumbLayout) return;
                QLayoutItem *item = nullptr;
                while ((item = breadcrumbLayout->takeAt(0)) != nullptr) {
                if (item->widget() && item->widget() != manualPathEdit) {
                item->widget()->deleteLater();
                }
                delete item;
                }
                if (manualPathEdit && manualPathEdit->isVisible()) {
                breadcrumbLayout->addWidget(manualPathEdit, 1);
                return;
                }
                auto addCrumbButton = [&](const QString &label, const QString &targetPath) {
                QPushButton *btn = new QPushButton(label, breadcrumbContainer);
                btn->setCursor(Qt::PointingHandCursor);
                btn->setFixedHeight(26);
                btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #F2F2EE;"
                "  border: 1px solid rgba(255, 255, 255, 200);"
                "  border-radius: 7px;"
                "  color: #1C1C1E;"
                "  font-size: 12px;"
                "  font-weight: 500;"
                "  padding: 2px 12px;"
                "}"
                "QPushButton:hover {"
                "  background-color: #FFFFFF;"
                "  border: 1px solid #5B9BD5;"
                "}"
                );
                QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(btn);
                shadow->setBlurRadius(8);
                shadow->setOffset(0, 2);
                shadow->setColor(QColor(0, 0, 0, 30));
                btn->setGraphicsEffect(shadow);
                connect(btn, &QPushButton::clicked, this, [this, targetPath]() {
                openPathInActivePane(targetPath);
                });
                breadcrumbLayout->addWidget(btn);
                };
                addCrumbButton("Компьютер", "");
                QString clean = QDir::cleanPath(path);
                if (clean.isEmpty()) {
                breadcrumbLayout->addStretch(1);
                return;
                }
                QStringList parts = clean.split('/', Qt::SkipEmptyParts);
                QString accumulated;
                for (int i = 0; i < parts.size(); ++i) {
                if (i == 0 && parts[i].endsWith(':')) {
                accumulated = parts[i] + "/";
                } else {
                if (!accumulated.endsWith('/')) accumulated += '/';
                accumulated += parts[i];
                }
                addCrumbButton(parts[i], accumulated);
                }
                breadcrumbLayout->addStretch(1);
                }
                // ==========================================
                // Стартовый экран Bookmarks (при n = 0 панелей)
                // ==========================================
                void MainWindow::setupBookmarksBoard()
                {
                rightAreaContainer = new QWidget(this);
                rightAreaContainer->setStyleSheet("background: transparent;");
                rightAreaLayout = new QVBoxLayout(rightAreaContainer);
                rightAreaLayout->setContentsMargins(0, 0, 0, 0);
                rightAreaLayout->setSpacing(6);
                if (breadcrumbContainer) {
                breadcrumbContainer->setParent(rightAreaContainer);
                rightAreaLayout->addWidget(breadcrumbContainer);
                }
                QWidget *panesHolder = new QWidget(rightAreaContainer);
                panesHolder->setStyleSheet("background: transparent;");
                panesRowLayout = new QHBoxLayout(panesHolder);
                panesRowLayout->setContentsMargins(0, 0, 0, 0);
                panesRowLayout->setSpacing(Theme::kPaneSpacing);
                rightAreaLayout->addWidget(panesHolder, 1);
                bookmarksBoard = new QWidget(rightAreaContainer);
                bookmarksBoard->setStyleSheet("background: transparent;");
                QVBoxLayout *boardVBox = new QVBoxLayout(bookmarksBoard);
                boardVBox->setContentsMargins(20, 6, 16, 60);
                boardVBox->setSpacing(18);
                QHBoxLayout *headerRow = new QHBoxLayout();
                headerRow->setContentsMargins(0, 0, 0, 0);
                QWidget *leftSpacer = new QWidget(bookmarksBoard);
                leftSpacer->setFixedSize(28, 28);
                leftSpacer->setStyleSheet("background: transparent;");
                QLabel *boardTitle = new QLabel("Bookmarks", bookmarksBoard);
                boardTitle->setAlignment(Qt::AlignCenter);
                boardTitle->setStyleSheet(
                QString(
                "QLabel {"
                "  color: #111111;"
                "  font-family: %1;"
                "  font-size: 13px;"
                "  font-weight: 500;"
                "  background: transparent;"
                "}"
                ).arg(Theme::fontFamily())
                );
                QPushButton *addBookmarkBtn = new QPushButton("+", bookmarksBoard);
                addBookmarkBtn->setFixedSize(28, 28);
                addBookmarkBtn->setCursor(Qt::PointingHandCursor);
                addBookmarkBtn->setToolTip("Добавить папку или приложение в избранное");
                addBookmarkBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(242, 242, 238, 190);"
                "  border: 1px solid rgba(0, 0, 0, 45);"
                "  border-radius: 8px;"
                "  color: #111111;"
                "  font-size: 16px;"
                "  font-weight: 500;"
                "}"
                "QPushButton:hover {"
                "  background-color: #FFFFFF;"
                "  border: 1px solid #5B9BD5;"
                "}"
                "QPushButton::menu-indicator { image: none; width: 0px; height: 0px; }"
                );
                QMenu *addBmMenu = new QMenu(addBookmarkBtn);
                QAction *addFolderAct = addBmMenu->addAction("Добавить папку...");
                QAction *addAppAct = addBmMenu->addAction("Добавить программу или файл...");
                addBookmarkBtn->setMenu(addBmMenu);
                connect(addFolderAct, &QAction::triggered, this, [this]() {
                QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для избранного", QDir::homePath());
                if (!dir.isEmpty()) {
                addBookmark(dir);
                }
                });
                connect(addAppAct, &QAction::triggered, this, [this]() {
                QString file = QFileDialog::getOpenFileName(this, "Выберите программу или файл", QDir::homePath());
                if (!file.isEmpty()) {
                addBookmark(file);
                }
                });
                headerRow->addWidget(leftSpacer);
                headerRow->addStretch(1);
                headerRow->addWidget(boardTitle);
                headerRow->addStretch(1);
                headerRow->addWidget(addBookmarkBtn);
                boardVBox->addLayout(headerRow);
                QWidget *gridHolder = new QWidget(bookmarksBoard);
                gridHolder->setStyleSheet("background: transparent;");
                bookmarksGrid = new QGridLayout(gridHolder);
                bookmarksGrid->setContentsMargins(0, 0, 0, 0);
                bookmarksGrid->setHorizontalSpacing(22);
                bookmarksGrid->setVerticalSpacing(18);
                bookmarksGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                boardVBox->addWidget(gridHolder, 1);
                rightAreaLayout->addWidget(bookmarksBoard, 1);
                ui->splitter->addWidget(rightAreaContainer);
                rebuildBookmarksBoard();
                }
                void MainWindow::rebuildBookmarksBoard()
                {
                if (!bookmarksGrid) return;
                QLayoutItem *child = nullptr;
                while ((child = bookmarksGrid->takeAt(0)) != nullptr) {
                if (child->widget()) child->widget()->deleteLater();
                delete child;
                }
                const int columns = 7;
                int idx = 0;
                for (const QString &path : bookmarks) {
                QFileInfo info(path);
                if (!info.exists()) continue;
                QToolButton *itemBtn = new QToolButton(bookmarksBoard);
                itemBtn->setCursor(Qt::PointingHandCursor);
                itemBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                itemBtn->setIconSize(QSize(54, 54));
                itemBtn->setFixedSize(84, 88);
                itemBtn->setContextMenuPolicy(Qt::CustomContextMenu);
                QString label = info.completeBaseName();
                if (info.isDir() || label.isEmpty()) label = info.fileName();
                if (label.isEmpty()) label = path;
                itemBtn->setText(label);
                if (iconProvider) {
                itemBtn->setIcon(iconProvider->icon(info));
                }
                itemBtn->setStyleSheet(
                QString(
                "QToolButton {"
                "  background: transparent;"
                "  border: none;"
                "  border-radius: 10px;"
                "  color: #111111;"
                "  font-family: %1;"
                "  font-size: 11px;"
                "  padding: 4px;"
                "}"
                "QToolButton:hover {"
                "  background-color: rgba(255, 255, 255, 75);"
                "}"
                ).arg(Theme::fontFamily())
                );
                connect(itemBtn, &QToolButton::clicked, this, [this, path]() {
                QFileInfo fi(path);
                if (fi.isDir()) {
                addNewPane(path);
                } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
                });
                connect(itemBtn, &QToolButton::customContextMenuRequested, this, [this, itemBtn, path](const QPoint &pos) {
                QMenu ctx(this);
                QAction *delAct = ctx.addAction("Удалить из избранного");
                connect(delAct, &QAction::triggered, this, [this, path]() {
                removeBookmark(path);
                });
                ctx.exec(itemBtn->mapToGlobal(pos));
                });
                bookmarksGrid->addWidget(itemBtn, idx / columns, idx % columns);
                idx++;
                }
                }
                void MainWindow::updateWorkspaceState()
                {
                int activePanes = 0;
                for (FilePane *p : panes) {
                if (!p->isDetached()) activePanes++;
                }
                QWidget *panesHolder = panesRowLayout ? panesRowLayout->parentWidget() : nullptr;
                if (activePanes == 0) {
                if (breadcrumbContainer) breadcrumbContainer->hide();
                if (panesHolder) panesHolder->hide();
                if (bookmarksBoard) {
                rebuildBookmarksBoard();
                bookmarksBoard->show();
                }
                } else {
                if (bookmarksBoard) bookmarksBoard->hide();
                if (breadcrumbContainer) breadcrumbContainer->show();
                if (panesHolder) panesHolder->show();
                }
                equalizePaneSizes();
                updatePaneHighlights();
                }
                // ==========================================
                // Кроссплатформенный ресайз, перемещение окна, поиск и фон
                // ==========================================
                void MainWindow::resizeEvent(QResizeEvent *event)
                {
                QMainWindow::resizeEvent(event);
                updateSearchBarGeometry();
                }
                void MainWindow::updateSearchBarGeometry()
                {
                if (!searchContainer) return;
                int barWidth = qBound(260, width() / 3, 520);
                int barHeight = searchContainer->height();
                int leftX = (width() - barWidth) / 2;
                QPoint splitterTopLeft = ui->splitter->mapTo(this, QPoint(0, 0));
                int topY = splitterTopLeft.y() + ui->splitter->height() - barHeight - 20;
                searchContainer->setGeometry(leftX, topY, barWidth, barHeight);
                searchContainer->raise();
                }
                void MainWindow::highlightTreeMatches(const QString &query)
                {
                QItemSelectionModel *selModel = ui->treeView->selectionModel();
                if (!selModel) return;
                selModel->clearSelection();
                QString trimmed = query.trimmed();
                if (trimmed.isEmpty()) return;
                bool scrolled = false;
                std::function<void(const QModelIndex&)> searchNode = [&](const QModelIndex &parent) {
                int rows = dirModel->rowCount(parent);
                for (int row = 0; row < rows; ++row) {
                QModelIndex idx = dirModel->index(row, 0, parent);
                if (!idx.isValid()) continue;
                QString folderName = dirModel->fileName(idx);
                if (folderName.contains(trimmed, Qt::CaseInsensitive)) {
                selModel->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                if (!scrolled) {
                ui->treeView->scrollTo(idx);
                scrolled = true;
                }
                }
                if (dirModel->hasChildren(idx) && (ui->treeView->isExpanded(idx) || !parent.isValid())) {
                searchNode(idx);
                }
                }
                };
                searchNode(QModelIndex());
                }
                bool MainWindow::eventFilter(QObject *watched, QEvent *event)
                {
                if (watched == breadcrumbContainer && event->type() == QEvent::MouseButtonDblClick) {
                if (manualPathEdit) {
                QString cur = activePane ? QDir::toNativeSeparators(activePane->currentPath()) : "";
                manualPathEdit->setText(cur);
                manualPathEdit->show();
                updateBreadcrumbs(cur);
                manualPathEdit->setFocus();
                manualPathEdit->selectAll();
                }
                return true;
                }
                return QMainWindow::eventFilter(watched, event);
                }
                void MainWindow::mouseMoveEvent(QMouseEvent *event)
                {
                Qt::Edges edges = PlatformHelper::hitTestResizeEdges(this, event->pos(), Theme::kResizeBorder);
                if ((edges == (Qt::TopEdge | Qt::LeftEdge)) || (edges == (Qt::BottomEdge | Qt::RightEdge))) {
                setCursor(Qt::SizeFDiagCursor);
                } else if ((edges == (Qt::TopEdge | Qt::RightEdge)) || (edges == (Qt::BottomEdge | Qt::LeftEdge))) {
                setCursor(Qt::SizeBDiagCursor);
                } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
                setCursor(Qt::SizeHorCursor);
                } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
                setCursor(Qt::SizeVerCursor);
                } else {
                unsetCursor();
                }
                QMainWindow::mouseMoveEvent(event);
                }
                void MainWindow::mousePressEvent(QMouseEvent *event)
                {
                if (event->button() == Qt::LeftButton) {
                Qt::Edges edges = PlatformHelper::hitTestResizeEdges(this, event->pos(), Theme::kResizeBorder);
                if (edges != Qt::Edges() && windowHandle()) {
                PlatformHelper::startWindowResize(this, edges);
                event->accept();
                return;
                }
                }
                QMainWindow::mousePressEvent(event);
                }
                void MainWindow::paintEvent(QPaintEvent *event)
                {
                QPainter painter(this);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.fillRect(rect(), Theme::windowGlassTint());
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                painter.setPen(QPen(Theme::windowOuterBorder(), 1));
                painter.drawRect(rect().adjusted(0, 0, -1, -1));
                QMainWindow::paintEvent(event);
                }
