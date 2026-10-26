#include "NameParser.h"
#include "Library.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
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
};
QTEST_GUILESS_MAIN(CoreTest)
#include "CoreTest.moc"
