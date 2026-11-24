#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Theme.h"
#include "platformhelper.h"
#include "thumbnailiconprovider.h"

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
#include <QPainterPath>
#include <QWindow>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QStandardPaths>
#include <QActionGroup>
#include <QDesktopServices>
#include <QUrl>
#include <functional>

namespace {
class SmoothWindowRevealOverlay : public QWidget
{
public:
    SmoothWindowRevealOverlay(const QRect &cardStartRect, const QRect &winEndRect, const QString &title)
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
          m_startRect(cardStartRect),
          m_endRect(winEndRect),
          m_title(title)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setGeometry(m_startRect);
    }

    void setProgress(qreal t)
    {
        m_progress = t;
        auto lerp = [](int a, int b, qreal k) { return qRound(a + (b - a) * k); };
        setGeometry(
            lerp(m_startRect.x(), m_endRect.x(), t),
            lerp(m_startRect.y(), m_endRect.y(), t),
            lerp(m_startRect.width(), m_endRect.width(), t),
            lerp(m_startRect.height(), m_endRect.height(), t)
        );
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int w = width();
        const int h = height();
        auto lerp = [](int a, int b, qreal k) { return qRound(a + (b - a) * k); };

        // 1. Внешний стеклянный фон окна (#0F0F0F 21%)
        QPainterPath winPath;
        winPath.addRoundedRect(0, 0, w, h, Theme::kCardRadius, Theme::kCardRadius);
        p.fillPath(winPath, QColor(15, 15, 15, qRound(54 * m_progress)));

        // 2. Верхний и нижний бары (38px, #B8B8B8 90%)
        int barH = lerp(0, Theme::kTopBarHeight, m_progress);
        if (barH > 2) {
            p.fillRect(0, 0, w, barH, QColor(184, 184, 184, qRound(230 * m_progress)));
            p.fillRect(0, h - barH, w, barH, QColor(184, 184, 184, qRound(230 * m_progress)));
        }

        // 3. Левый бар (отступ 8px от баров и левого края)
        int topPad = barH + lerp(0, Theme::kOuterMargin, m_progress);
        int bottomPad = barH + lerp(0, Theme::kOuterMargin, m_progress);
        int leftPad = lerp(0, Theme::kOuterMargin, m_progress);
        int treeW = lerp(0, Theme::kDefaultTreeWidth, m_progress);

        if (treeW > 8) {
            QPainterPath treePath;
            treePath.addRoundedRect(leftPad, topPad, treeW, qMax(10, h - topPad - bottomPad), Theme::kCardRadius, Theme::kCardRadius);
            p.fillPath(treePath, QColor(184, 184, 184, qRound(230 * m_progress)));
        }

        // 4. Сама карточка папки (#B8B8B8 90%, радиус 12)
        int cardLeft = (treeW > 0) ? (leftPad + treeW + lerp(0, Theme::kPaneSpacing, m_progress)) : 0;
        int cardTop = topPad + lerp(0, Theme::kBreadcrumbHeight + Theme::kOuterMargin, m_progress);
        int cardRight = w - lerp(0, Theme::kOuterMargin, m_progress);
        int cardBottom = h - bottomPad;
        QRect cardRect(cardLeft, cardTop, qMax(40, cardRight - cardLeft), qMax(40, cardBottom - cardTop));

        QPainterPath cardPath;
        cardPath.addRoundedRect(cardRect, Theme::kCardRadius, Theme::kCardRadius);
        p.fillPath(cardPath, Theme::surfaceColor());

        p.setPen(QColor("#1C1C1E"));
        p.setFont(QFont("Segoe UI", 9, QFont::Medium));
        p.drawText(QRect(cardRect.left() + 24, cardRect.top() + 18, cardRect.width() - 48, 20), Qt::AlignCenter, m_title);

        QPixmap folderIcon(":/icons/icons/folder.png");
        if (!folderIcon.isNull() && cardRect.height() > 90) {
            int iconSize = 48;
            QRect iconRect(
                cardRect.center().x() - iconSize / 2,
                cardRect.top() + 44 + (cardRect.height() - 44 - iconSize) / 2,
                iconSize,
                iconSize
            );
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(iconRect, folderIcon);
        }
    }

private:
    QRect m_startRect;
    QRect m_endRect;
    QString m_title;
    qreal m_progress = 0.0;
};
} // namespace

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

    if (statusBar()) {
        statusBar()->hide();
    }

    if (ui->centralwidget->layout()) {
        delete ui->centralwidget->layout();
    }
    QVBoxLayout *rootVBox = new QVBoxLayout(ui->centralwidget);
    rootVBox->setContentsMargins(0, 0, 0, 0);
    rootVBox->setSpacing(0);

    QWidget *workspaceBox = new QWidget(ui->centralwidget);
    workspaceBox->setStyleSheet("background: transparent;");
    QVBoxLayout *wsLayout = new QVBoxLayout(workspaceBox);
    wsLayout->setContentsMargins(Theme::kOuterMargin, Theme::kOuterMargin, Theme::kOuterMargin, Theme::kOuterMargin);
    wsLayout->setSpacing(0);
    wsLayout->addWidget(ui->splitter, 1);

    rootVBox->addWidget(workspaceBox, 1);

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

    // Горячие клавиши
    QShortcut *newPaneShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    connect(newPaneShortcut, &QShortcut::activated, this, [this]() {
        addNewPane(activePane ? activePane->currentPath() : QDir::homePath());
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
    setupBottomGlassBar();
    setupLeftSidebar();
    setupBreadcrumbs();
    setupBookmarksBoard();

    // Капсула поиска (#D9D9D9 50%, радиус 12px)
    searchContainer = new QWidget(this);
    searchContainer->setObjectName("SearchCapsule");
    searchContainer->setFixedHeight(36);

    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(12, 0, 8, 0);
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
    if (initialPath.isEmpty()) {
        PlatformHelper::enableGlassBlur(this);
    }

    ui->centralwidget->setAttribute(Qt::WA_TranslucentBackground);
    ui->centralwidget->setStyleSheet("#centralwidget { background: transparent; }");

    searchContainer->setStyleSheet(
                QString(
                "#SearchCapsule {"
                "  background-color: %2;"
                "  border: none;"
                "  border-radius: 12px;"
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
                "  color: #444444;"
                "  font-weight: bold;"
                "  border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "  background-color: rgba(0, 0, 0, 20);"
                "}"
                ).arg(Theme::fontFamily(), Theme::searchCss())
                );
                ui->splitter->setHandleWidth(Theme::kPaneSpacing);
                ui->splitter->setStyleSheet("QSplitter::handle { background: transparent; }");
                connect(ui->splitter, &QSplitter::splitterMoved, this, [this](int, int) {
                layoutPanesAnimated(false);
                updateSearchBarGeometry();
                });
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
                updateWorkspaceState(false);
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
                FilePane *pane = new FilePane(fileModel, panesHolder);
                pane->setViewMode(savedViewMode);
                pane->setIconSizePx(savedIconSize);
                // Новая папка появляется справа от существующих и плавно сдвигает их (FLUID-анимация)
                if (panesHolder && !panes.isEmpty()) {
                pane->setGeometry(panesHolder->width(), 0, qMax(180, panesHolder->width() / (panes.size() + 1)), panesHolder->height());
                }
                pane->show();
                panes.append(pane);
                connect(pane, &FilePane::activated, this, [this](FilePane *clickedPane) {
                setActivePane(clickedPane);
                });
                connect(pane, &FilePane::closeRequested, this, [this](FilePane *targetPane) {
                removePane(targetPane);
                });
                connect(pane, &FilePane::paneDragStateChanged, this, [this](FilePane *, bool) {
                // Когда карточку оторвали в миниатюру — оставшиеся папки плавно заполняют её место
                updateWorkspaceState(true);
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
                updateWorkspaceState(true);
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
                updateWorkspaceState(panes.size() > 1);
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
                pane->hide();
                pane->deleteLater();
                if (panes.isEmpty()) {
                activePane = nullptr;
                updateWorkspaceState(false);
                return;
                }
                if (activePane == pane) {
                activePane = nullptr;
                FilePane *nextPane = panes.value(qMax(0, idx - 1));
                setActivePane(nextPane);
                }
                // Плавное заполнение освободившегося места оставшимися папками!
                updateWorkspaceState(true);
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
                QPoint targetTopLeft = startRect.topLeft() - QPoint(236, 86);
                targetTopLeft.setX(qMax(20, targetTopLeft.x()));
                targetTopLeft.setY(qMax(20, targetTopLeft.y()));
                QRect endRect(targetTopLeft, targetSize);
                QString title = activePane ? QFileInfo(activePane->currentPath()).fileName() : QString("Folder");
                if (title.isEmpty() && activePane) title = activePane->currentPath();
                setGeometry(endRect);
                equalizePaneSizes();
                setWindowOpacity(0.0);
                show();
                auto *overlay = new SmoothWindowRevealOverlay(startRect, endRect, title);
                overlay->show();
                QVariantAnimation *anim = new QVariantAnimation(overlay);
                anim->setDuration(Theme::kExpandAnimMs);
                anim->setStartValue(0.0);
                anim->setEndValue(1.0);
                anim->setEasingCurve(QEasingCurve::OutCubic);
                connect(anim, &QVariantAnimation::valueChanged, overlay, [overlay](const QVariant &v) {
                overlay->setProgress(v.toReal());
                });
                connect(anim, &QVariantAnimation::finished, this, [this, overlay]() {
                setWindowOpacity(1.0);
                PlatformHelper::enableGlassBlur(this);
                raise();
                activateWindow();
                overlay->hide();
                overlay->deleteLater();
                setProperty("isExpanding", false);
                layoutPanesAnimated(false);
                updateSearchBarGeometry();
                });
                anim->start(QAbstractAnimation::DeleteWhenStopped);
                }
                // ==========================================
                // Пиксельный расчёт ширины папок (W - 8*(n-1)) / n + синхронная FLIP-анимация заполнения
                // ==========================================
                void MainWindow::layoutPanesAnimated(bool animate)
                {
                if (!panesHolder) return;
                QList<FilePane*> visiblePanes;
                for (FilePane *p : panes) {
                if (p->isDetached()) {
                p->hide();
                } else {
                p->show();
                visiblePanes.append(p);
                }
                }
                int n = visiblePanes.size();
                if (n == 0) return;
                int totalW = panesHolder->width();
                int totalH = panesHolder->height();
                if (totalW <= 0 || totalH <= 0) return;
                // Формула из ТЗ: (d - 8*(n - 1)) / n с округлением вниз и раскидыванием остатка по ±1 пикселю
                int totalGaps = Theme::kPaneSpacing * (n - 1);
                int availableW = qMax(n * 40, totalW - totalGaps);
                int baseW = availableW / n;
                int remainder = availableW % n;
                QList<QRect> targetRects;
                int curX = 0;
                for (int i = 0; i < n; ++i) {
                // Остаток раскидываем по +1 пикселю в ширину папок, чтобы сумма была строго равна totalW
                int paneW = baseW + (i >= (n - remainder) ? 1 : 0);
                targetRects.append(QRect(curX, 0, paneW, totalH));
                curX += paneW + Theme::kPaneSpacing;
                }
                if (!animate) {
                for (int i = 0; i < n; ++i) {
                visiblePanes[i]->setGeometry(targetRects[i]);
                }
                return;
                }
                // Параллельная анимация за одинаковое время (180 мс):
                // папка, которой нужно пройти больше пикселей, движется быстрее остальных!
                QParallelAnimationGroup *group = new QParallelAnimationGroup(panesHolder);
                for (int i = 0; i < n; ++i) {
                FilePane *p = visiblePanes[i];
                QRect startR = p->geometry();
                QRect endR = targetRects[i];
                if (startR == endR) continue;
                QPropertyAnimation *anim = new QPropertyAnimation(p, "geometry", group);
                anim->setDuration(Theme::kReflowAnimMs);
                anim->setStartValue(startR);
                anim->setEndValue(endR);
                anim->setEasingCurve(QEasingCurve::OutCubic);
                group->addAnimation(anim);
                }
                if (group->animationCount() > 0) {
                group->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                delete group;
                }
                }
                void MainWindow::equalizePaneSizes()
                {
                ui->splitter->setStretchFactor(0, 0);
                ui->splitter->setStretchFactor(1, 1);
                int treeWidth = leftSidebarCard ? leftSidebarCard->width() : Theme::kDefaultTreeWidth;
                if (treeWidth < 180 || treeWidth > 320) {
                treeWidth = Theme::kDefaultTreeWidth;
                }
                int totalWidth = ui->splitter->width();
                if (totalWidth <= treeWidth) totalWidth = width() - Theme::kOuterMargin * 2;
                int rightWidth = qMax(200, totalWidth - treeWidth - Theme::kPaneSpacing);
                ui->splitter->setSizes({treeWidth, rightWidth});
                layoutPanesAnimated(false);
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
                        updateWorkspaceState(false);
                    }
                }

                void MainWindow::removeBookmark(const QString &path)
                {
                    QString cleanPath = QDir::cleanPath(path);
                    if (bookmarks.removeOne(cleanPath)) {
                        saveBookmarks();
                        updateBookmarksMenu();
                        rebuildBookmarksBoard();
                        updateWorkspaceState(false);
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
                        updateWorkspaceState(false);
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
                // Верхний и Нижний стеклянные бары (38px, #B8B8B8 90%, без внешних отступов)
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
                        connect(topGlassBar, &TopGlassBar::settingsRequested, this, [this]()  {
                        QMessageBox::information(this, "Settings", "Настройки внешнего вида проводника.");
                        });
                        connect(topGlassBar, &TopGlassBar::helpRequested, this, [this]()  {
                        QMessageBox::information(this, "Справка",
                        "Горячие клавиши:\n"
                        "• Ctrl + T — новая панель\n"
                        "• Ctrl + F — быстрый поиск\n"
                        "• Ctrl + C / Ctrl + X / Ctrl + V — копировать / вырезать / вставить\n"
                        "• F2 — переименовать\n"
                        "• Backspace — на уровень вверх\n"
                        "• Delete — в корзину");
                        });
                        setMenuWidget(topGlassBar);
                        }
                        void MainWindow::setupBottomGlassBar()
                        {
                        bottomGlassBar = new QWidget(this);
                        bottomGlassBar->setObjectName("BottomGlassBar");
                        bottomGlassBar->setFixedHeight(Theme::kBottomBarHeight);
                        bottomGlassBar->setStyleSheet(
                        QString(
                        "#BottomGlassBar {"
                        "  background-color: %2;"
                        "  border-bottom-left-radius: 12px;"
                        "  border-bottom-right-radius: 12px;"
                        "}"
                        "QPushButton#ControlPanelBtn {"
                        "  background: transparent;"
                        "  border: none;"
                        "  border-radius: 6px;"
                        "  color: #111111;"
                        "  font-family: %1;"
                        "  font-size: 12px;"
                        "  font-weight: 400;"
                        "  padding: 4px 12px;"
                        "}"
                        "QPushButton#ControlPanelBtn:hover {"
                        "  background-color: rgba(255, 255, 255, 85);"
                        "}"
                        ).arg(Theme::fontFamily(), Theme::surfaceCss())
                        );
                        QHBoxLayout *botLayout = new QHBoxLayout(bottomGlassBar);
                        botLayout->setContentsMargins(10, 0, 10, 0);
                        botLayout->setSpacing(0);
                        // Слева кнопка control panel (без разделителя |), открывает Параметры Windows / Настройки Linux
                        QPushButton *ctrlPanelBtn = new QPushButton("control panel", bottomGlassBar);
                        ctrlPanelBtn->setObjectName("ControlPanelBtn");
                        ctrlPanelBtn->setCursor(Qt::PointingHandCursor);
                        connect(ctrlPanelBtn, &QPushButton::clicked, this, []() {
                        PlatformHelper::openSystemSettings();
                        });
                        botLayout->addWidget(ctrlPanelBtn);
                        botLayout->addStretch(1);
                        botLayout->addWidget(ctrlPanelBtn);
                           botLayout->addStretch(1);

                           if (auto *rootVBox = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout())) {
                               rootVBox->addWidget(bottomGlassBar);
                           }
                        }
                        // ==========================================
                        // Умный левый бар (16px отступы, линия 160px по центру, Bookmarks + Mounted devices)
                        // ==========================================
                        void MainWindow::setupLeftSidebar()
                        {
                        leftSidebarCard = new QWidget(this);
                        leftSidebarCard->setObjectName("LeftSidebarCard");
                        leftSidebarCard->setStyleSheet(
                        QString(
                        "#LeftSidebarCard {"
                        "  background-color: %2;"
                        "  border-radius: 12px;"
                        "}"
                        "QLabel#SidebarSectionTitle {"
                        "  color: #111111;"
                        "  font-family: %1;"
                        "  font-size: 12px;"
                        "  font-weight: 500;"
                        "  background: transparent;"
                        "}"
                        ).arg(Theme::fontFamily(), Theme::surfaceCss())
                        );
                        QVBoxLayout *sideLayout = new QVBoxLayout(leftSidebarCard);
                        sideLayout->setContentsMargins(10, Theme::kLeftBarPadding, 10, Theme::kLeftBarPadding);
                        sideLayout->setSpacing(Theme::kLeftBarPadding);
                        // Секция Bookmarks (показывается только когда открыта хотя бы 1 папка и есть закладки)
                        leftBookmarksSection = new QWidget(leftSidebarCard);
                        leftBookmarksSection->setStyleSheet("background: transparent;");
                        QVBoxLayout *bmSecLayout = new QVBoxLayout(leftBookmarksSection);
                        bmSecLayout->setContentsMargins(0, 0, 0, 0);
                        bmSecLayout->setSpacing(Theme::kLeftBarPadding);
                        QLabel *bmHeader = new QLabel("Bookmarks", leftBookmarksSection);
                        bmHeader->setObjectName("SidebarSectionTitle");
                        bmHeader->setAlignment(Qt::AlignCenter);
                        bmSecLayout->addWidget(bmHeader);
                        QWidget *bmItemsHolder = new QWidget(leftBookmarksSection);
                        bmItemsHolder->setStyleSheet("background: transparent;");
                        leftBookmarksListLayout = new QVBoxLayout(bmItemsHolder);
                        leftBookmarksListLayout->setContentsMargins(4, 0, 4, 0);
                        leftBookmarksListLayout->setSpacing(4);
                        bmSecLayout->addWidget(bmItemsHolder);
                        // Горизонтальная линия ровно 160px, отцентрированная по горизонтали левого бара
                        QHBoxLayout *lineRow = new QHBoxLayout();
                        lineRow->setContentsMargins(0, 0, 0, 0);
                        QFrame *divider = new QFrame(leftBookmarksSection);
                        divider->setFixedSize(Theme::kLeftDividerWidth, 1);
                        divider->setStyleSheet("background-color: rgba(0, 0, 0, 65);");
                        lineRow->addStretch(1);
                        lineRow->addWidget(divider);
                        lineRow->addStretch(1);
                        bmSecLayout->addLayout(lineRow);
                        sideLayout->addWidget(leftBookmarksSection);
                        // Заголовок Mounted devices по центру левого бара
                        QLabel *devHeader = new QLabel("Mounted devices", leftSidebarCard);
                        devHeader->setObjectName("SidebarSectionTitle");
                        devHeader->setAlignment(Qt::AlignCenter);
                        sideLayout->addWidget(devHeader);
                        // Дерево папок (отступ 16px от Mounted devices обеспечивается spacing=16)
                        ui->treeView->setStyleSheet(
                        QString(
                        "QTreeView {"
                        "  background: transparent;"
                        "  border: none;"
                        "  color: #1C1C1E;"
                        "  font-family: %1;"
                        "  font-size: 12px;"
                        "  outline: none;"
                        "}"
                        "QTreeView::item {"
                        "  height: 24px;"
                        "  border-radius: 6px;"
                        "  padding-left: 2px;"
                        "}"
                        "QTreeView::item:selected {"
                        "  background-color: rgba(255, 255, 255, 130);"
                        "  color: #111111;"
                        "}"
                        "QTreeView::item:hover:!selected {"
                        "  background-color: rgba(255, 255, 255, 75);"
                        "}"
                        ).arg(Theme::fontFamily())
                        );
                        sideLayout->addWidget(ui->treeView, 1);
                        ui->splitter->insertWidget(0, leftSidebarCard);
                        rebuildLeftBookmarks();
                        }
                        void MainWindow::rebuildLeftBookmarks()
                        {
                        if (!leftBookmarksListLayout) return;
                        QLayoutItem *item = nullptr;
                        while ((item = leftBookmarksListLayout->takeAt(0)) != nullptr) {
                        if (item->widget()) item->widget()->deleteLater();
                        delete item;
                        }
                        for (const QString &path : bookmarks) {
                        QFileInfo info(path);
                        if (!info.exists()) continue;
                        QPushButton *btn = new QPushButton(leftBookmarksSection);
                        btn->setCursor(Qt::PointingHandCursor);
                        btn->setIconSize(QSize(20, 20));
                        QString label = info.completeBaseName();
                        if (info.isDir() || label.isEmpty()) label = info.fileName();
                        if (label.isEmpty()) label = path;
                        btn->setText("  " + label);
                        if (iconProvider) {
                        btn->setIcon(iconProvider->icon(info));
                        }
                        btn->setStyleSheet(
                        QString(
                        "QPushButton {"
                        "  text-align: left;"
                        "  background: transparent;"
                        "  border: none;"
                        "  border-radius: 6px;"
                        "  color: #1C1C1E;"
                        "  font-family: %1;"
                        "  font-size: 12px;"
                        "  padding: 4px 6px;"
                        "}"
                        "QPushButton:hover {"
                        "  background-color: rgba(255, 255, 255, 85);"
                        "}"
                        ).arg(Theme::fontFamily())
                        );
                        connect(btn, &QPushButton::clicked, this, [this, path]() {
                        QFileInfo fi(path);
                        if (fi.isDir()) {
                        openPathInActivePane(path);
                        } else {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                        }
                        });
                        leftBookmarksListLayout->addWidget(btn);
                        }
                        }
                        // ==========================================
                        // Интерактивные «хлебные крошки» (шаг 4px, радиус 12px, #B8B8B8 90%)
                        // ==========================================
                        void MainWindow::setupBreadcrumbs()
                        {
                        if (ui->lineEdit) ui->lineEdit->hide();
                        if (ui->pushButton) ui->pushButton->hide();
                        if (ui->label) ui->label->hide();
                        breadcrumbContainer = new QWidget(this);
                        breadcrumbContainer->setObjectName("BreadcrumbBar");
                        breadcrumbContainer->setFixedHeight(Theme::kBreadcrumbHeight);
                        breadcrumbContainer->setStyleSheet("#BreadcrumbBar { background: transparent; }");
                        breadcrumbContainer->installEventFilter(this);
                        breadcrumbLayout = new QHBoxLayout(breadcrumbContainer);
                        breadcrumbLayout->setContentsMargins(0, 0, 0, 0);
                        breadcrumbLayout->setSpacing(Theme::kCrumbSpacing);
                        breadcrumbLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                        manualPathEdit = new QLineEdit(breadcrumbContainer);
                        manualPathEdit->setFixedHeight(Theme::kBreadcrumbHeight);
                        manualPathEdit->setPlaceholderText("Введите путь и нажмите Enter...");
                        manualPathEdit->setStyleSheet(
                        QString(
                        "QLineEdit {"
                        "  background-color: %1;"
                        "  border: 1px solid #007AFF;"
                        "  border-radius: 12px;"
                        "  color: #1C1C1E;"
                        "  font-size: 12px;"
                        "  padding: 2px 12px;"
                        "}"
                        ).arg(Theme::surfaceCss())
                        );
                        manualPathEdit->hide();
                        connect(manualPathEdit, &QLineEdit::returnPressed, this, [this]()  {
                        QString typed = manualPathEdit->text().trimmed();
                        manualPathEdit->hide();
                        openPathInActivePane(typed);
                        });
                        connect(manualPathEdit, &QLineEdit::editingFinished, this, [this]()  {
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
                        btn->setFixedHeight(Theme::kBreadcrumbHeight);
                        btn->setStyleSheet(
                        QString(
                        "QPushButton {"
                        "  background-color: %1;"
                        "  border: none;"
                        "  border-radius: 12px;"
                        "  color: #111111;"
                        "  font-size: 12px;"
                        "  font-weight: 500;"
                        "  padding: 2px 14px;"
                        "}"
                        "QPushButton:hover {"
                        "  background-color: rgba(215, 215, 215, 240);"
                        "}"
                        ).arg(Theme::surfaceCss())
                        );
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
                        // Стартовый экран Bookmarks (при n = 0: надпись Bookmarks по центру #000000 35%, + справа сверху)
                        // ==========================================
                        void MainWindow::setupBookmarksBoard()
                        {
                        rightAreaContainer = new QWidget(this);
                        rightAreaContainer->setStyleSheet("background: transparent;");
                        rightAreaLayout = new QVBoxLayout(rightAreaContainer);
                        rightAreaLayout->setContentsMargins(0, 0, 0, 0);
                        rightAreaLayout->setSpacing(Theme::kPaneSpacing);
                        if (breadcrumbContainer) {
                        breadcrumbContainer->setParent(rightAreaContainer);
                        rightAreaLayout->addWidget(breadcrumbContainer);
                        }
                        panesHolder = new QWidget(rightAreaContainer);
                        panesHolder->setStyleSheet("background: transparent;");
                        panesHolder->installEventFilter(this);
                        rightAreaLayout->addWidget(panesHolder, 1);
                        bookmarksBoard = new QWidget(rightAreaContainer);
                        bookmarksBoard->setObjectName("BookmarksBoardCard");
                        bookmarksBoard->setStyleSheet(
                        QString(
                        "#BookmarksBoardCard {"
                        "  background-color: %1;"
                        "  border-radius: 12px;"
                        "}"
                        ).arg(Theme::surfaceCss())
                        );
                        // Водяной знак "Bookmarks" строго по центру вертикали/горизонтали (#000000 @ 35%)
                        QLabel *centerWatermark = new QLabel("Bookmarks", bookmarksBoard);
                        centerWatermark->setObjectName("CenterWatermark");
                        centerWatermark->setAlignment(Qt::AlignCenter);
                        centerWatermark->setAttribute(Qt::WA_TransparentForMouseEvents);
                        centerWatermark->setStyleSheet(
                        QString(
                        "QLabel#CenterWatermark {"
                        "  color: rgba(0, 0, 0, 89);"
                        "  font-family: %1;"
                        "  font-size: 22px;"
                        "  font-weight: 400;"
                        "  background: transparent;"
                        "}"
                        ).arg(Theme::fontFamily())
                        );
                        QVBoxLayout *boardVBox = new QVBoxLayout(bookmarksBoard);
                        boardVBox->setContentsMargins(20, 14, 14, 60);
                        boardVBox->setSpacing(12);
                        QHBoxLayout *headerRow = new QHBoxLayout();
                        headerRow->setContentsMargins(0, 0, 0, 0);
                        headerRow->addStretch(1);
                        QPushButton *addBookmarkBtn = new QPushButton("+", bookmarksBoard);
                        addBookmarkBtn->setFixedSize(30, 30);
                        addBookmarkBtn->setCursor(Qt::PointingHandCursor);
                        addBookmarkBtn->setToolTip("Добавить в избранное");
                        addBookmarkBtn->setStyleSheet(
                        "QPushButton {"
                        "  background: transparent;"
                        "  border: none;"
                        "  border-radius: 8px;"
                        "  color: #111111;"
                        "  font-size: 20px;"
                        "  font-weight: 400;"
                        "}"
                        "QPushButton:hover {"
                        "  background-color: rgba(255, 255, 255, 85);"
                        "}"
                        "QPushButton::menu-indicator { image: none; width: 0px; height: 0px; }"
                        );
                        QMenu *addBmMenu = new QMenu(addBookmarkBtn);
                        QAction *addFolderAct = addBmMenu->addAction("Добавить папку...");
                        QAction *addAppAct = addBmMenu->addAction("Добавить программу или файл...");
                        addBookmarkBtn->setMenu(addBmMenu);
                        connect(addFolderAct, &QAction::triggered, this,[this]() {
                        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для избранного", QDir::homePath());
                        if (!dir.isEmpty()) addBookmark(dir);
                        });
                        connect(addAppAct, &QAction::triggered, this, [this]() {
                        QString file = QFileDialog::getOpenFileName(this, "Выберите программу или файл", QDir::homePath());
                        if (!file.isEmpty()) addBookmark(file);
                        });
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
                        bookmarksBoard->installEventFilter(this);
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
                        void MainWindow::updateWorkspaceState(bool animatePanes)
                        {
                        int activePanesCount = 0;
                        for (FilePane *p : panes) {
                        if (!p->isDetached()) activePanesCount++;
                        }
                        if (activePanesCount == 0) {
                        // При n = 0 в левом баре НЕТ Bookmarks и приложений, хлебных крошек НЕТ
                        if (leftBookmarksSection) leftBookmarksSection->hide();
                        if (breadcrumbContainer) breadcrumbContainer->hide();
                        if (panesHolder) panesHolder->hide();
                        if (bookmarksBoard) {
                        rebuildBookmarksBoard();
                        bookmarksBoard->show();
                        }
                        } else {
                        // При n >= 1 избранное появляется в левом баре (если добавлено)
                        if (leftBookmarksSection) {
                        rebuildLeftBookmarks();
                        leftBookmarksSection->setVisible(!bookmarks.isEmpty());
                        }
                        if (bookmarksBoard) bookmarksBoard->hide();
                        if (breadcrumbContainer) breadcrumbContainer->show();
                        if (panesHolder) panesHolder->show();
                        }
                        layoutPanesAnimated(animatePanes);
                        updatePaneHighlights();
                        updateSearchBarGeometry();
                        }
                        // ==========================================
                        // Поиск (1/6 высоты правого бара от нижнего края, центр по горизонтали) и фон
                        // ==========================================
                        void MainWindow::resizeEvent(QResizeEvent *event)
                        {
                        QMainWindow::resizeEvent(event);
                        // Позиционируем нижний бар 38px вплотную к низу, левому и правому краям

                        layoutPanesAnimated(false);
                        updateSearchBarGeometry();
                        }
                        void MainWindow::updateSearchBarGeometry()
                        {
                        if (!searchContainer || !rightAreaContainer) return;
                        QPoint rightTopLeft = rightAreaContainer->mapTo(this, QPoint(0, 0));
                        int rightW = rightAreaContainer->width();
                        int rightH = rightAreaContainer->height();
                        if (rightW <= 0 || rightH <= 0) return;
                        int barWidth = qBound(240, qRound(rightW * 0.44), 600);
                        int barHeight = 36;
                        int leftX = rightTopLeft.x() + (rightW - barWidth) / 2;
                        // По вертикали: на расстоянии 1/6 высоты правого бара от его нижнего края
                        int bottomOffset = qMax(16, rightH / 6);
                        int topY = rightTopLeft.y() + rightH - bottomOffset - barHeight / 2;
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
                        if (watched == panesHolder && event->type() == QEvent::Resize) {
                        layoutPanesAnimated(false);
                        return false;
                        }
                        if (watched == bookmarksBoard && event->type() == QEvent::Resize) {
                        if (auto *wm = bookmarksBoard->findChild<QLabel*>("CenterWatermark")) {
                        wm->setGeometry(bookmarksBoard->rect());
                        }
                        return false;
                        }
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
                        painter.setRenderHint(QPainter::Antialiasing, true);
                        painter.setCompositionMode(QPainter::CompositionMode_Source);
                        // Область под барами: цвет #0F0F0F с прозрачностью 21%
                        QPainterPath fullWinPath;
                        fullWinPath.addRoundedRect(rect(), Theme::kCardRadius, Theme::kCardRadius);
                        painter.fillRect(rect(), QColor(0, 0, 0, 0));
                        painter.fillPath(fullWinPath, Theme::windowUnderlayColor());
                        QMainWindow::paintEvent(event);
                        }
