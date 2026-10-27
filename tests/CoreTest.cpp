#include "NameParser.h"
#include "Library.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class CoreTest : public QObject {
    Q_OBJECT
private slots:
    void names_data(){
        QTest::addColumn<QString>("name");QTest::addColumn<QStringList>("dirs");QTest::addColumn<QString>("kind");QTest::addColumn<QString>("title");QTest::addColumn<int>("season");QTest::addColumn<int>("episode");QTest::addColumn<int>("year");
        QTest::newRow("sxe")<<"Astillas.de.Cristal.S02E04.1080p.mkv"<<QStringList{}<<"SERIES"<<"Astillas de Cristal"<<2<<4<<0;
        QTest::newRow("spaces")<<"Serie S1 E2.mp4"<<QStringList{}<<"SERIES"<<"Serie"<<1<<2<<0;
        QTest::newRow("nxn")<<"Puerto Norte - 2x07 - final.mkv"<<QStringList{}<<"SERIES"<<"Puerto Norte"<<2<<7<<0;
        QTest::newRow("spanish")<<"Serie - Temporada 3 - Capítulo 10.mp4"<<QStringList{}<<"SERIES"<<"Serie"<<3<<10<<0;
        QTest::newRow("folder")<<"07.mkv"<<QStringList{"Puerto Norte (2023)","Season 02"}<<"SERIES"<<"Puerto Norte"<<2<<7<<2023;
        QTest::newRow("root")<<"2x05.mkv"<<QStringList{"2"}<<"SERIES"<<"La Leyenda de Aang"<<2<<5<<0;
        QTest::newRow("movie")<<"El.Ultimo.Faro.2024.2160p.mkv"<<QStringList{}<<"MOVIES"<<"El Ultimo Faro"<<-1<<0<<2024;
        QTest::newRow("numeric-title")<<"1917.2019.1080p.BluRay.x264.mkv"<<QStringList{}<<"MOVIES"<<"1917"<<-1<<0<<2019;
        QTest::newRow("numeric-title-2")<<"2001.A.Space.Odyssey.1968.mkv"<<QStringList{}<<"MOVIES"<<"2001 A Space Odyssey"<<-1<<0<<1968;
        QTest::newRow("generic")<<"movie.mkv"<<QStringList{"Noche de Neón (2023)"}<<"MOVIES"<<"Noche de Neón"<<-1<<0<<2023;
        QTest::newRow("cap")<<"La Serie [HDTV 720p][Cap.102][AC3 5.1 Castellano].mkv"<<QStringList{}<<"SERIES"<<"La Serie"<<1<<2<<0;
    }
    void names(){QFETCH(QString,name);QFETCH(QStringList,dirs);QFETCH(QString,kind);QFETCH(QString,title);QFETCH(int,season);QFETCH(int,episode);QFETCH(int,year);auto p=Names::parse(name,dirs,kind,"La Leyenda de Aang");QCOMPARE(p.title,title);QCOMPARE(p.season,season);QCOMPARE(p.episode,episode);QCOMPARE(p.year,year);}
    void ambiguity(){auto p=Names::parse("La.Serie.X.S101.mkv",{},"SERIES");QVERIFY(p.confidence<80);QCOMPARE(p.altSeason,10);QCOMPARE(p.altEpisode,1);auto r=Names::parse("La.Serie.X.S101.mkv",{},"SERIES",{},"SSE");QCOMPARE(r.season,10);QCOMPARE(r.confidence,100);QCOMPARE(Names::resolveCompactGroup({101,102,103}),QString("SEE"));QVERIFY(Names::resolveCompactGroup({101}).isEmpty());}
    void keysAndNumbers(){QCOMPARE(Names::key("Cartografía del Silencio!"),QString("cartografia del silencio"));QCOMPARE(Names::seasonFromDir("Specials"),0);QCOMPARE(Names::seasonFromDir("1917"),-1);QCOMPARE(Names::plainNumber("InuYasha - 100 Latino.mp4"),100);QCOMPARE(Names::plainNumber("Serie S01E02.mkv"),0);QVERIFY(Names::plainNumbering({70,71,72,73,74}));QVERIFY(!Names::plainNumbering({1,50,200}));QCOMPARE(Names::parse("sample-test.mp4").kind,Names::Kind::Ignored);}
    void scanAndProgress(){
        QTemporaryDir storage,media;QVERIFY(storage.isValid());
        QFile file(media.path()+"/Arca.Test.2024.mkv");QVERIFY(file.open(QIODevice::WriteOnly));file.write("invalid video fixture");file.close();
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"MOVIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QCOMPARE(db.files().size(),1);
        auto path=db.files().first().toMap()["path"].toString();
        db.saveProgress(path,42000,100000);db.reload();QCOMPARE(db.media(path)["position"].toLongLong(),42000);
        db.createProfile("Segundo");auto second=db.profiles().last().toMap()["id"].toInt();db.selectProfile(second);QCOMPARE(db.media(path)["position"].toLongLong(),0);
        db.selectProfile(1);db.saveProgress(path,95000,100000);db.reload();QVERIFY(db.media(path)["watched"].toBool());
        db.correct(path,"Título corregido",0,0,false);db.scan();QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QCOMPARE(db.media(path)["title"].toString(),QString("Título corregido"));
        auto backup=QUrl::fromLocalFile(storage.path()+"/backup.json");QVERIFY(db.exportBackup(backup));db.markWatched(path,false);QVERIFY(db.importBackup(backup));QVERIFY(!db.media(path)["watched"].toBool());
        QVERIFY(QFile::remove(path));db.scan();QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QVERIFY(!db.media(path)["available"].toBool());QVERIFY(QFileInfo::exists(media.path()));
    }
    void backupRejectsInvalid(){QTemporaryDir dir;Library db(dir.path());QFile file(dir.path()+"/bad.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write("{\"format\":\"other\",\"version\":1}");file.close();QVERIFY(!db.importBackup(QUrl::fromLocalFile(file.fileName())));QCOMPARE(db.profiles().size(),1);}
    void subtitleTiming(){
        QTemporaryDir dir;Library db(dir.path());
        QFile srt(dir.path()+"/Film.es.srt");QVERIFY(srt.open(QIODevice::WriteOnly));srt.write("1\r\n00:01:02,345 --> 00:01:04,000\r\nHola\r\nmundo\r\n\r\n2\r\n00:01:05,000 --> 00:01:06,000\r\nAdiós\r\n");srt.close();
        auto cues=db.readSubtitles(QUrl::fromLocalFile(srt.fileName()));QCOMPARE(cues.size(),2);QCOMPARE(cues[0].toMap()["start"].toLongLong(),62345);QCOMPARE(cues[0].toMap()["text"].toString(),QString("Hola\nmundo"));
        QFile ass(dir.path()+"/Film.ass");QVERIFY(ass.open(QIODevice::WriteOnly));ass.write("[Events]\nDialogue: 0,0:00:01.20,0:00:04.30,Default,,0,0,0,,{\\i1}Hola\\NAmigo, sí\n");ass.close();
        auto styled=db.readSubtitles(QUrl::fromLocalFile(ass.fileName()));QCOMPARE(styled.size(),1);QCOMPARE(styled[0].toMap()["start"].toLongLong(),1200);QCOMPARE(styled[0].toMap()["text"].toString(),QString("Hola\nAmigo, sí"));
        QCOMPARE(db.externalSubtitles(dir.path()+"/Film.mkv").size(),2);
    }
    void settingsSurviveRestart(){QTemporaryDir dir;{Library db(dir.path());db.setSetting("scanOnOpen",false);db.setSetting("resume",false);db.setSetting("resumeRewindSeconds",7);}Library restored(dir.path());QVERIFY(!restored.setting("resume",true).toBool());QCOMPARE(restored.setting("resumeRewindSeconds",3).toInt(),7);}
    void backupRollsBackAllRows(){QTemporaryDir dir;Library db(dir.path());QFile file(dir.path()+"/bad.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(R"({"format":"arca-desktop","version":1,"profiles":[{"id":8,"name":"No debe quedar"}],"progress":[{"profile":999,"path":"x","position":0,"duration":100}]})");file.close();QVERIFY(!db.importBackup(QUrl::fromLocalFile(file.fileName())));QCOMPARE(db.profiles().size(),1);}
    void profileVisibilityAndSpoilersPersist() {
        QTemporaryDir storage,media;
        QFile file(media.path()+"/Serie.S01E01.El.secreto.mkv");QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");file.close();
        int folder=0;
        {
            Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
            folder=db.folders().first().toMap().value("id").toInt();const auto path=db.files().first().toMap().value("path").toString();
            db.setSetting("spoilerFree",true);QVERIFY(db.media(path).value("spoilerHidden").toBool());QVERIFY(db.media(path).value("episodeTitle").toString().isEmpty());
            db.saveProgress(path,10000,100000);db.reload();QVERIFY(!db.media(path).value("spoilerHidden").toBool());QCOMPARE(db.continuing().size(),1);
            db.hideContinue(path,true);QVERIFY(db.continuing().isEmpty());
            db.setShowSpoilers("serie",0);QVERIFY(!db.media(path).value("spoilerEnabled").toBool());db.setShowSpoilers("serie",-1);QVERIFY(db.media(path).value("spoilerEnabled").toBool());
            db.setFolderVisible(folder,false);QVERIFY(db.files().isEmpty());QVERIFY(db.shows().isEmpty());QVERIFY(!db.folders().first().toMap().value("visible").toBool());
            db.createProfile("Segundo");db.selectProfile(db.profiles().last().toMap().value("id").toInt());QCOMPARE(db.files().size(),1);QVERIFY(db.continuing().isEmpty());db.selectProfile(1);QVERIFY(db.files().isEmpty());
        }
        Library restored(storage.path());QVERIFY(restored.files().isEmpty());restored.showAllFolders();QCOMPARE(restored.files().size(),1);QVERIFY(restored.continuing().isEmpty());
    }
    void absoluteNumberingMatchesAndroid() {
        QList<Names::EpisodeRef> episodes;
        for(int n=1;n<=30;++n)episodes.append({"aang",1,n,true});
        for(int n=31;n<=60;++n)episodes.append({"aang",2,n,true});
        for(int n=61;n<=81;++n)episodes.append({"aang",3,n,true});
        auto offsets=Names::absoluteOffsets(episodes);
        QCOMPARE(offsets.value({"aang",2}).offset,30);QVERIFY(offsets.value({"aang",2}).sure);
        QCOMPARE(offsets.value({"aang",3}).offset,60);QVERIFY(offsets.value({"aang",3}).sure);
        QVERIFY(!offsets.contains({"aang",1}));
        episodes.clear();for(int n=31;n<=60;++n)episodes.append({"alone",2,n,true});
        offsets=Names::absoluteOffsets(episodes);QCOMPARE(offsets.value({"alone",2}).offset,30);QVERIFY(!offsets.value({"alone",2}).sure);
        episodes.clear();for(int n=5;n<=10;++n)episodes.append({"explicit",2,n,false});QVERIFY(Names::absoluteOffsets(episodes).isEmpty());
    }
    void playbackChoicesArePerProfileAndInheritedBySeries() {
        QTemporaryDir storage,media;
        for(int n=1;n<=2;++n){QFile file(media.path()+QString("/Serie.S01E0%1.mkv").arg(n));QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");}
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        QCOMPARE(db.files().size(),2);const auto first=db.files()[0].toMap().value("path").toString(),second=db.files()[1].toMap().value("path").toString();
        QCOMPARE(db.nextEpisode(first).value("path").toString(),second);QVERIFY(db.nextEpisode(second).isEmpty());
        db.setPlaybackSetting("audioChoice",first,"es||3");db.setPlaybackSetting("subDelay",first,0.4);
        QCOMPARE(db.playbackSetting("audioChoice",second).toString(),QString("es||3"));
        db.createProfile("Segundo");db.selectProfile(db.profiles().last().toMap().value("id").toInt());
        QVERIFY(db.playbackSetting("audioChoice",first).toString().isEmpty());QCOMPARE(db.playbackSetting("subDelay",first,0).toInt(),0);
        db.setPlaybackSetting("audioChoice",first,"en||7");db.selectProfile(1);
        QCOMPARE(db.playbackSetting("audioChoice",first).toString(),QString("es||3"));QCOMPARE(db.playbackSetting("subDelay",first).toDouble(),0.4);
    }
    void scanRenumbersAndManualRuleWins() {
        QTemporaryDir storage,media;
        const auto first=media.path()+"/Aang/Temporada 1",second=media.path()+"/Aang/Temporada 2";
        QDir().mkpath(first);QDir().mkpath(second);
        for(const auto &path:QStringList{first+"/30.mkv",second+"/31.mkv",second+"/32.mkv",second+"/33.mkv"}){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");}
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        auto episode=db.media(second+"/31.mkv");QCOMPARE(episode.value("episode").toInt(),1);QCOMPARE(episode.value("rawEpisode").toInt(),31);QVERIFY(episode.value("confidence").toInt()>=88);
        QVERIFY(QFile::remove(first+"/30.mkv"));db.scan();QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        episode=db.media(second+"/31.mkv");QCOMPARE(episode.value("episode").toInt(),1);QCOMPARE(episode.value("confidence").toInt(),65);
        db.correct(second+"/31.mkv","Aang",5,9,true);QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        episode=db.media(second+"/32.mkv");QCOMPARE(episode.value("season").toInt(),5);QCOMPARE(episode.value("episode").toInt(),10);QCOMPARE(episode.value("confidence").toInt(),100);
    }
    void animePlainNumberingMatchesAndroid(){
        QList<QPair<QString,Names::Result>> files;
        for(int n=70;n<=138;++n){auto name=n>=98&&n<=111?QString("InuYasha - %1 Latino.mp4").arg(n):QString("%1.mp4").arg(n);files.append({name,Names::parse(name,{"InuYasha"},"SERIES","Series")});}
        const auto fixed=Names::plainNumberingFixes(files,"InuYasha","Series");QCOMPARE(fixed.size(),files.size());for(auto it=fixed.cbegin();it!=fixed.cend();++it){QCOMPARE(it.value().episode,Names::plainNumber(files[it.key()].first));QCOMPARE(it.value().title,QString("InuYasha"));QCOMPARE(it.value().season,1);QVERIFY(it.value().confidence>=80);QVERIFY(it.value().episodeTitle.isEmpty());}
        files.clear();for(int n=1;n<=12;++n){auto name=QString("%1.mkv").arg(n,2,10,QChar('0'));files.append({name,Names::parse(name,{"Serie","Temporada 2"},"SERIES","Series")});}QVERIFY(Names::plainNumberingFixes(files,"Temporada 2","Series").isEmpty());
    }
    void duplicatesHiddenSubtitlesAndCompactRules(){
        QTemporaryDir storage,media;for(const auto &name:{"Film.2024.720p.mkv","Film.2024.1080p.mkv","Serie.S101.mkv","Serie.es.srt"}){QFile file(media.path()+'/'+name);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");}
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"AUTO");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QCOMPARE(db.files().size(),4);const auto first=media.path()+"/Film.2024.720p.mkv",second=media.path()+"/Film.2024.1080p.mkv";QVERIFY(db.media(first).value("hasDuplicates").toBool());db.preferFile(first);QVERIFY(!db.media(first).value("duplicate").toBool());QVERIFY(db.media(second).value("duplicate").toBool());db.setFileHidden(first,true);QVERIFY(!db.media(second).value("duplicate").toBool());db.resetDecision(first);QVERIFY(!db.media(first).value("hidden").toBool());
        const auto series=media.path()+"/Serie.S101.mkv";db.setCompactRule(db.folders().first().toMap().value("id").toInt(),"SSE");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);QCOMPARE(db.media(series).value("season").toInt(),10);QCOMPARE(db.media(series).value("episode").toInt(),1);QCOMPARE(db.media(media.path()+"/Serie.es.srt").value("kind").toString(),QString("SUBTITLE"));
    }
    void episodeNfoKeepsShowIdentity() {
        QTemporaryDir storage,media;
        QFile file(media.path()+"/Serie.S01E01.mkv");QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");file.close();
        QFile nfo(media.path()+"/Serie.S01E01.nfo");QVERIFY(nfo.open(QIODevice::WriteOnly));nfo.write("<episodedetails><title>El secreto</title><plot>Sinopsis del episodio</plot><genre>Drama</genre><genre>Misterio</genre></episodedetails>");nfo.close();
        Library db(storage.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);
        auto episode=db.media(file.fileName());QCOMPARE(episode.value("title").toString(),QString("Serie"));QCOMPARE(episode.value("episodeTitle").toString(),QString("El secreto"));QCOMPARE(episode.value("genres").toString(),QString("Drama · Misterio"));
        db.setSetting("spoilerFree",true);QVERIFY(db.media(file.fileName()).value("episodeOverview").toString().isEmpty());
        db.accept(file.fileName());db.setSetting("spoilerFree",false);QCOMPARE(db.media(file.fileName()).value("episodeTitle").toString(),QString("El secreto"));
    }
};
QTEST_GUILESS_MAIN(CoreTest)
#include "CoreTest.moc"
