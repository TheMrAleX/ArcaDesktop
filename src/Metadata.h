#pragma once
#include <QVariantList>
#include <QVariantMap>

namespace Metadata {
QVariantList ranges(const QVariantList &counts);
QVariantMap absoluteEpisode(int number, const QVariantList &ranges);
bool looksAbsolute(const QVariantList &files, const QVariantList &ranges);
bool covers(const QVariantList &files, const QVariantList &counts);
int structureScore(const QVariantList &files, const QVariantList &counts);
QVariantList wikipedia(const QString &wikitext);
QVariantList tvmaze(const QVariantList &episodes);
QVariantList overlayWikipedia(QVariantList episodes, const QVariantList &text);
QVariantList candidates(const QVariantList &hits, const QString &title, int year);
QString plainHtml(QString value);
}
