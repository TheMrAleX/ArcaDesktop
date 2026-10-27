#include "Library.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QFile>
class PerformanceTest:public QObject {
    Q_OBJECT
private slots:
    void cachedLibraryOfTenThousandFiles(){
        QTemporaryDir storage;{Library empty(storage.path());empty.setSetting("scanOnOpen",false);}
        const QString connection="performance-seed";
        {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(storage.path()+"/library.sqlite3");QVERIFY(db.open());QVERIFY(db.transaction());QSqlQuery root(db);QVERIFY(root.exec("INSERT INTO folders(id,path,kind) VALUES(1,'/benchmark/Series','SERIES')"));
            QSqlQuery insert(db);QVERIFY(insert.prepare("INSERT INTO files(path,folder,size,mtime,data,available) VALUES(?,1,1000,1,?,1)"));
            for(int n=0;n<10000;++n){const auto path=QString("/benchmark/Series/Show%1/S%2E%3.mkv").arg(n/100).arg((n%100)/20+1).arg(n%20+1);QVariantMap file{{"path",path},{"folder",1},{"name",QFileInfo(path).fileName()},{"title",QString("Show %1").arg(n/100)},{"kind","EPISODE"},{"season",(n%100)/20+1},{"episode",n%20+1},{"confidence",98},{"mtime",1},{"size",1000},{"duration",1200000}};insert.bindValue(0,path);insert.bindValue(1,QString::fromUtf8(QJsonDocument::fromVariant(file).toJson(QJsonDocument::Compact)));QVERIFY(insert.exec());}QVERIFY(db.commit());db.close();}QSqlDatabase::removeDatabase(connection);
        QElapsedTimer timer;timer.start();Library loaded(storage.path());const auto cold=timer.elapsed();QCOMPARE(loaded.files().size(),10000);QCOMPARE(loaded.shows().size(),100);for(const auto &v:loaded.shows())QCOMPARE(v.toMap().value("episodeCount").toInt(),100);
        timer.restart();loaded.reload();const auto reload=timer.elapsed();qInfo()<<"PERFORMANCE: 10000 archivos, 100 series; apertura"<<cold<<"ms; recarga"<<reload<<"ms";
        QFile status("/proc/self/status");if(status.open(QIODevice::ReadOnly))for(const auto &line:status.readAll().split('\n'))if(line.startsWith("VmHWM:"))qInfo().noquote()<<line;
    }
};
QTEST_GUILESS_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
