#ifndef TOPGLASSBAR_H
#define TOPGLASSBAR_H

#include <QWidget>
#include <QMenu>

class TopGlassBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopGlassBar(QMenu *viewMenu, QMenu *bookmarksMenu, QWidget *parent = nullptr);

signals:
    void newFolderRequested();
    void newPaneRequested();
    void exitRequested();
    void goUpRequested();
    void goHomeRequested();
    void deleteRequested();
    void settingsRequested();
    void helpRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // TOPGLASSBAR_H
