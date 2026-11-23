#ifndef THUMBNAILICONPROVIDER_H
#define THUMBNAILICONPROVIDER_H

#include <QFileIconProvider>
#include <QString>
#include <QHash>
#include <QIcon>

class ThumbnailIconProvider : public QFileIconProvider
{
public:
    ThumbnailIconProvider() = default;

    void setActivePath(const QString &path);
    QIcon icon(const QFileInfo &info) const override;

private:
    QString m_activeRoot;
    mutable QHash<QString, QIcon> m_cache;
};

#endif // THUMBNAILICONPROVIDER_H
