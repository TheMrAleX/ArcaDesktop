#pragma once
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

namespace TrackChoice {
QString language(const QVariant &value);
QString languageName(const QVariant &value);
QString normalizeId(QString id);
QString encode(const QVariantMap &track);
int pick(const QVariantList &options, const QString &saved);
int preferred(const QVariantList &options, const QStringList &languages, bool forcedOnly = false);
}
