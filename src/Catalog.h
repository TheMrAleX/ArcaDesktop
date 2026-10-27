#pragma once
#include <QVariantList>
#include <QVariantMap>

namespace Catalog {
struct View {
    QVariantList shows;
    QVariantList continuing;
};
QString showKey(const QVariantMap &file);
QString continueKey(const QVariantMap &file);
QVariantList markDuplicates(QVariantList files);
View build(const QVariantList &files, const QVariantMap &hidden = {}, bool duplicatesMarked = false);
}
