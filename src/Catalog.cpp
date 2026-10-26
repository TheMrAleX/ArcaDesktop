#include "Catalog.h"
#include "NameParser.h"
#include <QMap>
#include <QSet>
#include <QHash>
#include <algorithm>

QString Catalog::showKey(const QVariantMap &file) {
    return Names::key(file.value("fileTitle", file.value("title")).toString());
}

QString Catalog::continueKey(const QVariantMap &file) {
    return file.value("kind") == "EPISODE" ? "s:" + showKey(file) : "m:" + file.value("path").toString();
}

Catalog::View Catalog::build(const QVariantList &files, const QVariantMap &hidden) {
    View view;
    QMap<QString, QVariantList> grouped;
    auto appendContinue = [&](QVariantMap file, qint64 lastPlayed) {
        const auto key = continueKey(file);
        if (hidden.value(key).toLongLong() >= lastPlayed) return;
        file["updated"] = lastPlayed;
        file["continueKey"] = key;
        view.continuing.append(file);
    };
    for (const auto &value : files) {
        auto file = value.toMap();
        if (file.value("confidence").toInt() < 80) continue;
        if (file.value("kind") == "EPISODE") grouped[showKey(file)].append(file);
        else if (file.value("kind") == "MOVIE" && !file.value("watched").toBool() && file.value("position").toLongLong() > 5000)
            appendContinue(file, file.value("updated").toLongLong());
    }
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        auto episodes = it.value();
        std::stable_sort(episodes.begin(), episodes.end(), [](const QVariant &a, const QVariant &b) {
            const auto left = a.toMap(), right = b.toMap();
            if (left.value("season") != right.value("season")) return left.value("season").toInt() < right.value("season").toInt();
            if (left.value("episode") != right.value("episode")) return left.value("episode").toInt() < right.value("episode").toInt();
            return left.value("path").toString() < right.value("path").toString();
        });
        // Alternate encodings of one episode must not count as extra episodes.
        QVariantList unique;
        QHash<QString,int> seen;
        for (const auto &episode : episodes) {
            const auto file = episode.toMap();
            const auto key = file.value("season").toString() + ':' + file.value("episode").toString();
            if (!seen.contains(key)) { seen.insert(key,unique.size()); unique.append(file); }
            else {
                const auto previous=unique[seen[key]].toMap();
                if(file.value("updated").toLongLong()>previous.value("updated").toLongLong() ||
                   (file.value("updated")==previous.value("updated") && file.value("available").toBool()&&!previous.value("available").toBool()))unique[seen[key]]=file;
            }
        }
        episodes = unique;
        int last = -1, next = -1, watched = 0, width = 0;
        qint64 lastPlayed = 0, remaining = 0, newest = 0;
        QSet<int> seasons;
        for (int index = 0; index < episodes.size(); ++index) {
            const auto file = episodes[index].toMap();
            const auto updated = file.value("updated").toLongLong();
            if (updated > lastPlayed) { lastPlayed = updated; last = index; }
            if (file.value("watched").toBool()) ++watched;
            else remaining += qMax<qint64>(0, qMax(file.value("duration").toLongLong(), file.value("progressDuration").toLongLong()) - file.value("position").toLongLong());
            seasons.insert(file.value("season").toInt());
            width = qMax(width, file.value("width").toInt());
            newest = qMax(newest, file.value("mtime").toLongLong());
        }
        if (last >= 0) {
            if (!episodes[last].toMap().value("watched").toBool()) next = last;
            else for (int index = last + 1; index < episodes.size(); ++index)
                if (!episodes[index].toMap().value("watched").toBool()) { next = index; break; }
        }
        int resume = next;
        if (resume < 0) for (int index = 0; index < episodes.size(); ++index)
            if (!episodes[index].toMap().value("watched").toBool()) { resume = index; break; }
        if (resume < 0) resume = 0;
        auto show = episodes[resume].toMap();
        show["isShow"] = true;
        show["showKey"] = it.key();
        show["continueKey"] = continueKey(show);
        show["episodeCount"] = episodes.size();
        show["watchedCount"] = watched;
        show["watched"] = watched == episodes.size();
        show["inProgress"] = last >= 0 && watched < episodes.size();
        show["remaining"] = remaining;
        show["updated"] = lastPlayed;
        show["width"] = width;
        show["mtime"] = newest;
        QList<int> sortedSeasons(seasons.begin(), seasons.end());
        std::sort(sortedSeasons.begin(), sortedSeasons.end());
        QVariantList seasonList;
        for (int season : sortedSeasons) seasonList.append(season);
        show["seasons"] = seasonList;
        view.shows.append(show);
        if (next >= 0) appendContinue(episodes[next].toMap(), lastPlayed);
    }
    std::stable_sort(view.shows.begin(), view.shows.end(), [](const QVariant &a, const QVariant &b) {
        return QString::localeAwareCompare(a.toMap().value("title").toString(), b.toMap().value("title").toString()) < 0;
    });
    std::stable_sort(view.continuing.begin(), view.continuing.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("updated").toLongLong() > b.toMap().value("updated").toLongLong();
    });
    return view;
}
