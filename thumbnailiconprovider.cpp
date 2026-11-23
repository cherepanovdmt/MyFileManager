#include "thumbnailiconprovider.h"
#include <QDir>
#include <QImageReader>
#include <QPixmap>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QDateTime>

void ThumbnailIconProvider::setActivePath(const QString &path)
{
    if (path.isEmpty()) {
        m_activeRoot.clear();
    } else {
        m_activeRoot = QStorageInfo(path).rootPath();
    }
}

QIcon ThumbnailIconProvider::icon(const QFileInfo &info) const
{
    if (info.isFile()) {
        static const QStringList imgExts = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico"};
        QString ext = info.suffix().toLower();
        if (imgExts.contains(ext)) {
            QString thumbKey = "thumb:" + info.absoluteFilePath() + ":" + QString::number(info.lastModified().toSecsSinceEpoch());
            if (m_cache.contains(thumbKey)) {
                return m_cache.value(thumbKey);
            }

            QImageReader reader(info.absoluteFilePath());
            reader.setAutoTransform(true);
            reader.setScaledSize(QSize(128, 128));
            QImage img = reader.read();
            if (!img.isNull()) {
                QIcon imgIcon;
                imgIcon.addPixmap(QPixmap::fromImage(img));
                m_cache.insert(thumbKey, imgIcon);
                return imgIcon;
            }
        }
    }

    QString iconName = "floppy";

    if (info.isRoot()) {
        QStorageInfo storage(info.absoluteFilePath());
        QString fsType = QString::fromLatin1(storage.fileSystemType()).toUpper();
        QString devName = QString::fromLatin1(storage.device()).toUpper();

        if (!m_activeRoot.isEmpty() && storage.rootPath() == m_activeRoot) {
            iconName = "drive-active";
        } else if (fsType.contains("CDFS") || fsType.contains("UDF") || fsType.contains("ISO9660")) {
            iconName = "drive-optical";
        } else if (fsType == "FAT32" || fsType == "EXFAT" || devName.contains("USB")) {
            iconName = "drive-usb";
        } else {
            iconName = "drive";
        }
    } else if (info.isDir()) {
        QString absPath = QDir::cleanPath(info.absoluteFilePath());
        if (absPath.compare(QDir::cleanPath(QDir::homePath()), Qt::CaseInsensitive) == 0) {
            iconName = "folder_home";
        } else if (absPath.compare(QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)), Qt::CaseInsensitive) == 0) {
            iconName = "folder_download";
        } else if (absPath.compare(QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)), Qt::CaseInsensitive) == 0) {
            iconName = "desktop";
        } else {
            iconName = "folder";
        }
    } else {
        QString ext = info.suffix().toLower();
        QString baseName = info.completeBaseName().toLower();

        if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") {
            iconName = "7zipa";
        } else if (ext == "doc" || ext == "docx" || ext == "rtf" || ext == "odt" || ext == "txt" || ext == "md") {
            iconName = "WINWORD";
        } else if (ext == "xls" || ext == "xlsx" || ext == "csv" || ext == "ods") {
            iconName = "EXCEL";
        } else if (ext == "py" || ext == "pyw" || ext == "pyc") {
            iconName = "py";
        } else if (ext == "xcf") {
            iconName = "gimp";
        } else if (ext == "psd" || ext == "psb") {
            iconName = "AdobePhotoshop";
        } else if (ext == "ai" || ext == "eps") {
            iconName = "AdobeIllustrator";
        } else if (ext == "prproj") {
            iconName = "AdobePremierePro";
        } else if (ext == "aep") {
            iconName = "AdobeAfterEffects";
        } else if (ext == "indd") {
            iconName = "AdobeInDesign";
        } else if (ext == "lrcat" || ext == "lrtemplate") {
            iconName = "AdobeLightroom";
        } else if (ext == "sesx") {
            iconName = "AdobeAudition";
        } else if (ext == "fla" || ext == "swf") {
            iconName = "AdobeFlash";
        } else if (ext == "html" || ext == "htm" || ext == "css") {
            iconName = "AdobeDreamweaver";
        } else if (ext == "iso" || ext == "img" || ext == "vhd") {
            iconName = "floppy";
        } else if (ext == "bat" || ext == "cmd" || ext == "ps1" || ext == "sh" || ext == "exe" || ext == "msi" || ext == "lnk" || ext == "desktop" || ext == "appimage") {
            if (baseName.contains("chrome")) iconName = "chrome-browser";
            else if (baseName.contains("firefox")) iconName = "firefox";
            else if (baseName.contains("spotify")) iconName = "spotify";
            else if (baseName.contains("gimp")) iconName = "gimp";
            else if (baseName.contains("photoshop")) iconName = "AdobePhotoshop";
            else if (baseName.contains("illustrator")) iconName = "AdobeIllustrator";
            else if (baseName.contains("premiere")) iconName = "AdobePremierePro";
            else if (baseName.contains("after")) iconName = "AdobeAfterEffects";
            else if (baseName.contains("indesign")) iconName = "AdobeInDesign";
            else if (baseName.contains("lightroom")) iconName = "AdobeLightroom";
            else if (baseName.contains("audition")) iconName = "AdobeAudition";
            else if (baseName.contains("bridge")) iconName = "AdobeBridge";
            else if (baseName.contains("dreamweaver")) iconName = "AdobeDreamweaver";
            else if (baseName.contains("encore")) iconName = "AdobeEncore";
            else if (baseName.contains("fireworks")) iconName = "AdobeFireworks";
            else if (baseName.contains("prelude")) iconName = "AdobePrelude";
            else if (baseName.contains("word") || baseName.contains("winword")) iconName = "WINWORD";
            else if (baseName.contains("excel")) iconName = "EXCEL";
            else if (baseName.contains("python")) iconName = "py";
            else iconName = "term";
        }
    }

    if (!m_cache.contains(iconName)) {
        QString resPath = QString(":/icons/icons/%1.png").arg(iconName);
        QIcon loadedIcon(resPath);

        if (loadedIcon.isNull()) {
            loadedIcon = QIcon(":/icons/icons/floppy.png");
            if (loadedIcon.isNull()) {
                loadedIcon = QFileIconProvider::icon(info);
            }
        }
        m_cache.insert(iconName, loadedIcon);
    }

    return m_cache.value(iconName);
}
