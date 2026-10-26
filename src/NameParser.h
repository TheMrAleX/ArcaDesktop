#pragma once
#include <QString>
#include <QStringList>
#include <QList>

namespace Names {
enum class Kind { Movie, Episode, Unknown, Ignored };
struct Result {
    Kind kind = Kind::Unknown;
    QString title, episodeTitle, reason, compactToken;
    int year = 0, season = -1, episode = 0, episodeEnd = 0, confidence = 0;
    int compactNumber = 0, altSeason = -1, altEpisode = 0, yearHint = 0;
    bool seasonFromFolder = false;
};
QString key(QString value);
QString normalize(QString value);
QString clean(QString value);
bool isVideo(const QString &name);
bool ignoredDir(const QString &name);
int seasonFromDir(const QString &name);
int plainNumber(const QString &name);
int numberHint(const QString &name);
bool plainNumbering(const QList<int> &numbers);
QString resolveCompactGroup(const QList<int> &numbers);
Result parse(const QString &name, const QStringList &dirs = {}, const QString &kind = "AUTO",
             const QString &root = {}, const QString &rule = {}, bool ignoreSamples = true);
}
