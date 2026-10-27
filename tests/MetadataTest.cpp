#include "Library.h"
#include "Metadata.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QUrlQuery>
#include <QTimer>

class MetadataTest : public QObject {
    Q_OBJECT
    static void file(const QString &path){QFile output(path);QVERIFY(output.open(QIODevice::WriteOnly));output.write("fixture");}
private slots:
    void absoluteRangesMatchAndroid(){
        QVariantList counts;for(auto pair:QList<QPair<int,int>>{{1,27},{2,27},{3,27},{4,29},{5,26},{6,31},{7,0}})counts<<QVariantMap{{"season",pair.first},{"count",pair.second}};
        auto ranges=Metadata::ranges(counts);QCOMPARE(ranges.size(),6);QCOMPARE(ranges[3].toMap().value("first").toInt(),82);QCOMPARE(ranges.last().toMap().value("last").toInt(),167);
        auto mapped=Metadata::absoluteEpisode(100,ranges);QCOMPARE(mapped.value("season").toInt(),4);QCOMPARE(mapped.value("episode").toInt(),19);QVERIFY(Metadata::absoluteEpisode(168,ranges).isEmpty());
        QVERIFY(Metadata::looksAbsolute({QVariantMap{{"season",1},{"episode",100}}},ranges));QVERIFY(!Metadata::looksAbsolute({QVariantMap{{"season",1},{"episode",500}}},ranges));
    }
    void wikipediaPreservesHoles(){
        auto parsed=Metadata::wikipedia("{{Lista de episodios\n|Título='''El próximo rey''' ([[Jaden Yuki|Jaden]] vs. Crowler)\n\n|ResumenBreve=Jaden recibe a [[Kuriboh Alado]].<ref>Fuente</ref>\n}}\n{{Lista de episodios\n|Título=\n\n|ResumenBreve=\n}}\n{{Lista de episodios\n|Título=Final\n}}");
        QCOMPARE(parsed.size(),3);QCOMPARE(parsed[0].toMap().value("title").toString(),QString("El próximo rey (Jaden vs. Crowler)"));QVERIFY(parsed[1].toMap().value("title").toString().isEmpty());
        QVariantList base;for(int i=1;i<=4;++i)base<<QVariantMap{{"absolute",i},{"title",QString("English %1").arg(i)},{"overview","Original"}};
        auto overlaid=Metadata::overlayWikipedia(base,parsed);QCOMPARE(overlaid[1].toMap().value("title").toString(),QString("English 2"));QCOMPARE(overlaid[2].toMap().value("title").toString(),QString("Final"));QCOMPARE(overlaid[3].toMap().value("title").toString(),QString("English 4"));
    }
    void realWikipediaFixture(){QFile fixture(QString(ARCA_SOURCE_DIR)+"/tests/fixtures/wikipedia/yu-gi-oh-gx.wikitext");QVERIFY(fixture.open(QIODevice::ReadOnly));auto episodes=Metadata::wikipedia(QString::fromUtf8(fixture.readAll()));QCOMPARE(episodes.size(),180);QCOMPARE(episodes[109].toMap().value("title").toString(),QString("Instinto Primitivo (Hassleberry vs Jim)"));QVERIFY(episodes[109].toMap().value("overview").toString().startsWith("El grupo de Jaden"));}
    void seasonsFallbackAndCacheSurviveNewFiles(){
        QTemporaryDir storage,media;file(media.path()+"/My.Show.S01E01.mkv");file(media.path()+"/My.Show.S01E02.mkv");
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.setSetting("tmdbKey","fixture");db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        QMap<QString,int> imageRequests;QFile png(QString(ARCA_SOURCE_DIR)+"/assets/arca.png");QVERIFY(png.open(QIODevice::ReadOnly));const auto bytes=png.readAll();
        db.setMetadataTransport([&](const QUrl &url,std::function<void(QVariant)> reply){QVariant value;auto path=url.path();bool en=QUrlQuery(url).queryItemValue("language")=="en-US";
            if(url.host()=="image.tmdb.org"){++imageRequests[path];value=bytes;}
            else if(path=="/3/tv/7")value=QVariantMap{{"id",7},{"name","Mi serie"},{"overview","Sinopsis"},{"first_air_date","2020-01-01"},{"poster_path","/poster.jpg"},{"backdrop_path","/backdrop.jpg"},{"genres",QVariantList{QVariantMap{{"name","Drama"}}}},{"seasons",QVariantList{QVariantMap{{"season_number",1},{"episode_count",2}}}}};
            else if(path.endsWith("/season/1"))value=QVariantMap{{"season_number",1},{"poster_path","/season.jpg"},{"episodes",QVariantList{QVariantMap{{"episode_number",1},{"name",en?"Pilot":"Episodio 1"},{"overview",en?"English plot":""},{"still_path","/still.jpg"}},QVariantMap{{"episode_number",2},{"name",en?"Episode 2":"El destino"},{"overview","Sinopsis 2"},{"still_path","/still.jpg"}}}}};
            QTimer::singleShot(0,&db,[reply,value]{reply(value);});
        });
        const auto first=db.files().first().toMap().value("path").toString();db.useMetadata(first,7,true);
        QTRY_COMPARE(db.media(first).value("episodeTitle").toString(),QString("Pilot"));QCOMPARE(db.media(first).value("episodeOverview").toString(),QString("English plot"));
        QTRY_VERIFY(!db.media(first).value("seasonPosters").toMap().value("1").toString().isEmpty());QCOMPARE(imageRequests.value("/t/p/w500/still.jpg"),1);
        file(media.path()+"/My.Show.S01E03.mkv");db.scan();QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QCOMPARE(db.media(media.path()+"/My.Show.S01E03.mkv").value("title").toString(),QString("Mi serie"));
        db.setSetting("spoilerFree",true);QVERIFY(db.media(first).value("episodeTitle").toString().isEmpty());QVERIFY(db.media(first).value("still").toString().isEmpty());
    }
    void alternativeGuideAndSpanishOverlay(){
        QTemporaryDir storage,media;file(media.path()+"/My.Show.S01E03.mkv");file(media.path()+"/My.Show.S02E01.mkv");Library db(storage.path());db.setSetting("scanOnOpen",false);db.setSetting("tmdbKey","fixture");db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        db.setMetadataTransport([&](const QUrl &url,std::function<void(QVariant)> reply){QVariant result;
            if(url.path()=="/3/tv/7")result=QVariantMap{{"id",7},{"name","My Show"},{"overview","Series"},{"seasons",QVariantList{QVariantMap{{"season_number",1},{"episode_count",2}}}}};
            else if(url.path().contains("/season/"))result=QVariantMap{{"season_number",1},{"episodes",QVariantList{QVariantMap{{"episode_number",1},{"name","Pilot"},{"overview","Plot"}}}}};
            else if(url.path()=="/search/shows")result=QVariantList{QVariantMap{{"show",QVariantMap{{"id",42},{"name","My Show"},{"url","https://www.tvmaze.com/shows/42/my-show"}}}}};
            else if(url.path()=="/shows/42/episodes"){QVariantList episodes;for(int n=1;n<=4;++n)episodes<<QVariantMap{{"season",n<=3?1:2},{"number",n<=3?n:1},{"name",QString("English %1").arg(n)},{"summary","<p>Original</p>"},{"runtime",24}};result=episodes;}
            else if(url.host()=="es.wikipedia.org"){const QString text="{{Lista de episodios\n|Título=Uno\n|ResumenBreve=Resumen 1}}\n{{Lista de episodios\n|Título=\n|ResumenBreve=}}\n{{Lista de episodios\n|Título=El tercer episodio\n|ResumenBreve=Resumen español}}";QVariantMap main{{"content",text}};QVariantMap slotData{{"main",main}};QVariantMap revision{{"slots",slotData}};QVariantMap page{{"title","Anexo:Episodios de My Show"},{"revisions",QVariantList{revision}}};result=QVariantMap{{"query",QVariantMap{{"pages",QVariantList{page}}}}};}
            QTimer::singleShot(0,&db,[reply,result]{reply(result);});
        });
        const auto path=media.path()+"/My.Show.S01E03.mkv";db.useMetadata(path,7,true);QTRY_COMPARE(db.media(path).value("episodeTitle").toString(),QString("El tercer episodio"));QCOMPARE(db.media(path).value("episodeOverview").toString(),QString("Resumen español"));QCOMPARE(db.media(media.path()+"/My.Show.S02E01.mkv").value("episodeTitle").toString(),QString("English 4"));QVERIFY(db.media(path).value("episodeSourceUrl").toString().contains("wikipedia.org"));
    }
    void clearingCacheCancelsPendingMetadata(){
        QTemporaryDir storage,media;file(media.path()+"/Film.2024.mkv");Library db(storage.path());db.setSetting("scanOnOpen",false);db.setSetting("tmdbKey","fixture");db.addFolder(QUrl::fromLocalFile(media.path()),"MOVIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        QList<std::function<void(QVariant)>> callbacks;db.setMetadataTransport([&](const QUrl &,std::function<void(QVariant)> reply){callbacks<<reply;});const auto path=media.path()+"/Film.2024.mkv";
        db.downloadMetadata(path);QCOMPARE(callbacks.size(),1);db.clearMetadata();callbacks.takeFirst()(QVariantMap{{"results",QVariantList{QVariantMap{{"id",7},{"title","Film"}}}}});QVERIFY(callbacks.isEmpty());QVERIFY(!db.media(path).contains("tmdbId"));
        db.useMetadata(path,7,false);QCOMPARE(callbacks.size(),1);db.clearMetadata();callbacks.takeFirst()(QVariantMap{{"id",7},{"title","Incorrect"},{"overview","Plot"}});QVERIFY(!db.media(path).contains("tmdbId"));QVERIFY(db.metadataJobs().isEmpty());
    }
    void absolutePromptPreservesProgressAndBothModes(){
        QTemporaryDir storage,media;file(media.path()+"/InuYasha.S01E028.mkv");file(media.path()+"/InuYasha.S01E100.mkv");
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.setSetting("tmdbKey","fixture");db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        const auto path=media.path()+"/InuYasha.S01E100.mkv";db.saveProgress(path,12000,100000);db.reload();
        db.setMetadataTransport([&](const QUrl &url,std::function<void(QVariant)> reply){QVariantMap data;
            if(url.path()=="/3/tv/9"){QVariantList seasons;for(int s=1;s<=6;++s)seasons<<QVariantMap{{"season_number",s},{"episode_count",s==4?29:s==5?26:s==6?31:27}};data={{"id",9},{"name","InuYasha"},{"overview","Serie"},{"seasons",seasons}};}
            else{int season=url.path().section('/',-1).toInt();data={{"season_number",season},{"episodes",QVariantList{QVariantMap{{"episode_number",season==4?19:1},{"name",season==4?"El episodio cien":"Otro"},{"overview","Sinopsis"}}}}};}
            QTimer::singleShot(0,&db,[reply,data]{reply(data);});
        });
        db.useMetadata(path,9,true);QTRY_VERIFY(!db.absolutePrompt().isEmpty());db.resolveAbsolute(0);
        QTRY_COMPARE(db.media(path).value("episodeTitle").toString(),QString("El episodio cien"));QCOMPARE(db.media(path).value("season").toInt(),1);QCOMPARE(db.media(path).value("episode").toInt(),100);
        db.setSetting("absoluteMode","RENUMBER");QCOMPARE(db.media(path).value("season").toInt(),4);QCOMPARE(db.media(path).value("episode").toInt(),19);QCOMPARE(db.media(path).value("position").toLongLong(),12000);
    }
};
QTEST_GUILESS_MAIN(MetadataTest)
#include "MetadataTest.moc"
