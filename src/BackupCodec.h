#pragma once
#include <QVariantMap>
#include <QVariantList>

namespace BackupCodec {
struct ReadResult { QVariantMap data; QString error; };
ReadResult read(const QByteArray &json);
QString folderPath(const QString &reference,const QVariantList &folders,const QVariantList &watched,const QVariantMap &aliases);
QVariantMap match(const QVariantMap &item,const QVariantList &files,const QVariantList &folders,const QVariantList &watched,const QVariantMap &aliases);
}
