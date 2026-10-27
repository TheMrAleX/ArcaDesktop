#include "Library.h"
#include "BackupCodec.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
class BackupTest:public QObject {
    Q_OBJECT
    static QUrl write(const QString &path,const QVariantMap &data){QFile file(path);if(!file.open(QIODevice::WriteOnly))return {};file.write(QJsonDocument::fromVariant(data).toJson());return QUrl::fromLocalFile(path);}
private slots:
    void androidCopyBeforeFolderAndRestart(){
        QTemporaryDir storage,media;QDir().mkpath(media.path()+"/Aang/Season 2");const auto path=media.path()+"/Aang/Season 2/31.mkv";QFile video(path);QVERIFY(video.open(QIODevice::WriteOnly));video.write("fixture");video.close();
        const QString tree="content://com.android.externalstorage.documents/tree/primary%3ASeries";
        QVariantMap identity{{"folder",tree},{"relPath","Aang/Season 2/31.mkv"},{"name","31.mkv"},{"size",7}};
        auto item=identity;item.insert({{"positionMs",42000},{"durationMs",100000},{"watched",false},{"updatedAt",1000},{"subtitleDelayMs",350}});
        auto decision=identity;decision.insert({{"kind","EPISODE"},{"title","Avatar"},{"season",2},{"episode",1},{"year",2005},{"preferred",true}});
        QVariantMap backup{{"version",2},{"profile","Invitado"},{"folders",QVariantList{QVariantMap{{"treeUri",tree},{"displayPath","Interno/Series"},{"name","Series"},{"kind","SERIES"}}}},{"items",QVariantList{item}},{"decisions",QVariantList{decision}},{"rules",QVariantList{QVariantMap{{"folder",tree},{"pattern","dir:primary:Series/Aang/Season 2"},{"dirPath","Aang/Season 2"},{"interpretation","2|30|Avatar"}}}},{"settings",QVariantMap{{"languages",QStringList{"ja","es"}},{"subtitleSize","L"},{"skipSeconds",15}}},{"absoluteShows",QVariantMap{{"avatar",true}}}};
        const auto url=write(storage.path()+"/android.json",backup);
        {Library db(storage.path());db.setSetting("scanOnOpen",false);QVERIFY(db.importBackup(url));QCOMPARE(db.pendingFolders().size(),1);QCOMPARE(db.setting("languages").toString(),QString("ja,es"));}
        Library db(storage.path());QCOMPARE(db.pendingFolders().size(),1);db.bindImportedFolder(tree,QUrl::fromLocalFile(media.path()),"SERIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning()&&!db.files().isEmpty(),20000);QTRY_VERIFY_WITH_TIMEOUT(db.pendingFolders().isEmpty(),20000);
        QCOMPARE(db.profile(),QString("Principal"));QCOMPARE(db.profiles().size(),1);
        auto restored=db.media(path);QCOMPARE(restored.value("title").toString(),QString("Avatar"));QCOMPARE(restored.value("position").toLongLong(),42000);QCOMPARE(restored.value("episode").toInt(),1);QCOMPARE(restored.value("year").toInt(),2005);QCOMPARE(db.playbackSetting("subDelay",path).toDouble(),0.35);
        db.saveProgress(path,60000,100000);QVERIFY(db.importBackup(url));QCOMPARE(db.media(path).value("position").toLongLong(),60000);
    }
    void desktopRoundtripMovedRoot(){
        QTemporaryDir oldStore,newStore,oldMedia,newMedia;const auto name=QString("Film.2024.mkv");for(const auto &root:{oldMedia.path(),newMedia.path()}){QFile file(root+'/'+name);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");}
        const auto oldPath=oldMedia.path()+'/'+name,newPath=newMedia.path()+'/'+name;QUrl backup;
        {Library db(oldStore.path());db.setSetting("scanOnOpen",false);db.addFolder(QUrl::fromLocalFile(oldMedia.path()),"MOVIES");QTRY_VERIFY_WITH_TIMEOUT(!db.scanning(),20000);db.saveProgress(oldPath,25000,100000);db.correct(oldPath,"Película",0,0,false,2020);db.setFileHidden(oldPath,true);db.setPlaybackSetting("audioChoice",oldPath,"es||2");backup=QUrl::fromLocalFile(newStore.path()+"/desktop.json");QVERIFY(db.exportBackup(backup));}
        // Remove the original root so restoration needs an explicit new location.
        QVERIFY(QFile::remove(oldPath));QVERIFY(QDir().rmdir(oldMedia.path()));
        Library restored(newStore.path());restored.setSetting("scanOnOpen",false);QVERIFY(restored.importBackup(backup));QVERIFY(!restored.pendingFolders().isEmpty());restored.bindImportedFolder(QUrl::fromLocalFile(oldMedia.path()).toString(),QUrl::fromLocalFile(newMedia.path()),"MOVIES");QTRY_VERIFY_WITH_TIMEOUT(!restored.scanning()&&!restored.files().isEmpty(),20000);QTRY_VERIFY_WITH_TIMEOUT(restored.pendingFolders().isEmpty(),20000);
        const auto file=restored.media(newPath);QCOMPARE(file.value("title").toString(),QString("Película"));QCOMPARE(file.value("position").toInt(),25000);QVERIFY(file.value("hidden").toBool());QCOMPARE(restored.playbackSetting("audioChoice",newPath).toString(),QString("es||2"));
    }
    void badRulesCannotAddFolders(){
        QTemporaryDir store,media;Library db(store.path());auto url=write(store.path()+"/invalid.json",{{"version",2},{"items",QVariantList()},{"folders",QVariantList{QVariantMap{{"treeUri",QUrl::fromLocalFile(media.path()).toString()},{"kind","SERIES"}}}},{"rules",QVariantList{QVariantMap{{"pattern","dir:test"},{"dirPath","../../elsewhere"},{"interpretation","1|0|Show"}}}}});QVERIFY(!db.importBackup(url));QVERIFY(db.folders().isEmpty());QCOMPARE(db.profiles().size(),1);
        QVERIFY(!BackupCodec::read(R"({"version":2,"items":{}})").error.isEmpty());
    }
    void ambiguousMatchesStayPending(){
        QVariantList files{QVariantMap{{"path","/a/S01E01.mkv"},{"name","S01E01.mkv"},{"size",10}},QVariantMap{{"path","/b/S01E01.mkv"},{"name","S01E01.mkv"},{"size",10}}};QVERIFY(BackupCodec::match({{"name","S01E01.mkv"},{"size",10}},files,{},{},{}).isEmpty());
    }
    void profileEditingDeletesOnlySelectedProgress(){
        QTemporaryDir store;Library db(store.path());db.createProfile("Segundo");const int id=db.profiles().last().toMap().value("id").toInt();db.editProfile(id,"Cine","#FF9E7A");QCOMPARE(db.profiles().last().toMap().value("color").toString(),QString("#FF9E7A"));db.selectProfile(id);db.setPlaybackSetting("audioChoice","file","es||2");db.deleteProfile(id);QCOMPARE(db.profile(),QString("Principal"));QCOMPARE(db.profiles().size(),1);db.deleteProfile(1);QCOMPARE(db.profiles().size(),1);QVERIFY(db.playbackSetting("audioChoice","file").toString().isEmpty());
    }
};
QTEST_GUILESS_MAIN(BackupTest)
#include "BackupTest.moc"
