#include "Catalog.h"
#include "TrackChoice.h"
#include <QLocale>
#include <QtTest>

class CatalogTest : public QObject {
    Q_OBJECT
    static QVariantMap episode(int number, qint64 updated = 0, bool watched = false, qint64 position = 0, int season = 1) {
        return {{"kind","EPISODE"},{"title","La Serie"},{"path",QString("/serie/%1-%2.mkv").arg(season).arg(number)},
                {"season",season},{"episode",number},{"confidence",98},{"updated",updated},{"watched",watched},
                {"position",position},{"duration",100000},{"available",true}};
    }
private slots:
    void unopenedShowsStayOutOfContinue() {
        auto result=Catalog::build({episode(2),episode(1)});
        QCOMPARE(result.shows.size(),1);QVERIFY(result.continuing.isEmpty());
        QCOMPARE(result.shows.first().toMap().value("episode").toInt(),1);
        QCOMPARE(result.shows.first().toMap().value("episodeCount").toInt(),2);
    }
    void openingForTwoSecondsDoesNotSkipEpisode() {
        auto result=Catalog::build({episode(1,100,false,2000),episode(2)});
        QCOMPARE(result.continuing.size(),1);QCOMPARE(result.continuing.first().toMap().value("episode").toInt(),1);
    }
    void nextEpisodeAfterWatchedAndAcrossSeasons() {
        auto result=Catalog::build({episode(1,0,false,0,2),episode(2,100,true),episode(1,50,true)});
        auto next=result.continuing.first().toMap();QCOMPARE(next.value("season").toInt(),2);QCOMPARE(next.value("episode").toInt(),1);
        auto show=result.shows.first().toMap();QCOMPARE(show.value("watchedCount").toInt(),2);QCOMPARE(show.value("remaining").toLongLong(),100000);QVERIFY(show.value("inProgress").toBool());
    }
    void hidingSeriesAlsoHidesNextEpisode() {
        auto files=QVariantList{episode(1,100,true),episode(2)};
        QVariantMap hidden{{"s:la serie",200}};
        QVERIFY(Catalog::build(files,hidden).continuing.isEmpty());
        files[1]=episode(2,300,false,50000);
        QCOMPARE(Catalog::build(files,hidden).continuing.first().toMap().value("episode").toInt(),2);
    }
    void hidingMovieUntilPlayedAgain() {
        QVariantMap movie{{"kind","MOVIE"},{"title","Película"},{"path","/movie.mkv"},{"confidence",96},{"position",10000},{"updated",100}};
        QVERIFY(Catalog::build({movie},{{"m:/movie.mkv",200}}).continuing.isEmpty());
        movie["updated"]=300;QCOMPARE(Catalog::build({movie},{{"m:/movie.mkv",200}}).continuing.size(),1);
    }
    void finishedShowCanBeRewatched() {
        auto result=Catalog::build({episode(1,100,true),episode(2,200,true)});
        QVERIFY(result.continuing.isEmpty());QVERIFY(result.shows.first().toMap().value("watched").toBool());
        QCOMPARE(result.shows.first().toMap().value("episode").toInt(),1);
        result=Catalog::build({episode(1,300,false,10000),episode(2,200,true)});
        QCOMPARE(result.continuing.first().toMap().value("episode").toInt(),1);
    }
    void duplicatesUseMostRecentlyPlayedFile() {
        auto alternate=episode(1,100,false,10000);alternate["path"]="/serie/alternate.mkv";
        auto result=Catalog::build({episode(1),alternate,episode(2)});
        QCOMPARE(result.shows.first().toMap().value("episodeCount").toInt(),2);
        QCOMPARE(result.continuing.first().toMap().value("path").toString(),QString("/serie/alternate.mkv"));
    }
    void metadataTitleDoesNotChangeIdentity() {
        auto first=episode(1,100,true),second=episode(2);first["fileTitle"]="Original";second["fileTitle"]="Original";
        first["title"]="Título traducido";second["title"]="Título traducido";
        auto result=Catalog::build({first,second},{{"s:original",200}});
        QCOMPARE(result.shows.size(),1);QVERIFY(result.continuing.isEmpty());
    }
    void exactTrackAndLanguageFallbackMatchAndroid() {
        QVariantList tracks;
        for(const auto &id:{"2","3","4"})tracks.append(QVariantMap{{"lang","spa"},{"id",id},{"forced",false}});
        tracks.append(QVariantMap{{"lang","eng"},{"id","7"},{"forced",false}});
        QCOMPARE(TrackChoice::pick(tracks,"spa||3"),1);
        QCOMPARE(TrackChoice::pick(tracks,"spa||9"),0);
        QCOMPARE(TrackChoice::pick(tracks,"spa||7"),0);
        QCOMPARE(TrackChoice::pick(tracks,"spa"),0);
        QCOMPARE(TrackChoice::pick(tracks,"fra"),-1);
        QCOMPARE(TrackChoice::normalizeId("1:ext:12"),QString("ext:12"));
        QCOMPARE(TrackChoice::normalizeId("0:3"),QString("3"));
    }
    void forcedSubtitlesAndExplicitOff() {
        const QVariantList tracks{QVariantMap{{"lang","spa"},{"id","8"},{"forced",true}},QVariantMap{{"lang","spa"},{"id","9"},{"forced",false}}};
        QCOMPARE(TrackChoice::pick(tracks,"spa"),1);QCOMPARE(TrackChoice::pick(tracks,"spa|f"),0);QCOMPARE(TrackChoice::pick(tracks,"off"),-1);
        QCOMPARE(TrackChoice::preferred(tracks,{"es","en"},true),0);
        QCOMPARE(TrackChoice::encode(tracks.first().toMap()),QString("es|f|8"));
        QCOMPARE(TrackChoice::language(QVariant(int(QLocale::Spanish))),QString("es"));
        QCOMPARE(TrackChoice::language(QVariant(double(QLocale::English))),QString("en"));
        QVERIFY(TrackChoice::language("und").isEmpty());
    }
};
QTEST_GUILESS_MAIN(CatalogTest)
#include "CatalogTest.moc"
