#include "filepane.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QInputDialog>


FilePane::FilePane(QFileSystemModel *model, QWidget *parent)
    : QFrame(parent), m_model(model)
{
    setObjectName("FilePaneCard");
    setFrameShape(QFrame::NoFrame);
    updateCardStyle();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ==========================================
    // Шапка карточки панели (как в макете Figma)
    // ==========================================
    m_headerBar = new QWidget(this);
    m_headerBar->setObjectName("PaneHeaderBar");
    m_headerBar->setFixedHeight(28);
    m_headerBar->setStyleSheet(
        "#PaneHeaderBar {"
        "  background-color: #EAE8E1;"
        "  border-radius: 7px;"
        "}"
    );
    m_headerBar->installEventFilter(this);

    QHBoxLayout *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(8, 0, 6, 0);
    headerLayout->setSpacing(4);

    // Невидимый спейсер слева такой же ширины, как кнопка закрытия справа,
    // чтобы заголовок папки стоял строго по центру карточки до пикселя
    QWidget *leftSpacer = new QWidget(m_headerBar);
    leftSpacer->setFixedSize(20, 20);

    m_titleLabel = new QLabel("Компьютер", m_headerBar);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "  color: #2C2C2E;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  background: transparent;"
        "}"
    );
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_closeBtn = new QPushButton("×", m_headerBar);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setToolTip("Закрыть панель");
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "  color: #555555;"
        "  font-size: 15px;"
        "  font-weight: bold;"
        "  border-radius: 4px;"
        "  padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(0, 0, 0, 22);"
        "  color: #111111;"
        "}"
    );
    m_closeBtn->hide();

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested(this);
    });

    headerLayout->addWidget(leftSpacer);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(m_closeBtn);

    layout->addWidget(m_headerBar);

    // Общий стиль для таблиц и сетки значков внутри карточки
    QString viewStyle =
        "QAbstractItemView {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "  color: #1C1C1E;"
        "}"
        "QListView::item {"
        "  border-radius: 10px;"
        "  padding: 4px;"
        "}"
        "QListView::item:selected {"
        "  background-color: rgba(165, 203, 238, 175);"
        "  color: #111111;"
        "}"
        "QListView::item:hover:!selected {"
        "  background-color: rgba(0, 0, 0, 10);"
        "}"
        "QTableView::item:selected {"
        "  background-color: rgba(165, 203, 238, 175);"
        "  color: #111111;"
        "}"
        "QHeaderView::section {"
        "  background-color: #ECEAE3;"
        "  color: #444444;"
        "  border: none;"
        "  border-right: 1px solid #DCDAD2;"
        "  padding: 4px 6px;"
        "  font-size: 11px;"
        "}";

    // 1. Режим таблицы
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setStyleSheet(viewStyle);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(24);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setShowGrid(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setColumnWidth(0, 220);

    // 2. Режим сетки крупных значков
    m_iconView = new QListView(this);
    m_iconView->setModel(m_model);
    m_iconView->setStyleSheet(viewStyle);
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setWordWrap(true);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setSpacing(10);
    m_iconView->hide();

    setupView(m_tableView);
    setupView(m_iconView);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    layout->addWidget(m_tableView, 1);
    layout->addWidget(m_iconView, 1);

    m_currentView = m_tableView;
    updateGridMetrics();
}

void FilePane::setupView(QAbstractItemView *view)
{
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setAcceptDrops(true);
    view->viewport()->setAcceptDrops(true);
    view->viewport()->installEventFilter(this);

    connect(view, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &index) {
        emit activated(this);
        if (!index.isValid()) return;
        QString path = m_model->filePath(index);
        if (QFileInfo(path).isDir()) {
            setCurrentPath(path);
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    connect(view, &QAbstractItemView::customContextMenuRequested, this, [this](const QPoint &pos) {
        emit activated(this);
        emit contextMenuRequested(this, pos);
    });
}

void FilePane::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;

    if (m_viewMode == TableMode) {
        m_iconView->hide();
        m_tableView->show();
        m_currentView = m_tableView;
    } else {
        m_tableView->hide();
        m_iconView->show();
        m_currentView = m_iconView;
        updateGridMetrics();
    }

    QModelIndex idx = m_model->index(m_currentPath);
    if (idx.isValid() || m_currentPath.isEmpty()) {
        m_currentView->setRootIndex(idx);
    }
}

void FilePane::setIconSizePx(int size)
{
    m_iconSizePx = qBound(24, size, 192);
    updateGridMetrics();
}

void FilePane::updateGridMetrics()
{
    m_iconView->setIconSize(QSize(m_iconSizePx, m_iconSizePx));
    int cellW = qMax(96, m_iconSizePx + 40);
    int cellH = m_iconSizePx + 44;
    m_iconView->setGridSize(QSize(cellW, cellH));
    m_iconView->doItemsLayout();
}

void FilePane::setCurrentPath(const QString &path)
{
    m_currentPath = path;
    QModelIndex idx = m_model->index(path);
    if (idx.isValid() || path.isEmpty()) {
        m_tableView->setRootIndex(idx);
        m_iconView->setRootIndex(idx);
    }

    // Обновляем заголовок по центру шапки карточки
    if (path.isEmpty()) {
        m_titleLabel->setText("Компьютер");
    } else {
        QFileInfo info(path);
        QString title = info.fileName();
        if (title.isEmpty()) {
            title = QDir::toNativeSeparators(path); // Для корня диска вроде C:
        }
        m_titleLabel->setText(title);
    }

    emit pathChanged(m_currentPath);
}

void FilePane::setActive(bool active)
{
    m_isActive = active;
    updateCardStyle();
}

void FilePane::setClosable(bool closable)
{
    if (m_closeBtn) {
        m_closeBtn->setVisible(closable);
    }
}

void FilePane::updateCardStyle()
{
    if (m_isActive) {
        setStyleSheet(
            "#FilePaneCard {"
            "  background-color: #F4F3EF;"
            "  border: 2px solid #5B9BD5;"
            "  border-radius: 12px;"
            "}"
        );
    } else {
        setStyleSheet(
            "#FilePaneCard {"
            "  background-color: #F4F3EF;"
            "  border: 1px solid rgba(255, 255, 255, 160);"
            "  border-radius: 12px;"
            "}"
        );
    }
}

void FilePane::highlightMatches(const QString &query)
{
    QItemSelectionModel *selModel = m_currentView->selectionModel();
    if (!selModel) return;

    selModel->clearSelection();

    QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) return;

    QModelIndex rootIdx = m_currentView->rootIndex();
    int rows = m_model->rowCount(rootIdx);
    bool scrolled = false;

    QItemSelectionModel::SelectionFlags flags = (m_viewMode == TableMode)
        ? (QItemSelectionModel::Select | QItemSelectionModel::Rows)
        : QItemSelectionModel::Select;

    for (int row = 0; row < rows; ++row) {
        QModelIndex idx = m_model->index(row, 0, rootIdx);
        if (!idx.isValid()) continue;

        QString fileName = m_model->fileName(idx);
        if (fileName.contains(trimmed, Qt::CaseInsensitive)) {
            selModel->select(idx, flags);
            if (!scrolled) {
                m_currentView->scrollTo(idx);
                scrolled = true;
            }
        }
    }
}

bool FilePane::eventFilter(QObject *watched, QEvent *event)
{
    // Перетаскивание панели за верхнюю шапку (m_headerBar)
    if (watched == m_headerBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            emit activated(this);
            if (me->button() == Qt::LeftButton) {
                startPaneDrag(me->globalPos());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (m_isDraggingPane) {
                updatePaneDrag(me->globalPos());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_isDraggingPane) {
                auto *me = static_cast<QMouseEvent*>(event);
                bool wasSpawned = m_ghostSpawned;
                endPaneDrag(me->globalPos());
                if (wasSpawned) return true;
            }
        }

                return QFrame::eventFilter(watched, event);
                }
    if (m_isDraggingPane && (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::FocusOut)) {
        m_isDraggingPane = false;
        m_ghostSpawned = false;
        m_isDetached = false;
        emit paneDragStateChanged(this, false);
        setCursor(Qt::ArrowCursor);
        if (m_ghost) {
            m_ghost->hide();
            m_ghost->deleteLater();
            m_ghost = nullptr;
        }
    }

                // События внутри списка файлов
    if (watched == m_tableView->viewport() || watched == m_iconView->viewport()) {
        QAbstractItemView *view = (watched == m_iconView->viewport())
            ? static_cast<QAbstractItemView*>(m_iconView)
            : static_cast<QAbstractItemView*>(m_tableView);

        // 0. Ctrl + Колесо мыши
        if (event->type() == QEvent::Wheel) {
            auto *we = static_cast<QWheelEvent*>(event);

                if (we->modifiers() & Qt::ControlModifier) {
                emit activated(this);
                int delta = we->angleDelta().y();
                if (delta != 0) {
                int step = (delta > 0) ? 12 : -12;
                if (m_viewMode == TableMode && delta > 0) {
                setViewMode(IconMode);
                setIconSizePx(48);
                } else if (m_viewMode == IconMode) {
                int nextSize = m_iconSizePx + step;
                if (nextSize < 32 && delta < 0) {
                setViewMode(TableMode);
                } else {
                setIconSizePx(nextSize);
                }
                }
                }
                return true;
                }
                }
                // 1. Нажатие кнопки мыши
                else if (event->type() == QEvent::MouseButtonPress) {
                auto me = static_cast<QMouseEvent*>(event);
                emit activated(this);
                if (me->button() == Qt::LeftButton) {
                QModelIndex idx = view->indexAt(me->pos());
                if (idx.isValid()) {
                m_isDraggingFile = true;
                m_isDraggingPane = false;
                m_dragStartPos = me->globalPos();
                m_draggedFilePath = m_model->filePath(idx);
                } else {
                view->clearSelection();
                m_isDraggingFile = false;
                startPaneDrag(me->globalPos());
                }
                }
                }
                // 2. Движение мыши с зажатой кнопкой
                else if (event->type() == QEvent::MouseMove) {
                auto me = static_cast<QMouseEvent*>(event);
                if (m_isDraggingFile && (me->buttons() & Qt::LeftButton)) {
                if ((me->globalPos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                m_isDraggingFile = false;
                QMimeData *mimeData = new QMimeData;
                mimeData->setUrls({ QUrl::fromLocalFile(m_draggedFilePath) });
                QDrag *drag = new QDrag(this);
                drag->setMimeData(mimeData);
                drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
                return true;
                }
                } else if (m_isDraggingPane) {
                updatePaneDrag(me->globalPos());
                return true;
                }
                }
                // 3. Отпускание кнопки мыши
                else if (event->type() == QEvent::MouseButtonRelease) {
                auto me = static_cast<QMouseEvent*>(event);
                m_isDraggingFile = false;
                if (m_isDraggingPane) {
                endPaneDrag(me->globalPos());
                return true;
                }
                }
                // 4. Вход перетаскиваемого файла в область панели
                else if (event->type() == QEvent::DragEnter) {
                auto de = static_cast<QDragEnterEvent*>(event);
                if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
                }
                }
                // 5. Перемещение файла над панелью
                else if (event->type() == QEvent::DragMove) {
                auto dme = static_cast<QDragMoveEvent*>(event);
                if (dme->mimeData()->hasUrls()) {
                QModelIndex idx = view->indexAt(dme->pos());
                if (idx.isValid() && QFileInfo(m_model->filePath(idx)).isDir()) {
                view->setCurrentIndex(idx);
                } else {
                view->clearSelection();
                }
                if (dme->keyboardModifiers() & Qt::ControlModifier) {
                dme->setDropAction(Qt::CopyAction);
                } else {
                dme->setDropAction(Qt::MoveAction);
                }
                dme->accept();
                return true;
                }
                }
                // 6. Сброс файла (Drop)
                else if (event->type() == QEvent::Drop) {
                auto dropEvent = static_cast<QDropEvent*>(event);
                if (!dropEvent->mimeData()->hasUrls()) return false;
                emit activated(this);
                QString targetDir = m_currentPath;
                QModelIndex idx = view->indexAt(dropEvent->pos());
                if (idx.isValid()) {
                QString hoveredPath = m_model->filePath(idx);
                if (QFileInfo(hoveredPath).isDir()) {
                targetDir = hoveredPath;
                }
                }
                if (targetDir.isEmpty()) return false;
                bool isCopy = (dropEvent->keyboardModifiers() & Qt::ControlModifier)
                || (dropEvent->dropAction() == Qt::CopyAction);
                performFileDrop(dropEvent->mimeData()->urls(), targetDir, isCopy);
                dropEvent->acceptProposedAction();
                return true;
                }
                }
                return QFrame::eventFilter(watched, event);
                }
                void FilePane::performFileDrop(const QList<QUrl> &urls, const QString &targetDir, bool isCopy)
                {
                QDir destDir(targetDir);
                if (!destDir.exists()) return;
                for (const QUrl &url : urls) {
                if (!url.isLocalFile()) continue;
                QString srcPath = QDir::cleanPath(url.toLocalFile());
                QFileInfo srcInfo(srcPath);
                if (!srcInfo.exists()) continue;
                QString dstPath = QDir::cleanPath(destDir.filePath(srcInfo.fileName()));
                if (srcPath == dstPath) continue;
                if (srcInfo.isDir() && dstPath.startsWith(srcPath + "/")) {
                QMessageBox::warning(this, "Ошибка", "Нельзя перенести папку саму в себя.");
                continue;
                }
                if (QFileInfo::exists(dstPath)) {
                auto reply = QMessageBox::question(
                this,
                "Подтверждение",
                QString("«%1» уже существует в папке назначения. Заменить?").arg(srcInfo.fileName()),
                QMessageBox::Yes | QMessageBox::No
                );
                if (reply != QMessageBox::Yes) continue;
                if (QFileInfo(dstPath).isDir()) {
                QDir(dstPath).removeRecursively();
                } else {
                QFile::remove(dstPath);
                }
                }
                bool ok = false;
                if (isCopy) {
                ok = srcInfo.isDir() ? copyRecursively(srcPath, dstPath) : QFile::copy(srcPath, dstPath);
                } else {
                ok = QFile::rename(srcPath, dstPath);
                if (!ok) {
                ok = srcInfo.isDir() ? copyRecursively(srcPath, dstPath) : QFile::copy(srcPath, dstPath);
                if (ok) {
                if (srcInfo.isDir()) {
                QDir(srcPath).removeRecursively();
                } else {
                QFile::remove(srcPath);
                }
                }
                }
                }
                if (!ok) {
                QMessageBox::warning(this, "Ошибка", QString("Не удалось обработать «%1».").arg(srcInfo.fileName()));
                }
                }
                }
                bool FilePane::copyRecursively(const QString &srcPath, const QString &dstPath)
                {
                QFileInfo srcInfo(srcPath);
                if (srcInfo.isDir()) {
                QDir dstDir(dstPath);
                if (!dstDir.exists() && !QDir().mkpath(dstPath)) {
                return false;
                }
                QDir srcDir(srcPath);
                const QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
                for (const QString &entry : entries) {
                if (!copyRecursively(srcDir.filePath(entry), dstDir.filePath(entry))) {
                return false;
                }
                }
                return true;
                } else {
                return QFile::copy(srcPath, dstPath);
                }
                }
                void FilePane::mousePressEvent(QMouseEvent *event)
                {
                emit activated(this);
                if (event->button() == Qt::LeftButton) {
                startPaneDrag(event->globalPos());
                }
                QFrame::mousePressEvent(event);
                }
                void FilePane::mouseMoveEvent(QMouseEvent *event)
                {
                if (m_isDraggingPane) {
                updatePaneDrag(event->globalPos());
                }
                QFrame::mouseMoveEvent(event);
                }
                void FilePane::mouseReleaseEvent(QMouseEvent *event)
                {
                if (m_isDraggingPane) {
                endPaneDrag(event->globalPos());
                }
                QFrame::mouseReleaseEvent(event);
                }
                FloatingPaneGhost::FloatingPaneGhost(const QString &folderTitle, const QRect &startRect, const QPoint &cursorPos)
                    : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
                      m_title(folderTitle)
                {
                    setAttribute(Qt::WA_TranslucentBackground);
                    setAttribute(Qt::WA_ShowWithoutActivating);

                    const int targetW = 240;
                    const int targetH = 175;
                    const int pad = 20;

                    m_anchorOffset = QPoint(targetW / 2 + pad, 18 + pad);

                    QRect initialGeo = startRect.adjusted(-pad, -pad, pad, pad);
                    QRect targetGeo(cursorPos - m_anchorOffset, QSize(targetW + pad * 2, targetH + pad * 2));

                    setGeometry(initialGeo);
                    show();

                    m_anim = new QPropertyAnimation(this, "geometry", this);
                    m_anim->setDuration(150);
                    m_anim->setStartValue(initialGeo);
                    m_anim->setEndValue(targetGeo);
                    m_anim->setEasingCurve(QEasingCurve::OutCubic);

                    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
                        m_animFinished = true;
                    });

                    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
                }

                void FloatingPaneGhost::updateCursorPos(const QPoint &cursorPos)
                {
                    QPoint topLeft = cursorPos - m_anchorOffset;
                    if (!m_animFinished && m_anim && m_anim->state() == QAbstractAnimation::Running) {
                        QRect endRect = m_anim->endValue().toRect();
                        endRect.moveTopLeft(topLeft);
                        m_anim->setEndValue(endRect);
                    } else {
                        move(topLeft);
                    }
                    update();
                }

                void FloatingPaneGhost::setDockHighlight(bool canDock)
                {
                    if (m_canDock != canDock) {
                        m_canDock = canDock;
                        update();
                    }
                }

                QRect FloatingPaneGhost::currentCardRect() const
                {
                    return geometry().adjusted(20, 20, -20, -20);
                }

                void FloatingPaneGhost::paintEvent(QPaintEvent *)
                {
                    QPainter p(this);
                    p.setRenderHint(QPainter::Antialiasing, true);

                    QRect cardRect = rect().adjusted(20, 20, -20, -20);

                    // 1. Мягкая тень под карточкой
                    for (int i = 5; i >= 1; --i) {
                        QPainterPath shadowPath;
                        shadowPath.addRoundedRect(cardRect.adjusted(-i, -i + 3, i, i + 3), 12 + i, 12 + i);
                        p.fillPath(shadowPath, QColor(0, 0, 0, 14 - i * 2));
                    }

                    // 2. Молочный фон карточки (#F4F3EF)
                    QPainterPath cardPath;
                    cardPath.addRoundedRect(cardRect, 12, 12);
                    p.fillPath(cardPath, QColor("#F4F3EF"));

                    // 3. Концентрическая шапка внутри (#EAE8E1) + заголовок и крестик ×
                    QRect headerRect(cardRect.left() + 6, cardRect.top() + 6, cardRect.width() - 12, 26);
                    QPainterPath headerPath;
                    headerPath.addRoundedRect(headerRect, 7, 7);
                    p.fillPath(headerPath, QColor("#EAE8E1"));

                    p.setPen(QColor("#2C2C2E"));
                    QFont titleFont("Segoe UI", 9, QFont::Medium);
                    p.setFont(titleFont);
                    p.drawText(headerRect.adjusted(22, 0, -22, 0), Qt::AlignCenter, m_title);

                    p.setPen(QColor("#555555"));
                    QFont closeFont("Segoe UI", 10, QFont::Bold);
                    p.setFont(closeFont);
                    p.drawText(QRect(headerRect.right() - 22, headerRect.top(), 20, headerRect.height()), Qt::AlignCenter, "×");

                    // 4. Аккуратная иконка папки по центру карточки (без сплющивания!)
                    QPixmap folderIcon(":/icons/icons/folder.png");
                    if (!folderIcon.isNull()) {
                        int iconSize = 48;
                        QRect iconRect(
                            cardRect.center().x() - iconSize / 2,
                            headerRect.bottom() + (cardRect.bottom() - headerRect.bottom() - iconSize) / 2,
                            iconSize,
                            iconSize
                        );
                        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                        p.drawPixmap(iconRect, folderIcon);
                    }

                    // 5. Тонкая контурная рамка
                    if (m_canDock) {
                        p.setPen(QPen(QColor("#007AFF"), 2));
                    } else {
                        p.setPen(QPen(QColor(0, 0, 0, 50), 1));
                    }
                    p.drawRoundedRect(cardRect, 12, 12);
                }

                void FilePane::startPaneDrag(const QPoint &globalPos)
                {
                    m_isDraggingPane = true;
                    m_ghostSpawned = false;
                    m_dragEmitted = false;
                    m_dragStartPos = globalPos;
                }


                void FilePane::updatePaneDrag(const QPoint &globalPos)
                {
                    if (!m_isDraggingPane) return;

                    // Порог начала отрыва — 24 пикселя, чтобы случайный клик не срывал панель
                    if (!m_ghostSpawned) {
                            if ((globalPos - m_dragStartPos).manhattanLength() < 24) {
                                return;
                            }
                            m_ghostSpawned = true;
                            m_isDetached = true;
                            QRect globalCardRect(mapToGlobal(QPoint(0, 0)), size());
                            m_ghost = new FloatingPaneGhost(m_titleLabel->text(), globalCardRect, globalPos);
                            emit paneDragStateChanged(this, true);
                        }
                    if (m_ghost) {
                        m_ghost->updateCursorPos(globalPos);

                        // Проверяем, находится ли курсор над другим окном MainWindow для стыковки
                        bool overTargetWindow = false;
                        QWidget *myWin = window();
                        for (QWidget *w : QApplication::topLevelWidgets()) {
                            if (w != myWin && w->isVisible() && !w->isMinimized() && w->inherits("MainWindow")) {
                                QRect winRect(w->mapToGlobal(QPoint(0, 0)), w->size());
                                if (winRect.contains(globalPos)) {
                                    overTargetWindow = true;
                                    break;
                                }
                            }
                        }
                        m_ghost->setDockHighlight(overTargetWindow);
                    }
                }

                void FilePane::endPaneDrag(const QPoint &globalPos)
                {
                    if (!m_isDraggingPane) return;
                    m_isDraggingPane = false;
                    setCursor(Qt::ArrowCursor);
                    setWindowOpacity(1.0);
                    if (!m_ghostSpawned || !m_ghost) {
                        m_ghostSpawned = false;
                        return;
                    }
                    QRect finalGhostRect = m_ghost->currentCardRect();
                    m_ghost->hide();
                    m_ghost->deleteLater();
                    m_ghost = nullptr;
                    m_ghostSpawned = false;
                    m_isDetached = false;
                    emit paneDragStateChanged(this, false);
                    if (m_dragEmitted) return;
                    m_dragEmitted = true;
                    emit paneDraggedOut(this, globalPos, finalGhostRect);
                }
                // ==========================================
                // Системный буфер обмена (Ctrl+C / Ctrl+X / Ctrl+V) и переименование (F2)
                // ==========================================
                void FilePane::copySelectionToClipboard(bool cut)
                {
                    if (!m_currentView || !m_currentView->selectionModel()) return;

                    QModelIndexList selected = m_currentView->selectionModel()->selectedRows();
                    if (selected.isEmpty()) {
                        selected = m_currentView->selectionModel()->selectedIndexes();
                    }

                    QList<QUrl> urls;
                    QSet<QString> uniquePaths;
                    for (const QModelIndex &idx : selected) {
                        if (idx.column() != 0) continue;
                        QString path = m_model->filePath(idx);
                        if (!path.isEmpty() && !uniquePaths.contains(path)) {
                            uniquePaths.insert(path);
                            urls.append(QUrl::fromLocalFile(path));
                        }
                    }

                    if (urls.isEmpty()) return;

                    QMimeData *mimeData = new QMimeData();
                    mimeData->setUrls(urls);
                    // Специальный флаг вырезания (понимают и Windows Explorer, и Linux файловые менеджеры)
                    mimeData->setData("application/x-myfm-cut", cut ? "1" : "0");

                    QApplication::clipboard()->setMimeData(mimeData);
                }

                void FilePane::pasteFromClipboard()
                {
                    if (m_currentPath.isEmpty()) return;

                    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
                    if (!mimeData || !mimeData->hasUrls()) return;

                    bool isCut = (mimeData->data("application/x-myfm-cut") == "1");
                    QList<QUrl> urls = mimeData->urls();

                    for (const QUrl &url : urls) {
                        if (!url.isLocalFile()) continue;
                        QString srcPath = url.toLocalFile();
                        QFileInfo srcInfo(srcPath);
                        if (!srcInfo.exists()) continue;

                        QString dstPath = QDir(m_currentPath).filePath(srcInfo.fileName());

                        // Если вставляем копию в ту же самую папку — добавляем суффикс " - копия"
                        if (QFileInfo::exists(dstPath)) {
                            if (isCut && QDir::cleanPath(srcPath) == QDir::cleanPath(dstPath)) {
                                continue;
                            }
                            QString base = srcInfo.completeBaseName();
                            QString ext = srcInfo.suffix();
                            int counter = 1;
                            do {
                                QString newName = ext.isEmpty()
                                    ? QString("%1 - копия (%2)").arg(base).arg(counter++)
                                    : QString("%1 - копия (%2).%3").arg(base).arg(counter++).arg(ext);
                                dstPath = QDir(m_currentPath).filePath(newName);
                            } while (QFileInfo::exists(dstPath));
                        }

                        if (isCut) {
                            if (!QFile::rename(srcPath, dstPath)) {
                                if (srcInfo.isDir() ? copyRecursively(srcPath, dstPath) : QFile::copy(srcPath, dstPath)) {
                                    if (srcInfo.isDir()) QDir(srcPath).removeRecursively();
                                    else QFile::remove(srcPath);
                                }
                            }
                        } else {
                            if (srcInfo.isDir()) {
                                copyRecursively(srcPath, dstPath);
                            } else {
                                QFile::copy(srcPath, dstPath);
                            }
                        }
                    }

                    if (isCut) {
                        QApplication::clipboard()->clear();
                    }
                }

                void FilePane::renameSelectedItem()
                {
                    if (!m_currentView) return;
                    QModelIndex idx = m_currentView->currentIndex();
                    if (!idx.isValid()) return;

                    QString path = m_model->filePath(idx);
                    QFileInfo info(path);

                    bool ok = false;
                    QString newName = QInputDialog::getText(this, "Переименовать", "Новое имя:",
                                                            QLineEdit::Normal, info.fileName(), &ok);
                    if (ok && !newName.trimmed().isEmpty() && newName.trimmed() != info.fileName()) {
                        QFile::rename(path, info.dir().filePath(newName.trimmed()));
                    }
                }
