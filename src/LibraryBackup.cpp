#include "Library.h"
#include "BackupCodec.h"
#include "Catalog.h"
#include "NameParser.h"
#include <QJsonDocument>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QSet>
#include <QSqlError>

static QString encode(const QVariantMap &data){return QString::fromUtf8(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact));}
static QVariantMap decode(const QVariant &data){return QJsonDocument::fromJson(data.toString().toUtf8()).toVariant().toMap();}
static QString profileKey(int id,const QString &name,const QString &path){return QString("playback/%1/%2/").arg(id).arg(name)+path;}

QVariantMap Library::buildBackup(bool includeFolders,bool includeDecisions,bool includeSettings)const{
    const auto all=rawFiles();QHash<QString,QVariantMap> byPath;QHash<int,QVariantMap> roots;
    QVariantList folders;
    for(const auto &v:m_folders){auto root=v.toMap();roots[root.value("id").toInt()]=root;folders<<QVariantMap{{"treeUri",QUrl::fromLocalFile(root.value("path").toString()).toString()},{"displayPath",root.value("path")},{"name",QFileInfo(root.value("path").toString()).fileName()},{"kind",root.value("kind")}};}
    auto identity=[&](const QVariantMap &file){return QVariantMap{{"name",file.value("name")},{"size",file.value("size")},{"relPath",file.value("relPath")},{"folder",QUrl::fromLocalFile(file.value("root").toString()).toString()}};};
    for(const auto &v:all)byPath[v.toMap().value("path").toString()]=v.toMap();
    const int pid=setting("profileId",1).toInt();QVariantList items,nativeItems,profiles,playback,visibility,hidden;
    for(const auto &v:m_profiles)if(v.toMap().value("id").toInt()==pid)profiles<<v;
    for(const auto &v:query("SELECT * FROM progress WHERE profile=?",{pid})){auto progress=v.toMap();const auto file=byPath.value(progress.value("path").toString());if(file.isEmpty())continue;auto item=identity(file);item["positionMs"]=progress.value("position");item["durationMs"]=progress.value("duration");item["watched"]=progress.value("watched").toBool();item["updatedAt"]=progress.value("updated");item["subtitleDelayMs"]=qint64(playbackSetting("subDelay",file.value("path").toString(),0).toDouble()*1000);items<<item;item["_profile"]=profile();nativeItems<<item;}
    QVariantList decisions,rules;
    if(includeDecisions){
        for(const auto &v:query("SELECT * FROM file_decisions")){auto row=v.toMap();const auto file=byPath.value(row.value("path").toString());if(file.isEmpty())continue;auto decision=identity(file),manual=decode(row.value("data"));for(const auto &key:{"kind","title","year","season","episode","hidden","preferred"})if(manual.contains(key))decision[key]=manual.value(key);decisions<<decision;}
        for(const auto &v:query("SELECT * FROM rules")){auto r=v.toMap();const auto path=r.value("path").toString();for(const auto &f:folders){auto root=f.toMap();auto base=QUrl(root.value("treeUri").toString()).toLocalFile();if(path==base||path.startsWith(base+'/')){auto relative=QDir(base).relativeFilePath(path);rules<<QVariantMap{{"folder",root.value("treeUri")},{"pattern","dir:"+relative},{"dirPath",relative=="."?QString():relative},{"interpretation",r.value("season").toString()+'|'+r.value("offset").toString()+'|'+r.value("title").toString()}};break;}}}
        for(const auto &v:query("SELECT * FROM compact_rules")){auto r=v.toMap();auto root=roots.value(r.value("folder").toInt());if(!root.isEmpty())rules<<QVariantMap{{"folder",QUrl::fromLocalFile(root.value("path").toString()).toString()},{"pattern","compact3"},{"interpretation",r.value("reading")},{"dirPath",""}};}
    }
    QVariantMap preferences,absolute,spoilers;
    if(includeSettings){
        for(const auto &key:{"scanOnOpen","ignoreSamples","resume","resumeRewindSeconds","autoNext","playerProgressBar","spoilerFree","seasonArt","absoluteMode","decoder","skipSeconds","tmdbKey","metaLang"})if(m_settings->contains(key))preferences[key]=m_settings->value(key);
        preferences["languages"]=setting("languages",QString("es,en")).toString().split(',',Qt::SkipEmptyParts);const auto scale=setting("subtitleScale",1.0).toDouble();preferences["subtitleSize"]=scale<0.9?"S":scale>1.1?"L":"M";
        const auto visibleKey=QString("visibleFolders/%1").arg(pid);QVariantList references;for(const auto &id:m_settings->value(visibleKey).toStringList()){auto root=roots.value(id.toInt());if(!root.isEmpty())references<<QUrl::fromLocalFile(root.value("path").toString()).toString();}visibility<<QVariantMap{{"profile",profile()},{"all",!m_settings->contains(visibleKey)},{"folders",references}};
        for(const auto &v:all){const auto file=v.toMap();const auto path=file.value("path").toString();for(const auto &name:{"audioChoice","subtitleChoice","externalSubtitleUrl","subDelay"}){const auto key=profileKey(pid,name,path);if(m_settings->contains(key)){auto item=identity(file);item["_profile"]=profile();item["setting"]=name;item["value"]=m_settings->value(key);playback<<item;}}}
    }
    if(includeDecisions){for(const auto &key:m_settings->allKeys()){if(key.startsWith("absoluteShows/"))absolute[key.mid(14)]=m_settings->value(key);if(key.startsWith("spoilerShows/"))spoilers[key.mid(13)]=m_settings->value(key);const auto prefix=QString("continueHidden/%1/").arg(pid);if(key.startsWith(prefix)){const auto identityKey=key.mid(prefix.size());QVariantMap row{{"_profile",profile()},{"key",identityKey},{"updatedAt",m_settings->value(key)}};if(identityKey.startsWith("m:")){auto file=byPath.value(identityKey.mid(2));if(file.isEmpty())continue;row["file"]=identity(file);}hidden<<row;}}}
    QVariantMap data{{"format","arca-desktop"},{"version",2},{"app","Arca Desktop"},{"exportedAt",QDateTime::currentMSecsSinceEpoch()},{"profile",profile()},{"folders",includeFolders?folders:QVariantList()},{"items",items},{"decisions",decisions},{"rules",rules},{"absoluteShows",absolute},{"spoilerShows",spoilers}};
    if(includeSettings)data["settings"]=preferences;
    // Android reads the standard fields; desktop keeps its extra per-profile state here.
    data["desktop"]=QVariantMap{{"profiles",profiles},{"items",nativeItems},{"playback",playback},{"visibility",visibility},{"continueHidden",hidden}};
    return data;
}
bool Library::exportBackup(const QUrl &url,bool folders,bool decisions,bool settings){
    const auto data=buildBackup(folders,decisions,settings);QSaveFile file(url.toLocalFile());const auto bytes=QJsonDocument::fromVariant(data).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){fail("No puedo escribir la copia: "+file.errorString());return false;}
    m_backupStatus=QString("Copia exportada · %1 archivos de progreso").arg(data.value("items").toList().size());emit changed();return true;
}
QVariantMap Library::inspectBackup(const QUrl &url){
    QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024){fail("No puedo leer la copia o supera 32 MB.");return {};}
    auto read=BackupCodec::read(file.readAll());if(!read.error.isEmpty()){fail(read.error);return {};}
    const auto &data=read.data;return {{"profile",data.value("profile")},{"items",data.value("items").toList().size()},{"folders",data.value("folders").toList().size()},{"decisions",data.value("decisions").toList().size()+data.value("rules").toList().size()},{"settings",!data.value("settings").toMap().isEmpty()}};
}
bool Library::importBackup(const QUrl &url,bool folders,bool decisions,bool settings){
    QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024){fail("No puedo leer la copia o supera 32 MB.");return false;}
    auto read=BackupCodec::read(file.readAll());if(!read.error.isEmpty()){fail(read.error);return false;}return restoreBackup(read.data,folders,decisions,settings);
}
bool Library::restoreBackup(QVariantMap data,bool restoreFolders,bool restoreDecisions,bool restoreSettings,bool pending){
    const auto folders=data.value("folders").toList();
    if(restoreFolders)for(const auto &v:folders){auto folder=v.toMap();QUrl url(folder.value("treeUri").toString());if(url.isLocalFile()&&QFileInfo(url.toLocalFile()).isDir()&&QFileInfo(url.toLocalFile()).isReadable()){bool covered=false;for(const auto &value:m_folders){const auto root=value.toMap().value("path").toString();if(url.toLocalFile()==root||url.toLocalFile().startsWith(root+'/'))covered=true;}if(!covered)addFolder(url,folder.value("kind").toString());}}
    QVariantMap aliases;for(const auto &v:query("SELECT * FROM folder_aliases")){auto row=v.toMap();aliases[row.value("source").toString()]=row.value("path");}
    const auto files=rawFiles();auto match=[&](const QVariantMap &item){return BackupCodec::match(item,files,folders,m_folders,aliases);};
    if(!m_db.transaction())return false;
    QHash<QString,int> profiles;for(const auto &v:m_profiles){auto p=v.toMap();profiles[p.value("name").toString()]=p.value("id").toInt();}
    bool ok=true;auto profileId=[&](QString name){if(name.trimmed().isEmpty())name=data.value("profile","Principal").toString();if(profiles.contains(name))return profiles.value(name);if(!exec("INSERT INTO profiles(name) VALUES(?)",{name})){ok=false;return 0;}auto rows=query("SELECT id FROM profiles WHERE name=? ORDER BY id DESC LIMIT 1",{name});if(rows.isEmpty()){ok=false;return 0;}profiles[name]=rows.first().toMap().value("id").toInt();return profiles.value(name);};
    profileId(data.value("profile").toString());for(const auto &v:data.value("profiles").toList())profileId(v.toMap().value("name").toString());
    QVariantList waitingItems,waitingDecisions,waitingRules,waitingPlayback,waitingVisibility,waitingHidden;QVariantMap settingUpdates;int applied=0;
    for(const auto &v:data.value("items").toList()){
        auto item=v.toMap(),file=match(item);if(file.isEmpty()){waitingItems<<item;continue;}const int pid=profileId(item.value("_profile").toString());const auto path=file.value("path").toString();const auto existing=query("SELECT updated FROM progress WHERE profile=? AND path=?",{pid,path});if(!existing.isEmpty()&&existing.first().toMap().value("updated").toLongLong()>=item.value("updatedAt").toLongLong())continue;
        ok=exec("INSERT INTO progress VALUES(?,?,?,?,?,?) ON CONFLICT(profile,path) DO UPDATE SET position=excluded.position,duration=excluded.duration,watched=excluded.watched,updated=excluded.updated",{pid,path,item.value("positionMs"),item.value("durationMs"),item.value("watched").toBool(),item.value("updatedAt")})&&ok;
        settingUpdates[profileKey(pid,"subDelay",path)]=item.value("subtitleDelayMs",0).toDouble()/1000;++applied;
    }
    if(restoreDecisions)for(const auto &v:data.value("decisions").toList()){
        auto item=v.toMap(),file=match(item);if(file.isEmpty()){waitingDecisions<<item;continue;}const auto path=file.value("path").toString();auto old=query("SELECT data FROM file_decisions WHERE path=?",{path});auto manual=old.isEmpty()?QVariantMap():decode(old.first().toMap().value("data"));for(const auto &key:{"kind","title","year","season","episode","hidden","preferred"})if(item.contains(key))manual[key]=item.value(key);if(!manual.value("kind").isNull()){manual["confidence"]=100;manual["reason"]="";}ok=exec("INSERT INTO file_decisions VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{path,encode(manual)})&&ok;
    }
    bool rescan=false;
    if(restoreDecisions)for(const auto &v:data.value("rules").toList()){
        auto rule=v.toMap();QString root=BackupCodec::folderPath(rule.value("folder").toString(),folders,m_folders,aliases);const auto nativePath=rule.value("_nativePath").toString();
        if(root.isEmpty()&&!nativePath.isEmpty())for(const auto &f:m_folders){const auto candidate=f.toMap().value("path").toString();if(nativePath==candidate||nativePath.startsWith(candidate+'/')){root=candidate;rule["dirPath"]=QDir(root).relativeFilePath(nativePath);break;}}
        if(root.isEmpty()){waitingRules<<rule;continue;}
        if(rule.value("pattern")=="compact3"){int id=0;for(const auto &f:m_folders)if(f.toMap().value("path")==root)id=f.toMap().value("id").toInt();if(!id){waitingRules<<rule;continue;}ok=exec("INSERT INTO compact_rules VALUES(?,?) ON CONFLICT(folder) DO UPDATE SET reading=excluded.reading",{id,rule.value("interpretation")})&&ok;}
        else{const auto relative=QDir::cleanPath(rule.value("dirPath").toString());if(relative==".."||relative.startsWith("../")||QDir::isAbsolutePath(relative)){ok=false;break;}const auto path=relative=="."?root:QDir(root).filePath(relative);const auto parts=rule.value("interpretation").toString().split('|');ok=exec("INSERT INTO rules VALUES(?,?,?,?) ON CONFLICT(path) DO UPDATE SET title=excluded.title,season=excluded.season,offset=excluded.offset",{path,parts.mid(2).join('|'),parts[0].toInt(),parts[1].toInt()})&&ok;}rescan=true;
    }
    if(restoreSettings){
        auto preferences=data.value("settings").toMap();for(const auto &key:{"scanOnOpen","ignoreSamples","resume","resumeRewindSeconds","autoNext","playerProgressBar","spoilerFree","seasonArt","absoluteMode","decoder","skipSeconds","tmdbKey","metaLang"})if(preferences.contains(key)&&!preferences.value(key).isNull())settingUpdates[key]=preferences.value(key);
        if(preferences.contains("ignoreSamples")&&preferences.value("ignoreSamples").toBool()!=setting("ignoreSamples",true).toBool())rescan=true;
        if(preferences.contains("languages"))settingUpdates["languages"]=preferences.value("languages").toStringList().join(',');if(preferences.contains("subtitleSize")){const auto size=preferences.value("subtitleSize").toString();settingUpdates["subtitleScale"]=size=="S"?0.8:size=="L"?1.3:1.0;}
        for(const auto &v:data.value("playback").toList()){auto item=v.toMap(),file=match(item);if(file.isEmpty()){waitingPlayback<<item;continue;}const auto name=item.value("setting").toString();if(!QStringList{"audioChoice","subtitleChoice","externalSubtitleUrl","subDelay"}.contains(name))continue;settingUpdates[profileKey(profileId(item.value("_profile").toString()),name,file.value("path").toString())]=item.value("value");}
        for(const auto &v:data.value("visibility").toList()){auto item=v.toMap();const auto key=QString("visibleFolders/%1").arg(profileId(item.value("profile").toString()));if(item.value("all").toBool()){settingUpdates[key]=QVariant();continue;}QStringList ids;bool missing=false;for(const auto &ref:item.value("folders").toList()){auto path=BackupCodec::folderPath(ref.toString(),folders,m_folders,aliases);int id=0;for(const auto &f:m_folders){auto root=f.toMap().value("path").toString();if(path==root||path.startsWith(root+'/'))id=f.toMap().value("id").toInt();}if(!id)missing=true;else ids<<QString::number(id);}if(missing)waitingVisibility<<item;else settingUpdates[key]=ids;}
    }
    if(restoreDecisions){for(const auto &prefix:{"absoluteShows/","spoilerShows/"}){auto values=data.value(prefix==QString("absoluteShows/")?"absoluteShows":"spoilerShows").toMap();for(auto it=values.cbegin();it!=values.cend();++it)if(!m_settings->contains(QString(prefix)+it.key()))settingUpdates[QString(prefix)+it.key()]=it.value();}
        for(const auto &v:data.value("continueHidden").toList()){auto item=v.toMap();auto key=item.value("key").toString();if(key.startsWith("m:")){auto file=match(item.value("file").toMap());if(file.isEmpty()){waitingHidden<<item;continue;}key="m:"+file.value("path").toString();}if(key.startsWith("m:")||key.startsWith("s:"))settingUpdates[QString("continueHidden/%1/").arg(profileId(item.value("_profile").toString()))+key]=item.value("updatedAt");}
    }
    if(!ok){m_db.rollback();fail("No puedo restaurar la copia; los datos existentes se conservan.");return false;}
    if(!m_db.commit()){fail(m_db.lastError().text());return false;}
    for(auto it=settingUpdates.cbegin();it!=settingUpdates.cend();++it){if(it.value().isValid())m_settings->setValue(it.key(),it.value());else m_settings->remove(it.key());}m_settings->sync();
    const auto waitingCount=waitingItems.size()+waitingDecisions.size()+waitingRules.size()+waitingPlayback.size()+waitingVisibility.size()+waitingHidden.size();
    if(!pending&&waitingCount){auto waiting=data;waiting.remove("desktop");waiting["items"]=waitingItems;waiting["decisions"]=waitingDecisions;waiting["rules"]=waitingRules;waiting["playback"]=waitingPlayback;waiting["visibility"]=waitingVisibility;waiting["continueHidden"]=waitingHidden;waiting.remove("settings");waiting.remove("absoluteShows");waiting.remove("spoilerShows");const auto bytes=encode(waiting).toUtf8();const auto id=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());exec("INSERT OR REPLACE INTO pending_backups VALUES(?,?)",{id,QString::fromUtf8(bytes)});}
    if(pending){data.remove("desktop");data["items"]=waitingItems;data["decisions"]=waitingDecisions;data["rules"]=waitingRules;data["playback"]=waitingPlayback;data["visibility"]=waitingVisibility;data["continueHidden"]=waitingHidden;const auto id=data.take("_pendingId");if(waitingCount)exec("UPDATE pending_backups SET data=? WHERE id=?",{encode(data),id});else exec("DELETE FROM pending_backups WHERE id=?",{id});}
    m_backupStatus=QString("Copia unida · %1 progresos actualizados · %2 datos pendientes de sus carpetas o archivos").arg(applied).arg(waitingCount);refreshPendingFolders();reload();if(rescan){m_rescanRequested=true;if(!m_scanning)QTimer::singleShot(0,this,[this]{if(m_rescanRequested){m_rescanRequested=false;scan();}});}return true;
}
void Library::applyPendingBackups(){for(const auto &v:query("SELECT * FROM pending_backups")){auto row=v.toMap(),data=decode(row.value("data"));data["_pendingId"]=row.value("id");restoreBackup(data,false,true,true,true);}refreshPendingFolders();}
void Library::refreshPendingFolders(){
    QMap<QString,QVariantMap> pending;
    for(const auto &v:query("SELECT data FROM pending_backups")){auto data=decode(v.toMap().value("data"));QSet<QString> refs;for(const auto &section:{"items","decisions","rules","playback"})for(const auto &item:data.value(section).toList())refs.insert(item.toMap().value("folder").toString());for(const auto &entry:data.value("visibility").toList())for(const auto &ref:entry.toMap().value("folders").toList())refs.insert(ref.toString());
        for(const auto &entry:data.value("continueHidden").toList())refs.insert(entry.toMap().value("file").toMap().value("folder").toString());
        for(const auto &ref:refs){QVariantMap folder;for(const auto &f:data.value("folders").toList())if(f.toMap().value("treeUri")==ref){folder=f.toMap();break;}if(folder.isEmpty())folder={{"treeUri",ref},{"displayPath",ref},{"name",ref.isEmpty()?"Archivos pendientes":QFileInfo(QUrl(ref).toLocalFile()).fileName()},{"kind","AUTO"}};if(folder.value("name").toString().isEmpty())folder["name"]="Carpeta pendiente";pending[ref]=folder;}
    }
    m_pendingFolders.clear();for(const auto &folder:pending)m_pendingFolders<<folder;
}
void Library::bindImportedFolder(const QString &reference,const QUrl &url,const QString &kind){
    QFileInfo dir(url.toLocalFile());if(!dir.isDir()||!dir.isReadable()){fail("No puedo leer esa carpeta.");return;}const auto path=dir.canonicalFilePath();bool watched=false;
    for(const auto &v:m_folders){auto root=v.toMap().value("path").toString();if(path==root||path.startsWith(root+'/'))watched=true;}
    if(!watched)addFolder(QUrl::fromLocalFile(path),kind);if(!reference.isEmpty())exec("INSERT INTO folder_aliases VALUES(?,?) ON CONFLICT(source) DO UPDATE SET path=excluded.path",{reference,path});scan();
}

void Library::clearPendingBackup(){exec("DELETE FROM pending_backups");m_pendingFolders.clear();m_backupStatus="Datos pendientes descartados.";emit changed();}
