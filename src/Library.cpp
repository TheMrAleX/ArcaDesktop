#include "Library.h"
#include "NameParser.h"
#include "Catalog.h"
#include "TrackChoice.h"
#include "Metadata.h"
#include <QtConcurrent>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QProcess>
#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <QUrl>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <algorithm>

struct Library::ScanState {
    std::atomic_bool cancel{false}, pause{false};
    QMutex mutex;
    QWaitCondition condition;
};
static QString json(const QVariantMap &v) { return QString::fromUtf8(QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact)); }
static QVariantMap unjson(const QString &v) { return QJsonDocument::fromJson(v.toUtf8()).toVariant().toMap(); }
static qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }

Library::Library(const QString &dataDir, QObject *parent) : QObject(parent) {
    m_dataDir=dataDir.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppDataLocation):dataDir;
    QDir().mkpath(m_dataDir);
    m_settings=new QSettings(m_dataDir+"/settings.ini",QSettings::IniFormat,this);
    m_connection=QString("arca-%1").arg(quintptr(this));
    m_db=QSqlDatabase::addDatabase("QSQLITE",m_connection);
    m_db.setDatabaseName(m_dataDir+"/library.sqlite3");
    if(!m_db.open()){fail("No puedo abrir la biblioteca: "+m_db.lastError().text());return;}
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA journal_mode=WAL");
    exec("CREATE TABLE IF NOT EXISTS folders(id INTEGER PRIMARY KEY,path TEXT UNIQUE NOT NULL,kind TEXT NOT NULL,lastScan INTEGER DEFAULT 0)");
    exec("CREATE TABLE IF NOT EXISTS files(path TEXT PRIMARY KEY,folder INTEGER REFERENCES folders(id) ON DELETE CASCADE,size INTEGER,mtime INTEGER,data TEXT NOT NULL,corrected INTEGER DEFAULT 0,available INTEGER DEFAULT 1)");
    exec("CREATE TABLE IF NOT EXISTS profiles(id INTEGER PRIMARY KEY,name TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS progress(profile INTEGER REFERENCES profiles(id),path TEXT,position INTEGER,duration INTEGER,watched INTEGER,updated INTEGER,PRIMARY KEY(profile,path))");
    exec("CREATE TABLE IF NOT EXISTS rules(path TEXT PRIMARY KEY,title TEXT,season INTEGER,offset INTEGER)");
    exec("CREATE TABLE IF NOT EXISTS metadata(path TEXT PRIMARY KEY,data TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS file_decisions(path TEXT PRIMARY KEY,data TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS pending_backups(id TEXT PRIMARY KEY,data TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS folder_aliases(source TEXT PRIMARY KEY,path TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS compact_rules(folder INTEGER PRIMARY KEY,reading TEXT NOT NULL)");
    for(const auto &value:query("SELECT path,data FROM files WHERE corrected=1")){auto row=value.toMap(),file=unjson(row.value("data").toString());QVariantMap decision;for(const auto &key:{"kind","title","year","season","episode","confidence","reason","hidden","preferred"})if(file.contains(key))decision[key]=file.value(key);exec("INSERT OR IGNORE INTO file_decisions VALUES(?,?)",{row.value("path"),json(decision)});}
    exec("UPDATE files SET corrected=0 WHERE corrected=1");
    exec("CREATE TABLE IF NOT EXISTS show_meta(key TEXT PRIMARY KEY,data TEXT NOT NULL)");
    exec("CREATE TABLE IF NOT EXISTS episode_meta(showkey TEXT,season INTEGER,episode INTEGER,data TEXT NOT NULL,PRIMARY KEY(showkey,season,episode))");
    for(const auto &value:query("SELECT f.data AS file,m.data AS meta FROM metadata m JOIN files f ON f.path=m.path")){auto row=value.toMap();auto file=unjson(row.value("file").toString());if(file.value("kind")=="EPISODE")exec("INSERT OR IGNORE INTO show_meta VALUES(?,?)",{Names::key(file.value("title").toString()),row.value("meta")});}
    m_artTimer.setSingleShot(true);m_artTimer.setInterval(40);connect(&m_artTimer,&QTimer::timeout,this,&Library::flushArtwork);
    exec("INSERT OR IGNORE INTO profiles VALUES(1,'Principal')");
    m_debounce.setSingleShot(true);m_debounce.setInterval(1500);
    connect(&m_watcher,&QFileSystemWatcher::directoryChanged,&m_debounce,qOverload<>(&QTimer::start));
    connect(&m_debounce,&QTimer::timeout,this,&Library::scan);
    connect(&m_scan,&QFutureWatcher<QVariantMap>::finished,this,[this]{
        const auto result=m_scan.result();
        if(!result.value("cancelled").toBool()) {
            if(m_db.transaction()) {
                bool ok=true;
                for(const auto &folder:result.value("complete").toList()) ok=exec("UPDATE files SET available=0 WHERE folder=?",{folder})&&ok;
                for(const auto &item:result.value("files").toList()) {
                    const auto f=item.toMap();
                    ok=exec("INSERT INTO files(path,folder,size,mtime,data,available) VALUES(?,?,?,?,?,1) ON CONFLICT(path) DO UPDATE SET folder=excluded.folder,size=excluded.size,mtime=excluded.mtime,data=excluded.data,available=1",
                         {f["path"],f["folder"],f["size"],f["mtime"],json(f)})&&ok;
                }
                for(const auto &folder:result.value("complete").toList()) ok=exec("UPDATE folders SET lastScan=? WHERE id=?",{now(),folder})&&ok;
                if(ok) {if(!m_db.commit())fail(m_db.lastError().text());} else m_db.rollback();
            }
            const auto errors=result.value("errors").toStringList();
            if(!errors.isEmpty()) fail(errors.join('\n'));
        }
        if(!m_watcher.directories().isEmpty())m_watcher.removePaths(m_watcher.directories());
        const auto watched=result.value("watched").toStringList();
        if(!watched.isEmpty())m_watcher.addPaths(watched);
        m_scanning=false;m_paused=false;
        m_scanStatus=QString("Exploración terminada · %1 archivos").arg(result.value("files").toList().size());
        applyPendingBackups();emit scanningChanged();emit scanStatusChanged();reload();
        if(m_rescanRequested){m_rescanRequested=false;QTimer::singleShot(0,this,&Library::scan);}
    });
    reload();
    refreshPendingFolders();
    if(setting("scanOnOpen",true).toBool())QTimer::singleShot(100,this,[this]{if(setting("scanOnOpen",true).toBool())scan();});
}
Library::~Library(){
    if(m_scanState){m_scanState->cancel=true;m_scanState->pause=false;m_scanState->condition.wakeAll();}
    m_scan.waitForFinished();m_db.close();m_db=QSqlDatabase();QSqlDatabase::removeDatabase(m_connection);
}
bool Library::exec(const QString &sql,const QVariantList &args){
    QSqlQuery q(m_db);q.prepare(sql);for(const auto &v:args)q.addBindValue(v);
    if(!q.exec()){fail("No puedo guardar la biblioteca: "+q.lastError().text());return false;}return true;
}
QVariantList Library::query(const QString &sql,const QVariantList &args)const{
    QVariantList out;QSqlQuery q(m_db);q.prepare(sql);for(const auto &v:args)q.addBindValue(v);
    if(!q.exec())return out;
    while(q.next()){QVariantMap row;for(int i=0;i<q.record().count();++i)row[q.record().fieldName(i)]=q.value(i);out<<row;}return out;
}
void Library::fail(const QString &message){m_error=message;emit errorChanged();}
void Library::clearError(){m_error.clear();emit errorChanged();}
QVariant Library::setting(const QString &key,const QVariant &fallback)const{auto value=m_settings->value(key,fallback);if(fallback.isValid())value.convert(fallback.metaType());return value;}
void Library::setSetting(const QString &key,const QVariant &value){m_settings->setValue(key,value);if(key=="absoluteMode")m_settings->remove("absoluteShows");m_settings->sync();if(key=="spoilerFree"||key=="absoluteMode"||key.startsWith("spoilerShows/"))reload();else emit changed();if(key=="ignoreSamples")scan();}
bool Library::onboarded()const{return setting("onboarded",false).toBool();}
void Library::finishWelcome(){setSetting("onboarded",true);}
QString Library::profile()const{const auto id=setting("profileId",1).toInt();for(const auto &p:m_profiles)if(p.toMap()["id"].toInt()==id)return p.toMap()["name"].toString();return "Principal";}
void Library::reload(){
    m_folders=query("SELECT * FROM folders ORDER BY path");m_profiles=query("SELECT * FROM profiles ORDER BY id");
    for(auto &value:m_profiles){auto p=value.toMap();const int id=p.value("id").toInt();p["color"]=setting(QString("profileColor/%1").arg(id),QString("#8B9CFF"));auto counts=query("SELECT SUM(watched) AS watched,SUM(CASE WHEN watched=0 AND position>5000 THEN 1 ELSE 0 END) AS inProgress FROM progress WHERE profile=?",{id});if(!counts.isEmpty()){p["watched"]=counts.first().toMap().value("watched").toInt();p["inProgress"]=counts.first().toMap().value("inProgress").toInt();}value=p;}
    const auto profileId=setting("profileId",1).toInt();
    const auto visibilityKey=QString("visibleFolders/%1").arg(profileId);
    const bool restricted=m_settings->contains(visibilityKey);
    const auto visible=m_settings->value(visibilityKey).toStringList();
    for(auto &value:m_folders){auto folder=value.toMap();folder["visible"]=!restricted||visible.contains(folder["id"].toString());value=folder;}
    m_files.clear();
    const auto rows=query("SELECT f.path,f.data,f.available,COALESCE(p.position,0) AS position,COALESCE(p.duration,0) AS progressDuration,COALESCE(p.watched,0) AS watched,COALESCE(p.updated,0) AS updated FROM files f LEFT JOIN progress p ON p.path=f.path AND p.profile=? ORDER BY json_extract(f.data,'$.title') COLLATE NOCASE,json_extract(f.data,'$.season'),json_extract(f.data,'$.episode'),f.path",{setting("profileId",1)});
    QSet<QString> episodes;
    QHash<QString,QVariantMap> metadata;
    QHash<QString,QVariantMap> decisions;
    for(const auto &value:query("SELECT * FROM file_decisions")){auto row=value.toMap();decisions[row.value("path").toString()]=unjson(row.value("data").toString());}
    for(const auto &v:query("SELECT * FROM metadata")){auto row=v.toMap();metadata[row["path"].toString()]=unjson(row["data"].toString());}
    QHash<QString,QVariantMap> showMeta,episodeMeta;
    for(const auto &value:query("SELECT * FROM show_meta")){auto row=value.toMap();showMeta[row.value("key").toString()]=unjson(row.value("data").toString());}
    for(const auto &value:query("SELECT * FROM episode_meta")){auto row=value.toMap();episodeMeta[row.value("showkey").toString()+'|'+row.value("season").toString()+'|'+row.value("episode").toString()]=unjson(row.value("data").toString());}
    QHash<QString,QVariantList> byShow;
    for(const auto &value:rows){auto row=value.toMap(),file=unjson(row.value("data").toString());const auto manual=decisions.value(row.value("path").toString());for(auto it=manual.cbegin();it!=manual.cend();++it)if(!it.value().isNull())file[it.key()]=it.value();if(file.value("kind")=="EPISODE")byShow[Names::key(file.value("title").toString())]<<file;}
    for(const auto &v:rows){auto row=v.toMap();auto f=unjson(row.take("data").toString());for(auto it=row.begin();it!=row.end();++it)f[it.key()]=it.value();
        if(restricted&&!visible.contains(f["folder"].toString()))continue;
        const auto manual=decisions.value(f.value("path").toString());for(auto it=manual.cbegin();it!=manual.cend();++it)if(!it.value().isNull())f[it.key()]=it.value();f["manual"]=!manual.isEmpty();
        f["fileTitle"]=f["title"];f["fileYear"]=f["year"];
        const auto sourceKey=Names::key(f.value("fileTitle").toString());
        if(f.value("kind")=="EPISODE"){
            const auto info=showMeta.value(sourceKey);for(auto it=info.cbegin();it!=info.cend();++it)if(it.key()!="details")f[it.key()]=it.value();
            const auto ranges=Metadata::ranges(info.value("seasonCounts").toList());
            const bool absolute=Metadata::looksAbsolute(byShow.value(sourceKey),ranges);
            auto choice=setting("absoluteShows/"+sourceKey);
            if(!choice.isValid()){const auto mode=setting("absoluteMode",QString("ASK")).toString();if(mode!="ASK")choice=mode=="RENUMBER";}
            int season=f.value("season").toInt(),number=f.value("episode").toInt();
            if(absolute){const auto mapped=Metadata::absoluteEpisode(number,ranges);if(!mapped.isEmpty()){season=mapped.value("season").toInt();number=mapped.value("episode").toInt();if(choice.isValid()&&choice.toBool()){f["sourceSeason"]=f["season"];f["sourceEpisode"]=f["episode"];f["season"]=season;f["episode"]=number;}}}
            const auto episode=episodeMeta.value(sourceKey+'|'+QString::number(season)+'|'+QString::number(number));for(auto it=episode.cbegin();it!=episode.cend();++it)f[it.key()]=it.value();
        }else{const auto info=metadata.value(f.value("path").toString());for(auto it=info.cbegin();it!=info.cend();++it)f[it.key()]=it.value();}
        if(f.value("fileYear").toInt()>0)f["year"]=f.value("fileYear");
        f["showKey"]=Catalog::showKey(f);f["continueKey"]=Catalog::continueKey(f);
        f["thumb"]="image://thumbs/"+QString::fromLatin1(QCryptographicHash::hash((f.value("path").toString()+'|'+f.value("mtime").toString()).toUtf8(),QCryptographicHash::Sha256).toHex());
        const bool spoilers=setting("spoilerShows/"+f["showKey"].toString(),setting("spoilerFree",false)).toBool();
        f["spoilerEnabled"]=spoilers;
        f["spoilerHidden"]=f["kind"]=="EPISODE"&&spoilers&&!f["watched"].toBool()&&f["position"].toLongLong()<=5000;
        if(f["spoilerHidden"].toBool()){f["episodeTitle"]="";f["episodeOverview"]="";f["still"]="";f["thumb"]="";}
        const auto id=Names::key(f["title"].toString())+"|"+f["season"].toString()+"|"+f["episode"].toString();
        if(f["kind"]=="EPISODE"){f["duplicate"]=episodes.contains(id);episodes.insert(id);}
        m_files<<f;
    }
    QVariantMap hidden;
    const auto prefix=QString("continueHidden/%1/").arg(profileId);
    for(const auto &key:m_settings->allKeys())if(key.startsWith(prefix))hidden[key.mid(prefix.size())]=m_settings->value(key);
    m_files=Catalog::markDuplicates(m_files);const auto catalog=Catalog::build(m_files,hidden);m_shows=catalog.shows;m_continuing=catalog.continuing;
    emit filesPrepared();emit changed();
}
void Library::addFolder(const QUrl &url,const QString &kind){
    const QFileInfo dir(url.toLocalFile());
    if(!dir.isDir()||!dir.isReadable()){fail("No puedo leer esa carpeta. Comprueba los permisos o monta el disco.");return;}
    if(!QStringList{"MOVIES","SERIES","AUTO","MIXED"}.contains(kind)){fail("Tipo de carpeta desconocido");return;}
    const auto path=dir.canonicalFilePath();
    for(const auto &v:m_folders){const auto existing=v.toMap()["path"].toString();if(path!=existing&&(path.startsWith(existing+'/')||existing.startsWith(path+'/'))){fail("Esa carpeta se solapa con otra ya añadida.");return;}}
    if(exec("INSERT INTO folders(path,kind) VALUES(?,?) ON CONFLICT(path) DO UPDATE SET kind=excluded.kind",{path,kind})){finishWelcome();reload();scan();}
}
void Library::removeFolder(int id){if(m_scanning){fail("Espera a que termine la exploración antes de quitar una carpeta.");return;}exec("DELETE FROM folders WHERE id=?",{id});reload();}

static QVariantMap probe(const QString &path){
    QProcess process;process.start("ffprobe",{"-v","quiet","-show_format","-show_streams","-of","json",path});
    if(!process.waitForStarted(2000))return {};
    if(!process.waitForFinished(10000)){process.kill();process.waitForFinished();return {};}
    return QJsonDocument::fromJson(process.readAllStandardOutput()).toVariant().toMap();
}
static void localMetadata(QVariantMap &f,const QFileInfo &file){
    QDir dir=file.dir();
    for(const auto &name:{file.completeBaseName()+"-poster.jpg",file.completeBaseName()+".jpg",QString("poster.jpg"),QString("folder.jpg"),QString("cover.jpg"),QString("poster.png")})
        if(QFileInfo::exists(dir.filePath(name))){f["poster"]=QUrl::fromLocalFile(dir.filePath(name)).toString();break;}
    for(const auto &name:{QString("fanart.jpg"),QString("backdrop.jpg")})if(QFileInfo::exists(dir.filePath(name))){f["backdrop"]=QUrl::fromLocalFile(dir.filePath(name)).toString();break;}
    QString nfo=dir.filePath(file.completeBaseName()+".nfo");if(!QFileInfo::exists(nfo))nfo=dir.filePath(f["kind"]=="EPISODE"?"tvshow.nfo":"movie.nfo");
    QFile input(nfo);
    if(input.size()>2*1024*1024||!input.open(QIODevice::ReadOnly))return;
    QXmlStreamReader xml(&input);
    QString documentType;
    QStringList genres;
    while(!xml.atEnd()){
        xml.readNext();if(!xml.isStartElement())continue;
        auto tag=xml.name().toString();
        if(documentType.isEmpty())documentType=tag;
        if(tag=="title"||tag=="plot"||tag=="genre"||tag=="year") {
            auto text=xml.readElementText(QXmlStreamReader::IncludeChildElements);if(text.isEmpty())continue;
            if(tag=="genre")genres<<text;
            else if(documentType=="episodedetails"&&tag=="title")f["episodeTitle"]=text;
            else if(documentType=="episodedetails"&&tag=="plot")f["episodeOverview"]=text;
            else f[tag]=text;
        }
    }
    if(!genres.isEmpty())f["genres"]=genres.join(" · ");
}
void Library::scan(){
    if(m_scanning){m_rescanRequested=true;return;}if(!m_db.isOpen()||m_folders.isEmpty())return;
    m_scanning=true;m_paused=false;m_scanStatus="Explorando tus carpetas…";emit scanningChanged();emit scanStatusChanged();
    const auto folders=m_folders;
    QHash<QString,QVariantMap> cache;
    for(const auto &v:query("SELECT path,size,mtime,data FROM files"))cache[v.toMap()["path"].toString()]=v.toMap();
    QHash<QString,QVariantMap> rules;
    for(const auto &v:query("SELECT * FROM rules"))rules[v.toMap()["path"].toString()]=v.toMap();
    QHash<int,QString> compactRules;for(const auto &v:query("SELECT * FROM compact_rules")){auto row=v.toMap();compactRules[row.value("folder").toInt()]=row.value("reading").toString();}
    const bool ignore=setting("ignoreSamples",true).toBool();
    const auto state=m_scanState=std::make_shared<ScanState>();
    m_scan.setFuture(QtConcurrent::run([this,folders,cache,rules,compactRules,ignore,state]{
        QVariantList files,complete;QStringList errors,watched;
        for(const auto &v:folders){
            auto folder=v.toMap();const auto root=folder["path"].toString();QFileInfo rootInfo(root);
            if(!rootInfo.isDir()||!rootInfo.isReadable()){errors<<"Carpeta no disponible: "+root;continue;}
            bool readable=true;QVariantList current;
            QStringList pending{root};
            while(!pending.isEmpty()&&!state->cancel){
                const auto path=pending.takeLast();
                {QMutexLocker lock(&state->mutex);while(state->pause&&!state->cancel)state->condition.wait(&state->mutex,500);}
                if(state->cancel)break;
                QFileInfo info(path);if(!info.isReadable()){readable=false;errors<<"No puedo leer: "+path;continue;}
                if(watched.size()<8000)watched<<path;
                QDir dir(path);
                const auto entries=dir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDir::Name);
                QHash<QString,QList<int>> groups;
                QList<QPair<QFileInfo,Names::Result>> parsed;
                const auto relative=QDir(root).relativeFilePath(path);
                const auto dirs=relative=="."?QStringList{}:relative.split('/');
                for(const auto &file:entries){
                    if(file.isDir()){if(!file.fileName().startsWith('.')&&(!ignore||!Names::ignoredDir(file.fileName())))pending<<file.absoluteFilePath();continue;}
                    if(QStringList{"srt","ass","ssa","vtt","ttml"}.contains(file.suffix().toLower())){current<<QVariantMap{{"path",file.absoluteFilePath()},{"url",QUrl::fromLocalFile(file.absoluteFilePath()).toString()},{"folder",folder.value("id")},{"name",file.fileName()},{"size",file.size()},{"mtime",file.lastModified().toMSecsSinceEpoch()},{"title",file.completeBaseName()},{"kind","SUBTITLE"},{"confidence",100},{"season",-1},{"episode",0}};continue;}
                    if(!Names::isVideo(file.fileName()))continue;
                    auto p=Names::parse(file.fileName(),dirs,folder["kind"].toString(),rootInfo.fileName(),compactRules.value(folder.value("id").toInt()),ignore);
                    if(p.compactNumber)groups[Names::key(p.title)]<<p.compactNumber;
                    parsed<<qMakePair(file,p);
                }
                QList<QPair<QString,Names::Result>> names;
                for(auto &pair:parsed){
                    auto &p=pair.second;
                    if(p.compactNumber&&!compactRules.contains(folder.value("id").toInt())){const auto rule=Names::resolveCompactGroup(groups[Names::key(p.title)]);if(!rule.isEmpty())p=Names::parse(pair.first.fileName(),dirs,folder["kind"].toString(),rootInfo.fileName(),rule,ignore);}
                    names.append({pair.first.fileName(),p});
                }
                const auto fixes=folder["kind"]!="MOVIES"?Names::plainNumberingFixes(names,info.fileName(),rootInfo.fileName()):QMap<int,Names::Result>();
                for(auto it=fixes.cbegin();it!=fixes.cend();++it)parsed[it.key()].second=it.value();
                for(auto &pair:parsed){
                    {QMutexLocker lock(&state->mutex);while(state->pause&&!state->cancel)state->condition.wait(&state->mutex,500);}
                    if(state->cancel)break;
                    const auto file=pair.first;auto p=pair.second;
                    if(rules.contains(path)&&p.kind!=Names::Kind::Ignored){auto rule=rules[path];int n=Names::numberHint(file.fileName());if(n>0&&n-rule["offset"].toInt()>=0){p.kind=Names::Kind::Episode;p.title=rule["title"].toString();p.season=rule["season"].toInt();p.episode=n-rule["offset"].toInt();p.confidence=100;p.reason.clear();}}
                    const auto old=cache.value(file.absoluteFilePath());
                    QVariantMap f;
                    if(old["size"].toLongLong()==file.size()&&old["mtime"].toLongLong()==file.lastModified().toMSecsSinceEpoch())f=unjson(old["data"].toString());
                    // Reparse cached files too: learned rules and folder type may have changed.
                    f["path"]=file.absoluteFilePath();f["url"]=QUrl::fromLocalFile(file.absoluteFilePath()).toString();f["folder"]=folder["id"];f["name"]=file.fileName();f["size"]=file.size();f["mtime"]=file.lastModified().toMSecsSinceEpoch();f["title"]=p.title;f["episodeTitle"]=p.episodeTitle;f["year"]=p.year;f["season"]=p.season;f["episode"]=p.episode;f["episodeEnd"]=p.episodeEnd;f["confidence"]=p.confidence;f["reason"]=p.reason;
                    f["kind"]=p.kind==Names::Kind::Movie?"MOVIE":p.kind==Names::Kind::Episode?"EPISODE":p.kind==Names::Kind::Ignored?"IGNORED":"UNKNOWN";
                    f["compactToken"]=p.compactToken;f["compactNumber"]=p.compactNumber;f["altSeason"]=p.altSeason;f["altEpisode"]=p.altEpisode;f["yearHint"]=p.yearHint;f["seasonFromFolder"]=p.seasonFromFolder;f["rawEpisode"]=p.episode;f["ruleApplied"]=rules.contains(path);
                    if(!f.contains("probe")&&p.kind!=Names::Kind::Ignored){const auto details=probe(file.absoluteFilePath());f["probe"]=details;f["duration"]=qint64(details["format"].toMap()["duration"].toDouble()*1000);for(const auto &stream:details["streams"].toList()){auto s=stream.toMap();if(s["codec_type"]=="video"){f["width"]=s["width"];f["height"]=s["height"];f["codec"]=s["codec_name"];break;}}}
                    localMetadata(f,file);current<<f;
                    if(current.size()%20==0){const auto status=QString("%1 archivos · %2").arg(files.size()+current.size()).arg(path);QMetaObject::invokeMethod(this,[this,status]{m_scanStatus=status;emit scanStatusChanged();},Qt::QueuedConnection);}
                }
            }
            if(readable&&!state->cancel)complete<<folder["id"];
            files<<current;
        }
        QList<Names::EpisodeRef> refs;
        auto group=[](const QVariantMap &file){return file.value("folder").toString()+':'+Names::key(file.value("title").toString());};
        for(const auto &value:files){const auto file=value.toMap();if(file.value("kind")=="EPISODE"&&!file.value("ruleApplied").toBool())refs.append({group(file),file.value("season").toInt(),file.value("episode").toInt(),file.value("seasonFromFolder").toBool()});}
        const auto offsets=Names::absoluteOffsets(refs);
        for(auto &value:files){
            auto file=value.toMap();const auto identity=qMakePair(group(file),file.value("season").toInt());
            if(file.value("kind")!="EPISODE"||file.value("ruleApplied").toBool()||!offsets.contains(identity))continue;
            const auto correction=offsets.value(identity);const int raw=file.value("episode").toInt(),episode=raw-correction.offset;
            file["episode"]=episode;if(file.value("episodeEnd").toInt()>0)file["episodeEnd"]=file.value("episodeEnd").toInt()-correction.offset;
            file["confidence"]=correction.sure?qMax(88,file.value("confidence").toInt()):qMin(65,file.value("confidence").toInt());
            file["reason"]=correction.sure?QString():QString("Numeración seguida: ¿el %1 es T%2·E%3?").arg(raw).arg(file.value("season").toInt()).arg(episode,2,10,QChar('0'));
            value=file;
        }
        return QVariantMap{{"files",files},{"complete",complete},{"errors",errors},{"watched",watched},{"cancelled",state->cancel.load()}};
    }));
}
void Library::pauseScan(){if(!m_scanState||!m_scanning)return;m_paused=!m_paused;m_scanState->pause=m_paused;if(!m_paused)m_scanState->condition.wakeAll();emit scanningChanged();}
void Library::saveProgress(const QString &path,qint64 position,qint64 duration){
    if(path.isEmpty()||duration<=0||position<0)return;
    const bool watched=position>=duration*0.9;
    exec("INSERT INTO progress VALUES(?,?,?,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=excluded.position,duration=excluded.duration,watched=CASE WHEN excluded.watched=1 THEN 1 WHEN excluded.position<5000 THEN progress.watched ELSE 0 END,updated=excluded.updated",{setting("profileId",1),path,watched?0:qMin(position,duration),duration,watched,now()});
}
void Library::markWatched(const QString &path,bool watched){auto f=media(path);exec("INSERT INTO progress VALUES(?,?,0,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=0,watched=excluded.watched,updated=excluded.updated",{setting("profileId",1),path,f["duration"],watched,now()});reload();}
void Library::correct(const QString &path,const QString &title,int season,int episode,bool folderRule,int year){
    const auto rows=query("SELECT data FROM files WHERE path=?",{path});
    if(media(path).isEmpty()||rows.isEmpty()||title.trimmed().isEmpty()||season<0||episode<0){fail("Escribe un título y números válidos.");return;}
    auto f=unjson(rows.first().toMap().value("data").toString());
    if(Names::key(f.value("title").toString())!=Names::key(title))exec("DELETE FROM metadata WHERE path=?",{path});
    auto prior=query("SELECT data FROM file_decisions WHERE path=?",{path});auto decision=prior.isEmpty()?QVariantMap():unjson(prior.first().toMap().value("data").toString());
    decision["title"]=title.trimmed();decision["season"]=season;decision["episode"]=episode;decision["kind"]=episode>0?"EPISODE":"MOVIE";decision["confidence"]=100;decision["reason"]="";
    if(year>=0)decision["year"]=year;
    exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{path,json(decision)});
    if(folderRule&&episode>0&&f.value("compactNumber").toInt()<=0){int hint=Names::numberHint(QFileInfo(path).fileName());if(hint>0)exec("INSERT INTO rules VALUES(?,?,?,?) ON CONFLICT(path) DO UPDATE SET title=excluded.title,season=excluded.season,offset=excluded.offset",{QFileInfo(path).absolutePath(),title.trimmed(),season,hint-episode});}
    if(folderRule&&episode>0&&f.value("compactNumber").toInt()>0){int n=f.value("compactNumber").toInt();QString reading=season==n/10&&episode==n%10?"SSE":season==n/100&&episode==n%100?"SEE":"";if(!reading.isEmpty())exec("INSERT INTO compact_rules VALUES(?,?) ON CONFLICT(folder) DO UPDATE SET reading=excluded.reading",{f.value("folder"),reading});}
    reload();if(folderRule)scan();
}
void Library::accept(const QString &path){const auto rows=query("SELECT data FROM files WHERE path=?",{path});if(media(path).isEmpty()||rows.isEmpty())return;auto file=unjson(rows.first().toMap().value("data").toString());if(file["kind"]=="UNKNOWN"||file["kind"]=="IGNORED"){fail("Corrige el título y el tipo antes de aceptar.");return;}auto prior=query("SELECT data FROM file_decisions WHERE path=?",{path});auto decision=prior.isEmpty()?QVariantMap():unjson(prior.first().toMap().value("data").toString());for(const auto &key:{"kind","title","year","season","episode"})decision[key]=file.value(key);decision["confidence"]=100;decision["reason"]="";exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{path,json(decision)});reload();}
void Library::acceptSafeGuesses(){
    auto files=rawFiles();if(!m_db.transaction())return;bool ok=true;for(const auto &v:files){auto file=v.toMap();if(file.value("hidden").toBool()||file.value("confidence").toInt()<50||file.value("confidence").toInt()>=80||file.value("altSeason",-1).toInt()>=0||file.value("title").toString().isEmpty()||(file.value("kind")!="MOVIE"&&file.value("kind")!="EPISODE"))continue;auto rows=query("SELECT data FROM file_decisions WHERE path=?",{file.value("path")});auto manual=rows.isEmpty()?QVariantMap():unjson(rows.first().toMap().value("data").toString());for(const auto &key:{"kind","title","year","season","episode"})manual[key]=file.value(key);manual["confidence"]=100;manual["reason"]="";ok=exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{file.value("path"),json(manual)})&&ok;}if(ok)m_db.commit();else m_db.rollback();reload();
}
void Library::setCompactRule(int folder,const QString &reading){if(reading!="SEE"&&reading!="SSE")return;bool exists=false;for(const auto &v:m_folders)if(v.toMap().value("id").toInt()==folder)exists=true;if(!exists)return;exec("INSERT INTO compact_rules VALUES(?,?) ON CONFLICT(folder) DO UPDATE SET reading=excluded.reading",{folder,reading});scan();}
void Library::setFileHidden(const QString &path,bool hidden){if(media(path).isEmpty())return;auto rows=query("SELECT data FROM file_decisions WHERE path=?",{path});auto decision=rows.isEmpty()?QVariantMap():unjson(rows.first().toMap().value("data").toString());decision["hidden"]=hidden;exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{path,json(decision)});reload();}
void Library::resetDecision(const QString &path){exec("DELETE FROM file_decisions WHERE path=?",{path});reload();}
void Library::preferFile(const QString &path){auto files=rawFiles();QVariantMap selected;for(const auto &v:files)if(v.toMap().value("path")==path)selected=v.toMap();if(selected.isEmpty())return;for(const auto &v:files){auto file=v.toMap();if(file.value("folder")!=selected.value("folder")||file.value("kind")!=selected.value("kind")||Catalog::showKey(file)!=Catalog::showKey(selected)||file.value("season")!=selected.value("season")||file.value("episode")!=selected.value("episode")||(file.value("kind")=="MOVIE"&&file.value("year")!=selected.value("year")))continue;auto rows=query("SELECT data FROM file_decisions WHERE path=?",{file.value("path")});auto decision=rows.isEmpty()?QVariantMap():unjson(rows.first().toMap().value("data").toString());decision["preferred"]=file.value("path")==path;exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{file.value("path"),json(decision)});}reload();}
void Library::createProfile(const QString &name){if(name.trimmed().isEmpty())return;exec("INSERT INTO profiles(name) VALUES(?)",{name.trimmed()});reload();}
void Library::editProfile(int id,const QString &name,const QString &color){
    if(name.trimmed().isEmpty())return;bool found=false;for(const auto &v:m_profiles)if(v.toMap().value("id").toInt()==id)found=true;if(!found)return;
    exec("UPDATE profiles SET name=? WHERE id=?",{name.trimmed(),id});if(QRegularExpression("^#[0-9a-fA-F]{6}$").match(color).hasMatch())m_settings->setValue(QString("profileColor/%1").arg(id),color);m_settings->sync();reload();
}
void Library::deleteProfile(int id){
    if(m_profiles.size()<2)return;bool found=false;int fallback=0;for(const auto &v:m_profiles){int current=v.toMap().value("id").toInt();if(current==id)found=true;else if(!fallback)fallback=current;}if(!found)return;
    if(!m_db.transaction())return;bool ok=exec("DELETE FROM progress WHERE profile=?",{id})&&exec("DELETE FROM profiles WHERE id=?",{id});if(!ok){m_db.rollback();return;}if(!m_db.commit())return;
    for(const auto &key:m_settings->allKeys())if(key==QString("profileColor/%1").arg(id)||key==QString("visibleFolders/%1").arg(id)||key.startsWith(QString("playback/%1/").arg(id))||key.startsWith(QString("continueHidden/%1/").arg(id)))m_settings->remove(key);
    if(setting("profileId",1).toInt()==id)m_settings->setValue("profileId",fallback);m_settings->sync();reload();
}
void Library::selectProfile(int id){for(const auto &v:m_profiles)if(v.toMap()["id"].toInt()==id){setSetting("profileId",id);reload();return;}}
void Library::setFolderVisible(int id,bool enabled){
    const auto key=QString("visibleFolders/%1").arg(setting("profileId",1).toInt());
    QStringList ids;
    if(m_settings->contains(key))ids=m_settings->value(key).toStringList();
    else for(const auto &folder:m_folders)ids<<folder.toMap()["id"].toString();
    bool exists=false;for(const auto &folder:m_folders)if(folder.toMap()["id"].toInt()==id)exists=true;
    if(!exists)return;
    ids.removeAll(QString::number(id));if(enabled)ids<<QString::number(id);
    m_settings->setValue(key,ids);m_settings->sync();reload();
}
void Library::showAllFolders(){m_settings->remove(QString("visibleFolders/%1").arg(setting("profileId",1).toInt()));m_settings->sync();reload();}
void Library::hideContinue(const QString &path,bool hidden){
    const auto file=media(path);if(file.isEmpty())return;
    const auto key=QString("continueHidden/%1/").arg(setting("profileId",1).toInt())+Catalog::continueKey(file);
    qint64 lastPlayed=file.value("updated").toLongLong();
    for(const auto &value:m_shows){const auto show=value.toMap();if(Catalog::continueKey(show)==Catalog::continueKey(file))lastPlayed=qMax(lastPlayed,show.value("updated").toLongLong());}
    if(hidden)m_settings->setValue(key,qMax(now(),lastPlayed));else m_settings->remove(key);
    m_settings->sync();reload();
}
void Library::setShowSpoilers(const QString &show,int mode){
    if(show.isEmpty())return;const auto key="spoilerShows/"+show;
    if(mode<0)m_settings->remove(key);else m_settings->setValue(key,mode>0);
    m_settings->sync();reload();
}
QVariant Library::playbackSetting(const QString &name,const QString &path,const QVariant &fallback)const{
    const auto prefix=QString("playback/%1/%2/").arg(setting("profileId",1).toInt()).arg(name);
    if(m_settings->contains(prefix+path))return m_settings->value(prefix+path);
    if(name=="audioChoice"||name=="subtitleChoice") {
        const auto current=media(path);
        if(current.value("kind")=="EPISODE"){
            QVariant inheritedChoice;qint64 latest=-1;
            for(const auto &value:m_files){const auto file=value.toMap();if(file.value("path")==path||file.value("kind")!="EPISODE"||Catalog::showKey(file)!=Catalog::showKey(current))continue;
                const auto inherited=prefix+file.value("path").toString();const auto played=file.value("updated").toLongLong();
                if(played>latest&&m_settings->contains(inherited)){latest=played;inheritedChoice=m_settings->value(inherited);}
            }
            if(inheritedChoice.isValid())return inheritedChoice;
        }
    }
    // Preserve choices from the first desktop release in the original profile.
    if(setting("profileId",1).toInt()==1&&m_settings->contains(name+'/'+path))return m_settings->value(name+'/'+path);
    return fallback;
}
void Library::setPlaybackSetting(const QString &name,const QString &path,const QVariant &value){m_settings->setValue(QString("playback/%1/%2/").arg(setting("profileId",1).toInt()).arg(name)+path,value);m_settings->sync();}
QString Library::trackLanguage(const QVariant &value)const{return TrackChoice::language(value);}
QString Library::trackLanguageName(const QVariant &value)const{return TrackChoice::languageName(value);}
QString Library::encodeTrack(const QVariantMap &track)const{return TrackChoice::encode(track);}
int Library::pickTrack(const QVariantList &options,const QString &saved)const{return TrackChoice::pick(options,saved);}
int Library::preferredTrack(const QVariantList &options,bool forcedOnly)const{return TrackChoice::preferred(options,setting("languages",QString("es,en")).toString().split(',',Qt::SkipEmptyParts),forcedOnly);}
QVariantMap Library::media(const QString &path)const{for(const auto &v:m_files)if(v.toMap()["path"]==path)return v.toMap();return {};}
QVariantList Library::externalSubtitles(const QString &path)const{
    QFileInfo file(path);QVariantList out;
    for(const auto &sub:file.dir().entryInfoList({"*.srt","*.ass","*.ssa","*.vtt"},QDir::Files,QDir::Name)){
        if(sub.completeBaseName().compare(file.completeBaseName(),Qt::CaseInsensitive)==0||sub.completeBaseName().startsWith(file.completeBaseName()+'.',Qt::CaseInsensitive)){
            const auto suffix=sub.completeBaseName().mid(file.completeBaseName().size()).toLower();QString lang;
            for(const auto &part:suffix.split(QRegularExpression("[._ -]+"),Qt::SkipEmptyParts)){auto code=TrackChoice::language(part);if(!code.isEmpty()){lang=code;break;}}
            out<<QVariantMap{{"name",sub.fileName()},{"url",QUrl::fromLocalFile(sub.absoluteFilePath())},{"lang",lang},{"forced",suffix.contains(QRegularExpression("(?:^|[._ -])(forced|forzados?|forzadas?)(?:$|[._ -])"))},{"id","ext:"+sub.fileName()}};
        }
    }return out;
}
QVariantList Library::readSubtitles(const QUrl &url)const{
    QFile file(url.toLocalFile());if(file.size()>16*1024*1024||!file.open(QIODevice::ReadOnly))return {};
    auto text=QString::fromUtf8(file.readAll());text.replace("\r","");
    auto time=[](const QString &value)->qint64{auto parts=value.trimmed().split(QRegularExpression("[:,.]"));if(parts.size()<3)return -1;const int offset=parts.size()==4?1:0;const int h=offset?parts[0].toInt():0;const int m=parts[offset].toInt();const int s=parts[offset+1].toInt();auto fraction=parts[offset+2];while(fraction.size()<3)fraction+='0';return ((h*60LL+m)*60+s)*1000+fraction.left(3).toInt();};
    QVariantList out;
    if(QFileInfo(file).suffix().compare("ass",Qt::CaseInsensitive)==0||QFileInfo(file).suffix().compare("ssa",Qt::CaseInsensitive)==0){
        for(const auto &line:text.split('\n')){if(!line.startsWith("Dialogue:",Qt::CaseInsensitive))continue;auto fields=line.mid(9).split(',');if(fields.size()<10)continue;auto caption=fields.mid(9).join(',');caption.remove(QRegularExpression(R"(\{[^}]*\})"));caption.replace("\\N","\n").replace("\\n","\n").replace("\\h"," ");out<<QVariantMap{{"start",time(fields[1])},{"end",time(fields[2])},{"text",caption}};}
    }else{
        const QRegularExpression timing(R"((\d{1,2}:\d{2}(?::\d{2})?[,.]\d{1,3})\s*-->\s*(\d{1,2}:\d{2}(?::\d{2})?[,.]\d{1,3})[^\n]*\n([\s\S]*?)(?=\n\s*\n|$))");
        auto matches=timing.globalMatch(text);while(matches.hasNext()){auto m=matches.next();auto caption=m.captured(3).trimmed();caption.remove(QRegularExpression("<[^>]+>"));out<<QVariantMap{{"start",time(m.captured(1))},{"end",time(m.captured(2))},{"text",caption}};}
    }
    std::sort(out.begin(),out.end(),[](const QVariant &a,const QVariant &b){return a.toMap()["start"].toLongLong()<b.toMap()["start"].toLongLong();});return out;
}
QVariantMap Library::nextEpisode(const QString &path)const{
    const auto current=media(path);if(current["kind"]!="EPISODE")return {};
    QVariantMap next;
    const auto after=qMakePair(current.value("season").toInt(),current.value("episode").toInt());
    for(const auto &value:m_files){const auto file=value.toMap();if(file.value("kind")!="EPISODE"||Catalog::showKey(file)!=Catalog::showKey(current)||!file.value("available").toBool())continue;
        const auto number=qMakePair(file.value("season").toInt(),file.value("episode").toInt());
        if(number>after&&(next.isEmpty()||number<qMakePair(next.value("season").toInt(),next.value("episode").toInt())))next=file;
    }
    return next;
}
