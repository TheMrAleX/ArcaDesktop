#include "Library.h"
#include "NameParser.h"
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
                    ok=exec("INSERT INTO files(path,folder,size,mtime,data,available) VALUES(?,?,?,?,?,1) ON CONFLICT(path) DO UPDATE SET folder=excluded.folder,size=excluded.size,mtime=excluded.mtime,data=CASE WHEN files.corrected=1 THEN files.data ELSE excluded.data END,available=1",
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
        emit scanningChanged();emit scanStatusChanged();reload();
    });
    reload();
    if(setting("scanOnOpen",true).toBool())QTimer::singleShot(100,this,&Library::scan);
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
void Library::setSetting(const QString &key,const QVariant &value){m_settings->setValue(key,value);m_settings->sync();emit changed();}
bool Library::onboarded()const{return setting("onboarded",false).toBool();}
void Library::finishWelcome(){setSetting("onboarded",true);}
QString Library::profile()const{const auto id=setting("profileId",1).toInt();for(const auto &p:m_profiles)if(p.toMap()["id"].toInt()==id)return p.toMap()["name"].toString();return "Principal";}
void Library::reload(){
    m_folders=query("SELECT * FROM folders ORDER BY path");m_profiles=query("SELECT * FROM profiles ORDER BY id");
    m_files.clear();
    const auto rows=query("SELECT f.path,f.data,f.available,COALESCE(p.position,0) AS position,COALESCE(p.duration,0) AS progressDuration,COALESCE(p.watched,0) AS watched,COALESCE(p.updated,0) AS updated FROM files f LEFT JOIN progress p ON p.path=f.path AND p.profile=? ORDER BY json_extract(f.data,'$.title') COLLATE NOCASE,json_extract(f.data,'$.season'),json_extract(f.data,'$.episode'),f.path",{setting("profileId",1)});
    QSet<QString> episodes;
    QHash<QString,QVariantMap> metadata;
    for(const auto &v:query("SELECT * FROM metadata")){auto row=v.toMap();metadata[row["path"].toString()]=unjson(row["data"].toString());}
    for(const auto &v:rows){auto row=v.toMap();auto f=unjson(row.take("data").toString());for(auto it=row.begin();it!=row.end();++it)f[it.key()]=it.value();
        f["fileTitle"]=f["title"];for(auto it=metadata[f["path"].toString()].cbegin();it!=metadata[f["path"].toString()].cend();++it)f[it.key()]=it.value();
        const auto id=Names::key(f["title"].toString())+"|"+f["season"].toString()+"|"+f["episode"].toString();
        if(f["kind"]=="EPISODE"){f["duplicate"]=episodes.contains(id);episodes.insert(id);}
        m_files<<f;
    }
    emit changed();
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
    while(!xml.atEnd()){
        xml.readNext();if(!xml.isStartElement())continue;
        auto tag=xml.name().toString();
        if(tag=="title"||tag=="plot"||tag=="genre"||tag=="year") {auto text=xml.readElementText(QXmlStreamReader::IncludeChildElements);if(!text.isEmpty())f[tag]=text;}
    }
}
void Library::scan(){
    if(m_scanning||!m_db.isOpen()||m_folders.isEmpty())return;
    m_scanning=true;m_paused=false;m_scanStatus="Explorando tus carpetas…";emit scanningChanged();emit scanStatusChanged();
    const auto folders=m_folders;
    QHash<QString,QVariantMap> cache;
    for(const auto &v:query("SELECT path,size,mtime,data FROM files"))cache[v.toMap()["path"].toString()]=v.toMap();
    QHash<QString,QVariantMap> rules;
    for(const auto &v:query("SELECT * FROM rules"))rules[v.toMap()["path"].toString()]=v.toMap();
    const bool ignore=setting("ignoreSamples",true).toBool();
    const auto state=m_scanState=std::make_shared<ScanState>();
    m_scan.setFuture(QtConcurrent::run([this,folders,cache,rules,ignore,state]{
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
                    if(!Names::isVideo(file.fileName()))continue;
                    auto p=Names::parse(file.fileName(),dirs,folder["kind"].toString(),rootInfo.fileName(),{},ignore);
                    if(p.compactNumber)groups[Names::key(p.title)]<<p.compactNumber;
                    parsed<<qMakePair(file,p);
                }
                QList<int> numbers;
                for(auto &pair:parsed){
                    auto &p=pair.second;
                    if(p.compactNumber){const auto rule=Names::resolveCompactGroup(groups[Names::key(p.title)]);if(!rule.isEmpty())p=Names::parse(pair.first.fileName(),dirs,folder["kind"].toString(),rootInfo.fileName(),rule,ignore);}
                    const auto n=Names::plainNumber(pair.first.fileName());
                    if(n&&p.yearHint==0&&(p.kind==Names::Kind::Unknown||(p.kind==Names::Kind::Episode&&(p.confidence<80||p.compactToken==QString::number(p.compactNumber)))||(p.kind==Names::Kind::Movie&&Names::clean(pair.first.completeBaseName())==QString::number(n))))numbers<<n;
                }
                const bool plain=folder["kind"]!="MOVIES"&&Names::plainNumbering(numbers);
                for(auto &pair:parsed){
                    {QMutexLocker lock(&state->mutex);while(state->pause&&!state->cancel)state->condition.wait(&state->mutex,500);}
                    if(state->cancel)break;
                    const auto file=pair.first;auto p=pair.second;
                    if(plain&&numbers.contains(Names::plainNumber(file.fileName()))){p.kind=Names::Kind::Episode;p.episode=Names::plainNumber(file.fileName());p.season=Names::seasonFromDir(info.fileName());if(p.season<0)p.season=1;p.title=Names::clean(dirs.isEmpty()?rootInfo.fileName():dirs.last());p.confidence=86;p.reason.clear();}
                    if(rules.contains(path)&&p.kind!=Names::Kind::Ignored){auto rule=rules[path];int n=Names::numberHint(file.fileName());if(n>0){p.kind=Names::Kind::Episode;p.title=rule["title"].toString();p.season=rule["season"].toInt();p.episode=n-rule["offset"].toInt();p.confidence=100;p.reason.clear();}}
                    const auto old=cache.value(file.absoluteFilePath());
                    QVariantMap f;
                    if(old["size"].toLongLong()==file.size()&&old["mtime"].toLongLong()==file.lastModified().toMSecsSinceEpoch())f=unjson(old["data"].toString());
                    // Reparse cached files too: learned rules and folder type may have changed.
                    f["path"]=file.absoluteFilePath();f["url"]=QUrl::fromLocalFile(file.absoluteFilePath()).toString();f["folder"]=folder["id"];f["name"]=file.fileName();f["size"]=file.size();f["mtime"]=file.lastModified().toMSecsSinceEpoch();f["title"]=p.title;f["episodeTitle"]=p.episodeTitle;f["year"]=p.year;f["season"]=p.season;f["episode"]=p.episode;f["episodeEnd"]=p.episodeEnd;f["confidence"]=p.confidence;f["reason"]=p.reason;
                    f["kind"]=p.kind==Names::Kind::Movie?"MOVIE":p.kind==Names::Kind::Episode?"EPISODE":p.kind==Names::Kind::Ignored?"IGNORED":"UNKNOWN";
                    if(!f.contains("probe")&&p.kind!=Names::Kind::Ignored){const auto details=probe(file.absoluteFilePath());f["probe"]=details;f["duration"]=qint64(details["format"].toMap()["duration"].toDouble()*1000);for(const auto &stream:details["streams"].toList()){auto s=stream.toMap();if(s["codec_type"]=="video"){f["width"]=s["width"];f["height"]=s["height"];f["codec"]=s["codec_name"];break;}}}
                    localMetadata(f,file);current<<f;
                    if(current.size()%20==0){const auto status=QString("%1 archivos · %2").arg(files.size()+current.size()).arg(path);QMetaObject::invokeMethod(this,[this,status]{m_scanStatus=status;emit scanStatusChanged();},Qt::QueuedConnection);}
                }
            }
            if(readable&&!state->cancel)complete<<folder["id"];
            files<<current;
        }
        return QVariantMap{{"files",files},{"complete",complete},{"errors",errors},{"watched",watched},{"cancelled",state->cancel.load()}};
    }));
}
void Library::pauseScan(){if(!m_scanState||!m_scanning)return;m_paused=!m_paused;m_scanState->pause=m_paused;if(!m_paused)m_scanState->condition.wakeAll();emit scanningChanged();}
void Library::saveProgress(const QString &path,qint64 position,qint64 duration){
    if(path.isEmpty()||duration<=0||position<0)return;
    const bool watched=position>=duration*0.9;
    exec("INSERT INTO progress VALUES(?,?,?,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=excluded.position,duration=excluded.duration,watched=MAX(progress.watched,excluded.watched),updated=excluded.updated",{setting("profileId",1),path,watched?0:qMin(position,duration),duration,watched,now()});
}
void Library::markWatched(const QString &path,bool watched){auto f=media(path);exec("INSERT INTO progress VALUES(?,?,0,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=0,watched=excluded.watched,updated=excluded.updated",{setting("profileId",1),path,f["duration"],watched,now()});reload();}
void Library::correct(const QString &path,const QString &title,int season,int episode,bool folderRule){
    auto f=media(path);if(f.isEmpty()||title.trimmed().isEmpty()||season<0||episode<0){fail("Escribe un título y números válidos.");return;}
    f["title"]=title.trimmed();f["season"]=season;f["episode"]=episode;f["kind"]=episode>0?"EPISODE":"MOVIE";f["confidence"]=100;f["reason"]="";
    exec("UPDATE files SET data=?,corrected=1 WHERE path=?",{json(f),path});
    if(folderRule&&episode>0){int hint=Names::numberHint(QFileInfo(path).fileName());if(hint>0)exec("INSERT INTO rules VALUES(?,?,?,?) ON CONFLICT(path) DO UPDATE SET title=excluded.title,season=excluded.season,offset=excluded.offset",{QFileInfo(path).absolutePath(),title.trimmed(),season,hint-episode});}
    reload();if(folderRule)scan();
}
void Library::accept(const QString &path){auto f=media(path);if(f["kind"]=="UNKNOWN"||f["kind"]=="IGNORED"){fail("Corrige el título y el tipo antes de aceptar.");return;}f["confidence"]=100;f["reason"]="";exec("UPDATE files SET data=?,corrected=1 WHERE path=?",{json(f),path});reload();}
void Library::createProfile(const QString &name){if(name.trimmed().isEmpty())return;exec("INSERT INTO profiles(name) VALUES(?)",{name.trimmed()});reload();}
void Library::selectProfile(int id){for(const auto &v:m_profiles)if(v.toMap()["id"].toInt()==id){setSetting("profileId",id);reload();return;}}
QVariantMap Library::media(const QString &path)const{for(const auto &v:m_files)if(v.toMap()["path"]==path)return v.toMap();return {};}
QVariantList Library::externalSubtitles(const QString &path)const{
    QFileInfo file(path);QVariantList out;
    for(const auto &sub:file.dir().entryInfoList({"*.srt","*.ass","*.ssa","*.vtt"},QDir::Files,QDir::Name)){
        if(sub.completeBaseName().compare(file.completeBaseName(),Qt::CaseInsensitive)==0||sub.completeBaseName().startsWith(file.completeBaseName()+'.',Qt::CaseInsensitive))out<<QVariantMap{{"name",sub.fileName()},{"url",QUrl::fromLocalFile(sub.absoluteFilePath())}};
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
    for(const auto &v:m_files){auto f=v.toMap();if(f["kind"]!="EPISODE"||Names::key(f["title"].toString())!=Names::key(current["title"].toString())||!f["available"].toBool())continue;
        if(f["season"].toInt()>current["season"].toInt()||(f["season"]==current["season"]&&f["episode"].toInt()>current["episode"].toInt()))return f;}
    return {};
}
bool Library::exportBackup(const QUrl &url){
    QVariantMap data{{"format","arca-desktop"},{"version",1},{"profiles",query("SELECT * FROM profiles")},{"progress",query("SELECT * FROM progress")},{"rules",query("SELECT * FROM rules")}};
    QSaveFile file(url.toLocalFile());if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument::fromVariant(data).toJson())<0||!file.commit()){fail("No puedo escribir la copia: "+file.errorString());return false;}return true;
}
bool Library::importBackup(const QUrl &url){
    QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024){fail("No puedo leer la copia o supera 32 MB.");return false;}
    QJsonParseError error;auto doc=QJsonDocument::fromJson(file.readAll(),&error);auto data=doc.toVariant().toMap();
    if(error.error!=QJsonParseError::NoError||data["format"]!="arca-desktop"||data["version"].toInt()!=1){fail("Formato de copia no compatible. Se necesita una copia de Arca Desktop v1.");return false;}
    if(!m_db.transaction())return false;
    bool ok=true;QHash<int,int> ids;
    for(const auto &v:data["profiles"].toList()){auto p=v.toMap();if(p["name"].toString().trimmed().isEmpty()){ok=false;break;}auto existing=query("SELECT id FROM profiles WHERE name=?",{p["name"]});if(existing.isEmpty()){ok=exec("INSERT INTO profiles(name) VALUES(?)",{p["name"]})&&ok;existing=query("SELECT id FROM profiles WHERE name=?",{p["name"]});}if(!existing.isEmpty())ids[p["id"].toInt()]=existing.first().toMap()["id"].toInt();}
    for(const auto &v:data["progress"].toList()){auto p=v.toMap();if(!ids.contains(p["profile"].toInt())||p["path"].toString().isEmpty()||p["position"].toLongLong()<0||p["duration"].toLongLong()<0){ok=false;break;}ok=exec("INSERT INTO progress VALUES(?,?,?,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=excluded.position,duration=excluded.duration,watched=excluded.watched,updated=excluded.updated WHERE excluded.updated>progress.updated",{ids[p["profile"].toInt()],p["path"],p["position"],p["duration"],p["watched"],p["updated"]})&&ok;}
    // Rules are only useful for folders that this computer already watches.
    for(const auto &v:data["rules"].toList()){auto r=v.toMap();bool local=false;for(const auto &f:m_folders){auto root=f.toMap()["path"].toString();if(r["path"].toString()==root||r["path"].toString().startsWith(root+'/'))local=true;}if(local)ok=exec("INSERT OR IGNORE INTO rules VALUES(?,?,?,?)",{r["path"],r["title"],r["season"],r["offset"]})&&ok;}
    if(!ok){m_db.rollback();fail("La copia contiene datos inválidos; no se ha importado.");return false;}if(!m_db.commit()){fail(m_db.lastError().text());return false;}reload();return true;
}

void Library::tmdb(const QString &endpoint,const QVariantMap &parameters,std::function<void(QVariantMap)> done){
    auto token=setting("tmdbKey",QString()).toString().trimmed();
    if(token.isEmpty()){fail("Añade tu clave de TMDB en Ajustes → Metadatos de internet.");done({});return;}
    QUrl url("https://api.themoviedb.org/3/"+endpoint);QUrlQuery query;
    query.addQueryItem("language",setting("metaLang",QString("es-ES")).toString());
    for(auto it=parameters.cbegin();it!=parameters.cend();++it)query.addQueryItem(it.key(),it.value().toString());
    if(!token.contains('.'))query.addQueryItem("api_key",token);
    url.setQuery(query);QNetworkRequest request(url);request.setTransferTimeout(15000);
    if(token.contains('.'))request.setRawHeader("Authorization","Bearer "+token.toUtf8());
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64){if(received>4*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,done]{
        QVariantMap data;
        if(reply->error()==QNetworkReply::NoError)data=QJsonDocument::fromJson(reply->readAll()).toVariant().toMap();
        else fail("No puedo consultar TMDB. Comprueba la conexión y la clave; tu biblioteca local sigue disponible.");
        reply->deleteLater();done(data);
    });
}
void Library::findMetadata(const QString &title,bool series){
    m_searching=true;m_matches.clear();emit matchesChanged();const auto generation=++m_searchGeneration;
    tmdb(series?"search/tv":"search/movie",{{"query",title},{"include_adult","false"}},[this,generation,series](QVariantMap response){
        if(generation!=m_searchGeneration)return;
        for(const auto &v:response["results"].toList()){
            auto item=v.toMap();m_matches<<QVariantMap{{"id",item["id"]},{"title",item[series?"name":"title"]},{"year",item[series?"first_air_date":"release_date"].toString().left(4)},{"plot",item["overview"]}};
        }m_searching=false;emit matchesChanged();
    });
}
void Library::cacheImage(const QString &path,const QString &key,const QString &remote){
    if(!remote.startsWith('/')||remote.contains(".."))return;
    const auto name=QString::fromLatin1(QCryptographicHash::hash(remote.toUtf8(),QCryptographicHash::Sha256).toHex())+".jpg";
    QDir().mkpath(m_dataDir+"/art");const auto destination=m_dataDir+"/art/"+name;
    auto update=[this,path,key,destination]{auto rows=query("SELECT data FROM metadata WHERE path=?",{path});if(rows.isEmpty())return;auto data=unjson(rows.first().toMap()["data"].toString());data[key]=QUrl::fromLocalFile(destination).toString();exec("UPDATE metadata SET data=? WHERE path=?",{json(data),path});reload();};
    if(QFileInfo::exists(destination)){update();return;}
    QNetworkRequest request(QUrl("https://image.tmdb.org/t/p/"+QString(key=="poster"?"w500":"w1280")+remote));request.setTransferTimeout(15000);
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 bytes,qint64){if(bytes>10*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,destination,update]{if(reply->error()==QNetworkReply::NoError){QSaveFile output(destination);const auto bytes=reply->readAll();if(output.open(QIODevice::WriteOnly)&&output.write(bytes)==bytes.size()&&output.commit())update();}reply->deleteLater();});
}
void Library::useMetadata(const QString &path,int id,bool series){
    if(id<=0)return;
    const auto original=media(path);const auto originalTitle=Names::key(original.value("fileTitle",original["title"]).toString());
    tmdb((series?"tv/":"movie/")+QString::number(id),{},[this,path,series,originalTitle](QVariantMap response){
        if(response.isEmpty())return;
        QStringList genres;for(const auto &v:response["genres"].toList())genres<<v.toMap()["name"].toString();
        QVariantMap metadata{{"title",response[series?"name":"title"]},{"plot",response["overview"]},{"year",response[series?"first_air_date":"release_date"].toString().left(4).toInt()},{"genres",genres.join(" · ")},{"tmdbId",response["id"]}};
        QStringList paths{path};
        if(series){paths.clear();for(const auto &v:m_files){auto file=v.toMap();if(file["kind"]=="EPISODE"&&Names::key(file.value("fileTitle",file["title"]).toString())==originalTitle)paths<<file["path"].toString();}}
        for(const auto &target:paths){exec("INSERT INTO metadata VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{target,json(metadata)});cacheImage(target,"poster",response["poster_path"].toString());cacheImage(target,"backdrop",response["backdrop_path"].toString());}
        reload();
    });
}
