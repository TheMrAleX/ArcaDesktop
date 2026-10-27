#pragma once
#include <QQuickImageProvider>
#include <QMutex>
#include <QVariantList>

class ThumbnailProvider : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(QString directory);
    void setFiles(const QVariantList &files);
    QImage requestImage(const QString &id,QSize *size,const QSize &requested) override;
private:
    QString m_directory;
    QMutex m_mutex;
    QHash<QString,QVariantMap> m_files;
};
