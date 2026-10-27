#include "BackupCodec.h"
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QSet>

BackupCodec::ReadResult BackupCodec::read(const QByteArray &bytes){
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())return {{},"La copia no es un JSON válido."};
    auto data=document.toVariant().toMap();const int version=data.value("version").toInt();
    if(version<1||version>2)return {{},"Versión de copia no compatible."};
    const auto format=data.value("format").toString();
    if(!format.isEmpty()&&format!="arca-desktop")return {{},"El archivo no es una copia de Arca."};
    if(format.isEmpty()&&!data.contains("items"))return {{},"No encuentro progreso de Arca en el archivo."};
    for(const auto &field:{"folders","items","decisions","rules","profiles","progress"})if(data.contains(field)&&data.value(field).metaType().id()!=QMetaType::QVariantList)return {{},"La copia contiene una lista inválida."};
    const auto native=data.value("desktop").toMap();
    if(format=="arca-desktop"&&version==1){
        QMap<int,QString> profiles;for(const auto &v:data.value("profiles").toList()){auto p=v.toMap();profiles[p.value("id").toInt()]=p.value("name").toString();}
        QVariantList items;for(const auto &v:data.value("progress").toList()){auto p=v.toMap();if(!profiles.contains(p.value("profile").toInt()))return {{},"La copia contiene un perfil desconocido."};items<<QVariantMap{{"_profile",profiles.value(p.value("profile").toInt())},{"_path",p.value("path")},{"name",QFileInfo(p.value("path").toString()).fileName()},{"size",0},{"_sizeUnknown",true},{"relPath",""},{"folder",""},{"positionMs",p.value("position")},{"durationMs",p.value("duration")},{"watched",p.value("watched").toBool()},{"updatedAt",p.value("updated")}};}
        QVariantList rules;for(const auto &v:data.value("rules").toList()){auto r=v.toMap();rules<<QVariantMap{{"_nativePath",r.value("path")},{"folder",""},{"dirPath",""},{"pattern","dir:"},{"interpretation",r.value("season").toString()+'|'+r.value("offset").toString()+'|'+r.value("title").toString()}};}
        data["items"]=items;data["rules"]=rules;data["decisions"]=QVariantList();data["profile"]="Principal";
    }else if(format=="arca-desktop"&&!native.isEmpty()){
        data["items"]=native.value("items",data.value("items"));data["profiles"]=native.value("profiles");data["playback"]=native.value("playback");data["visibility"]=native.value("visibility");data["continueHidden"]=native.value("continueHidden");
    }
    if(data.value("profile").toString().trimmed().isEmpty())data["profile"]="Principal";
    const auto folders=data.value("folders").toList();QSet<QString> refs;
    for(const auto &v:folders){auto f=v.toMap();if(f.value("treeUri").toString().isEmpty()||!QStringList{"MOVIES","SERIES","MIXED","AUTO"}.contains(f.value("kind").toString()))return {{},"La copia contiene una carpeta inválida."};refs.insert(f.value("treeUri").toString());}
    QVariantList items;
    for(const auto &v:data.value("items").toList()){
        auto item=v.toMap();if(item.value("name").toString().isEmpty()||item.value("positionMs").toLongLong()<0||item.value("durationMs").toLongLong()<0||item.value("updatedAt").toLongLong()<0||item.value("size").toLongLong()<0)return {{},"La copia contiene un progreso inválido."};
        if(!item.contains("positionMs")||!item.contains("durationMs")||!item.contains("updatedAt"))return {{},"Faltan campos de progreso en la copia."};
        if(item.value("_profile").toString().trimmed().isEmpty())item["_profile"]=data.value("profile");items<<item;
    }data["items"]=items;
    for(const auto &v:data.value("decisions").toList()){auto d=v.toMap();const auto kind=d.value("kind").toString();if(d.value("name").toString().isEmpty()||(!kind.isEmpty()&&!QStringList{"MOVIE","EPISODE","UNKNOWN","SUBTITLE","IGNORED"}.contains(kind))||(!d.value("season").isNull()&&d.value("season").toInt()<0)||(!d.value("episode").isNull()&&d.value("episode").toInt()<0))return {{},"La copia contiene una corrección inválida."};}
    for(const auto &v:data.value("rules").toList()){auto rule=v.toMap();const auto relative=QDir::cleanPath(rule.value("dirPath").toString());if(relative==".."||relative.startsWith("../")||QDir::isAbsolutePath(relative))return {{},"Ruta de regla inválida."};const auto pattern=rule.value("pattern").toString(),interpretation=rule.value("interpretation").toString();if(pattern=="compact3"){if(interpretation!="SEE"&&interpretation!="SSE")return {{},"Regla compacta inválida."};}else if(pattern.startsWith("dir:")){auto parts=interpretation.split('|');bool seasonOk=false,offsetOk=false;int season=parts.value(0).toInt(&seasonOk);parts.value(1).toInt(&offsetOk);if(parts.size()<3||!seasonOk||!offsetOk||season<0)return {{},"Regla de numeración inválida."};}else return {{},"La copia contiene una regla no compatible."};}
    for(const auto &v:data.value("profiles").toList())if(v.toMap().value("name").toString().trimmed().isEmpty())return {{},"Nombre de perfil inválido."};
    return {data,{}};
}
QString BackupCodec::folderPath(const QString &reference,const QVariantList &folders,const QVariantList &watched,const QVariantMap &aliases){
    if(reference.isEmpty())return {};
    if(aliases.contains(reference))return aliases.value(reference).toString();
    QString name,path;for(const auto &v:folders){auto folder=v.toMap();if(folder.value("treeUri")==reference){name=folder.value("name").toString();path=folder.value("displayPath").toString();break;}}
    QSet<QString> exact,byName;const auto local=QUrl(reference).toLocalFile();
    for(const auto &v:watched){auto root=v.toMap().value("path").toString();if(root==local||root==path||QUrl::fromLocalFile(root).toString()==reference)exact.insert(root);if(!name.isEmpty()&&QFileInfo(root).fileName()==name)byName.insert(root);}
    if(exact.size()==1)return *exact.cbegin();if(byName.size()==1)return *byName.cbegin();return {};
}
QVariantMap BackupCodec::match(const QVariantMap &item,const QVariantList &files,const QVariantList &folders,const QVariantList &watched,const QVariantMap &aliases){
    const auto reference=item.value("folder").toString();const auto root=folderPath(reference,folders,watched,aliases);
    if(!reference.isEmpty()&&root.isEmpty())return {};
    QVariantList candidates;
    for(const auto &v:files){auto file=v.toMap();auto path=file.value("path").toString();if(!root.isEmpty()&&path!=root&&!path.startsWith(root+'/'))continue;file["relative"]=root.isEmpty()?file.value("relPath"):QVariant(QDir(root).relativeFilePath(path));candidates<<file;}
    auto unique=[&](int mode){QVariantList out;for(const auto &v:candidates){auto file=v.toMap();bool exactPath=!item.value("_path").toString().isEmpty()&&file.value("path")==item.value("_path");bool relative=!item.value("relPath").toString().isEmpty()&&file.value("relative").toString()==QString(item.value("relPath").toString()).replace('\\','/');bool size=item.value("_sizeUnknown").toBool()||item.value("size").toLongLong()==file.value("size").toLongLong();if((mode==0&&exactPath)||(mode==1&&relative&&size)||(mode==2&&relative)||(mode==3&&file.value("name")==item.value("name")&&size)||(mode==4&&file.value("name")==item.value("name")))out<<file;}return out;};
    for(int mode=0;mode<5;++mode){auto found=unique(mode);if(found.size()==1)return found.first().toMap();if(found.size()>1)return {};}
    return {};
}
