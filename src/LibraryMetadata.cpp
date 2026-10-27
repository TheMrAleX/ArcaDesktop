#include "Library.h"
#include "Metadata.h"
#include "NameParser.h"
#include "Catalog.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

static QString pack(const QVariantMap &data){return QString::fromUtf8(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact));}
static QVariantMap unpack(const QVariant &data){return QJsonDocument::fromJson(data.toString().toUtf8()).toVariant().toMap();}
static bool generic(const QString &title){return title.trimmed().isEmpty()||QRegularExpression("^(episodio|episode|capítulo|capitulo|cap\\.?)\\s*\\d+$",QRegularExpression::CaseInsensitiveOption).match(title.trimmed()).hasMatch();}
static QString epTarget(const QString &show,int season,int episode){return "episode:"+show+'|'+QString::number(season)+'|'+QString::number(episode);}

QVariantList Library::rawFiles()const{
    QVariantList result;
    QHash<QString,QVariantMap> decisions;for(const auto &v:query("SELECT * FROM file_decisions")){auto row=v.toMap();decisions[row.value("path").toString()]=unpack(row.value("data"));}
    for(const auto &value:query("SELECT f.path,f.data,f.available,d.path AS root FROM files f JOIN folders d ON d.id=f.folder")){
        auto row=value.toMap(),file=unpack(row.value("data"));file["path"]=row.value("path");file["available"]=row.value("available");file["root"]=row.value("root");file["relPath"]=QDir(row.value("root").toString()).relativeFilePath(row.value("path").toString());const auto manual=decisions.value(file.value("path").toString());for(auto it=manual.cbegin();it!=manual.cend();++it)if(!it.value().isNull())file[it.key()]=it.value();result<<file;
    }return result;
}
void Library::setMetadataJob(const QString &key,const QVariantMap &state){m_metadataJobs[key]=state;emit metadataChanged();}
void Library::fetchJson(const QUrl &url,std::function<void(QVariant)> done){
    if(m_metadataTransport){m_metadataTransport(url,std::move(done));return;}
    QNetworkRequest request(url);request.setTransferTimeout(25000);request.setRawHeader("Accept","application/json");request.setRawHeader("User-Agent","ArcaDesktop native local media library");
    const auto token=setting("tmdbKey",QString()).toString();if(url.host()=="api.themoviedb.org"&&token.contains('.'))request.setRawHeader("Authorization","Bearer "+token.toUtf8());
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 count,qint64){if(count>8*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,done]{QVariant value;if(reply->error()==QNetworkReply::NoError)value=QJsonDocument::fromJson(reply->readAll()).toVariant();reply->deleteLater();done(value);});
}
void Library::tmdb(const QString &endpoint,const QVariantMap &parameters,std::function<void(QVariantMap)> done){
    auto token=setting("tmdbKey",QString()).toString().trimmed();
    if(token.isEmpty()){fail("Añade tu clave de TMDB en Ajustes → Metadatos de internet.");done({});return;}
    QUrl url("https://api.themoviedb.org/3/"+endpoint);QUrlQuery query;
    query.addQueryItem("language",parameters.value("language",setting("metaLang",QString("es-ES"))).toString());
    for(auto it=parameters.cbegin();it!=parameters.cend();++it)if(it.key()!="language")query.addQueryItem(it.key(),it.value().toString());
    if(!token.contains('.'))query.addQueryItem("api_key",token);url.setQuery(query);
    fetchJson(url,[this,done](QVariant result){auto data=result.toMap();if(data.isEmpty()||(data.contains("success")&&!data.value("success").toBool())){fail("No puedo consultar TMDB. Comprueba la conexión y la clave; tu biblioteca local sigue disponible.");done({});}else done(data);});
}
void Library::findMetadata(const QString &title,bool series){
    m_searching=true;m_matches.clear();emit matchesChanged();const auto generation=++m_searchGeneration;
    tmdb(series?"search/tv":"search/movie",{{"query",title},{"include_adult","false"}},[this,generation,series](QVariantMap response){
        if(generation!=m_searchGeneration)return;
        for(const auto &value:response.value("results").toList()){auto hit=value.toMap();m_matches<<QVariantMap{{"id",hit.value("id")},{"title",hit.value(series?"name":"title")},{"year",hit.value(series?"first_air_date":"release_date").toString().left(4)},{"plot",hit.value("overview")}};}
        m_searching=false;emit matchesChanged();
    });
}
void Library::downloadMetadata(const QString &path,const QString &input){
    const auto file=media(path);if(file.isEmpty())return;
    const bool series=file.value("kind")=="EPISODE";const auto key=series?Catalog::showKey(file):path;const int generation=++m_metadataGeneration[key];
    QString title=input.isEmpty()?file.value("fileTitle",file.value("title")).toString():input;int year=file.value("fileYear",file.value("year")).toInt();
    auto match=QRegularExpression("^(.*?)[\\s._-]*[([]?((?:19|20)\\d{2})[)\\]]?\\s*$").match(title.trimmed());if(match.hasMatch()&&!match.captured(1).trimmed().isEmpty()){title=match.captured(1).trimmed();year=match.captured(2).toInt();}
    setMetadataJob(key,{{"running",true},{"text","Buscando «"+title+"» en TMDB…"},{"fraction",0.0}});
    QVariantMap args{{"query",title}};if(year>0)args[series?"first_air_date_year":"year"]=year;
    tmdb(series?"search/tv":"search/movie",args,[this,path,key,title,year,series,generation](QVariantMap response){
        if(m_metadataGeneration.value(key)!=generation)return;
        const auto hits=response.value("results").toList();auto pool=Metadata::candidates(hits,title,year);if(pool.isEmpty()&&hits.size()==1)pool=hits;
        auto choices=[this,path,key,series,hits]{m_matches.clear();for(const auto &value:hits){auto hit=value.toMap();m_matches<<QVariantMap{{"id",hit.value("id")},{"title",hit.value(series?"name":"title")},{"year",hit.value(series?"first_air_date":"release_date").toString().left(4)},{"plot",hit.value("overview")}};}emit matchesChanged();setMetadataJob(key,{{"running",false},{"text",hits.isEmpty()?"No encuentro ese título.":"Hay varias fichas posibles. Elige la correcta."}});if(!hits.isEmpty())emit metadataChoicesReady(path);};
        if(pool.size()==1){useMetadata(path,pool.first().toMap().value("id").toInt(),series);return;}
        if(!series||pool.isEmpty()){choices();return;}
        auto remaining=std::make_shared<int>(qMin(4,int(pool.size())));auto scored=std::make_shared<QVariantList>();QVariantList files;for(const auto &value:rawFiles())if(Catalog::showKey(value.toMap())==key&&value.toMap().value("kind")=="EPISODE")files<<value;
        const int requests=*remaining;for(int i=0;i<requests;++i){const auto id=pool[i].toMap().value("id").toInt();tmdb("tv/"+QString::number(id),{},[this,id,path,key,generation,files,remaining,scored,choices](QVariantMap detail){if(m_metadataGeneration.value(key)!=generation)return;scored->append(QVariantMap{{"id",id},{"score",Metadata::structureScore(files,detail.value("seasons").toList())}});if(--*remaining>0)return;std::sort(scored->begin(),scored->end(),[](const QVariant &a,const QVariant &b){return a.toMap().value("score").toInt()>b.toMap().value("score").toInt();});if(scored->size()>1&&(*scored)[0].toMap().value("score").toInt()>(*scored)[1].toMap().value("score").toInt())useMetadata(path,scored->first().toMap().value("id").toInt(),true);else choices();});}
    });
}
void Library::applySeriesDetails(const QString &key,const QVariantMap &details){
    const auto rows=query("SELECT data FROM show_meta WHERE key=?",{key});auto data=rows.isEmpty()?QVariantMap():unpack(rows.first().toMap().value("data"));
    if(data.value("tmdbId").toInt()!=details.value("id").toInt()){data.clear();exec("DELETE FROM episode_meta WHERE showkey=?",{key});}
    QStringList genres;for(const auto &value:details.value("genres").toList())genres<<value.toMap().value("name").toString();
    data["title"]=details.value("name");data["plot"]=details.value("overview");data["genres"]=genres.join(" · ");data["year"]=details.value("first_air_date").toString().left(4).toInt();
    data["yearEnd"]=(details.value("status")=="Ended"||details.value("status")=="Canceled")?details.value("last_air_date").toString().left(4).toInt():0;
    data["tmdbId"]=details.value("id");data["seasonCounts"]=details.value("seasons");data["details"]=details;data["metaLang"]=setting("metaLang",QString("es-ES"));
    exec("INSERT INTO show_meta VALUES(?,?) ON CONFLICT(key) DO UPDATE SET data=excluded.data",{key,pack(data)});
    cacheImage("show:"+key,"poster",data.value("posterPickRemote",details.value("poster_path")).toString());cacheImage("show:"+key,"backdrop",data.value("backdropPickRemote",details.value("backdrop_path")).toString());reload();
}
void Library::applySeasonMetadata(const QString &key,const QVariantMap &season,const QVariantMap &english){
    const int number=season.value("season_number").toInt();const auto en=english.value("episodes").toList();
    const auto info=query("SELECT data FROM show_meta WHERE key=?",{key});const int tmdbId=info.isEmpty()?0:unpack(info.first().toMap().value("data")).value("tmdbId").toInt();
    for(const auto &value:season.value("episodes").toList()){
        const auto ep=value.toMap();const int n=ep.value("episode_number").toInt();QVariantMap fallback;for(const auto &v:en)if(v.toMap().value("episode_number").toInt()==n){fallback=v.toMap();break;}
        auto title=ep.value("name").toString();if(generic(title)&&!generic(fallback.value("name").toString()))title=fallback.value("name").toString();
        auto plot=ep.value("overview").toString();if(plot.isEmpty())plot=fallback.value("overview").toString();
        QVariantMap data{{"episodeTitle",title},{"episodeOverview",plot},{"runtimeMin",ep.value("runtime")},{"airDate",ep.value("air_date")},{"episodeTmdbId",ep.value("id")},{"tmdbId",tmdbId},{"_still",ep.value("still_path")}};
        exec("INSERT INTO episode_meta VALUES(?,?,?,?) ON CONFLICT(showkey,season,episode) DO UPDATE SET data=excluded.data",{key,number,n,pack(data)});cacheImage(epTarget(key,number,n),"still",ep.value("still_path").toString());
    }
    cacheImage("show:"+key,"season:"+QString::number(number),season.value("poster_path").toString());reload();
}
void Library::useMetadata(const QString &path,int id,bool series){
    if(id<=0)return;const auto file=media(path);if(file.isEmpty())return;const auto key=series?Catalog::showKey(file):path;const int generation=++m_metadataGeneration[key];
    setMetadataJob(key,{{"running",true},{"text","Descargando la ficha…"},{"fraction",0.05}});
    auto apply=[this,path,key,id,series,generation](QVariantMap details){
        if(m_metadataGeneration.value(key)!=generation)return;if(details.isEmpty()){setMetadataJob(key,{{"running",false},{"text","No he podido descargar la ficha."}});return;}
        if(!series){const auto rows=query("SELECT data FROM metadata WHERE path=?",{path});auto data=rows.isEmpty()?QVariantMap():unpack(rows.first().toMap().value("data"));if(data.value("tmdbId").toInt()!=id)data.clear();QStringList genres;for(const auto &v:details.value("genres").toList())genres<<v.toMap().value("name").toString();data["title"]=details.value("title");data["plot"]=details.value("overview");data["year"]=details.value("release_date").toString().left(4).toInt();data["genres"]=genres.join(" · ");data["tmdbId"]=id;
            exec("INSERT INTO metadata VALUES(?,?) ON CONFLICT(path) DO UPDATE SET data=excluded.data",{path,pack(data)});cacheImage(path,"poster",data.value("posterPickRemote",details.value("poster_path")).toString());cacheImage(path,"backdrop",data.value("backdropPickRemote",details.value("backdrop_path")).toString());reload();setMetadataJob(key,{{"running",false},{"text","Listo."},{"fraction",1.0}});return;}
        applySeriesDetails(key,details);QVariantList files;for(const auto &v:rawFiles())if(v.toMap().value("kind")=="EPISODE"&&Catalog::showKey(v.toMap())==key)files<<v;
        const auto ranges=Metadata::ranges(details.value("seasons").toList());auto choice=setting("absoluteShows/"+key);const auto mode=setting("absoluteMode",QString("ASK")).toString();if(!choice.isValid()&&mode!="ASK")choice=mode=="RENUMBER";
        if(Metadata::looksAbsolute(files,ranges)&&!choice.isValid()){m_absolutePrompt={{"showKey",key},{"path",path},{"tmdbId",id},{"title",details.value("name")},{"ranges",ranges},{"total",ranges.isEmpty()?0:ranges.last().toMap().value("last")}};setMetadataJob(key,{{"running",false},{"text","Confirma cómo leer la numeración corrida."},{"needsChoice",true}});emit metadataChanged();return;}
        fetchSeasons(key,id,generation);
    };
    tmdb((series?"tv/":"movie/")+QString::number(id),{},[this,id,series,apply](QVariantMap details){if(!details.isEmpty()&&details.value("overview").toString().isEmpty()&&setting("metaLang",QString("es-ES"))!="en-US")tmdb((series?"tv/":"movie/")+QString::number(id),{{"language","en-US"}},[details,apply](QVariantMap en)mutable{details["overview"]=en.value("overview");apply(details);});else apply(details);});
}
void Library::resolveAbsolute(int mode){
    if(m_absolutePrompt.isEmpty())return;const auto prompt=m_absolutePrompt;m_absolutePrompt.clear();emit metadataChanged();if(mode<0)return;
    const auto key=prompt.value("showKey").toString();m_settings->setValue("absoluteShows/"+key,mode==1);m_settings->sync();reload();fetchSeasons(key,prompt.value("tmdbId").toInt(),m_metadataGeneration.value(key));
}
void Library::fetchSeasons(const QString &key,int id,int generation){
    const auto rows=query("SELECT data FROM show_meta WHERE key=?",{key});if(rows.isEmpty())return;const auto data=unpack(rows.first().toMap().value("data"));const auto counts=data.value("seasonCounts").toList();
    auto remaining=std::make_shared<int>(0);for(const auto &v:counts)if(v.toMap().value("episode_count").toInt()>0)++*remaining;
    auto total=*remaining;auto finish=[this,key,id,generation,remaining,total,counts]{if(--*remaining>0){setMetadataJob(key,{{"running",true},{"text","Leyendo episodios…"},{"fraction",0.1+0.8*(total-*remaining)/qMax(1.0,double(total))}});return;}if(m_metadataGeneration.value(key)!=generation)return;QVariantList files;for(const auto &v:rawFiles())if(v.toMap().value("kind")=="EPISODE"&&Catalog::showKey(v.toMap())==key)files<<v;if(!Metadata::covers(files,counts)&&!Metadata::looksAbsolute(files,Metadata::ranges(counts)))findAlternative(key,id,counts,generation);else setMetadataJob(key,{{"running",false},{"text","Listo."},{"fraction",1.0}});reload();};
    if(!total){setMetadataJob(key,{{"running",false},{"text","La ficha no tiene una guía de episodios."}});return;}
    for(const auto &v:counts){auto c=v.toMap();if(c.value("episode_count").toInt()<=0)continue;const int season=c.value("season_number").toInt();tmdb("tv/"+QString::number(id)+"/season/"+QString::number(season),{},[this,key,id,season,generation,finish](QVariantMap response){
        if(m_metadataGeneration.value(key)!=generation)return;bool fallback=false;for(const auto &ep:response.value("episodes").toList()){auto e=ep.toMap();if(generic(e.value("name").toString())||e.value("overview").toString().isEmpty())fallback=true;}
        if(fallback&&setting("metaLang",QString("es-ES"))!="en-US")tmdb("tv/"+QString::number(id)+"/season/"+QString::number(season),{{"language","en-US"}},[this,key,generation,response,finish](QVariantMap en){if(m_metadataGeneration.value(key)!=generation)return;applySeasonMetadata(key,response,en);finish();});else{applySeasonMetadata(key,response);finish();}
    });}
}
void Library::findAlternative(const QString &key,int,const QVariantList &counts,int generation){
    QVariantList files;QString title;int year=0;for(const auto &v:rawFiles()){auto f=v.toMap();if(f.value("kind")=="EPISODE"&&Catalog::showKey(f)==key){files<<f;title=f.value("title").toString();year=f.value("year").toInt();}}
    setMetadataJob(key,{{"running",true},{"text","Buscando otra guía de episodios…"},{"fraction",0.9}});QUrl url("https://api.tvmaze.com/search/shows");QUrlQuery parameters;parameters.addQueryItem("q",title);url.setQuery(parameters);
    fetchJson(url,[this,key,generation,files,title,year,counts](QVariant response){
        if(m_metadataGeneration.value(key)!=generation)return;QVariantList hits;for(const auto &value:response.toList()){auto hit=value.toMap().value("show").toMap();auto name=Names::key(hit.value("name").toString());int date=hit.value("premiered").toString().left(4).toInt();if((name==key||(key.size()>=4&&(name.contains(key)||key.contains(name))))&&(year==0||date==0||qAbs(year-date)<=2)){hits<<hit;if(hits.size()==4)break;}}
        if(hits.isEmpty()){setMetadataJob(key,{{"running",false},{"text","Listo. Algunos episodios siguen sin datos."},{"fraction",1.0}});return;}
        auto remaining=std::make_shared<int>(hits.size());auto best=std::make_shared<QVariantMap>();const int baseline=Metadata::structureScore(files,counts);
        for(const auto &value:hits){auto hit=value.toMap();fetchJson(QUrl("https://api.tvmaze.com/shows/"+hit.value("id").toString()+"/episodes"),[this,key,generation,files,title,baseline,remaining,best,hit](QVariant result){
            if(m_metadataGeneration.value(key)!=generation)return;auto episodes=Metadata::tvmaze(result.toList());QMap<int,int> max;for(const auto &v:episodes){auto ep=v.toMap();int s=ep.value("season").toInt();max[s]=qMax(max.value(s),ep.value("episode").toInt());}QVariantList available;for(auto it=max.cbegin();it!=max.cend();++it)available<<QVariantMap{{"season",it.key()},{"count",it.value()}};int score=Metadata::structureScore(files,available);
            if(!episodes.isEmpty()&&Metadata::covers(files,available)&&score>baseline&&(best->isEmpty()||score>best->value("score").toInt()))*best={{"episodes",episodes},{"score",score},{"url",hit.value("url")}};
            if(--*remaining>0)return;
            auto save=[this,key,generation,best](const QVariantList &episodes,const QString &wiki){if(m_metadataGeneration.value(key)!=generation)return;auto rows=query("SELECT data FROM show_meta WHERE key=?",{key});if(rows.isEmpty())return;auto data=unpack(rows.first().toMap().value("data"));const auto ranges=Metadata::ranges(data.value("seasonCounts").toList());
                for(const auto &v:episodes){auto episode=v.toMap();int s=episode.value("season").toInt(),n=episode.value("episode").toInt();auto mapping=Metadata::absoluteEpisode(episode.value("absolute").toInt(),ranges);auto rows=query("SELECT data FROM episode_meta WHERE showkey=? AND season=? AND episode=?",{key,mapping.value("season"),mapping.value("episode")});auto info=rows.isEmpty()?QVariantMap():unpack(rows.first().toMap().value("data"));info["episodeTitle"]=episode.value("title");info["episodeOverview"]=episode.value("overview");info["runtimeMin"]=episode.value("runtime");info["airDate"]=episode.value("airDate");info["tmdbId"]=data.value("tmdbId");exec("INSERT INTO episode_meta VALUES(?,?,?,?) ON CONFLICT(showkey,season,episode) DO UPDATE SET data=excluded.data",{key,s,n,pack(info)});cacheImage(epTarget(key,s,n),"still",info.value("_still").toString());}
                data["episodeSource"]=wiki.isEmpty()?"TVmaze":"Wikipedia en español + TVmaze";data["episodeSourceUrl"]=wiki;data["episodeStructureUrl"]=best->value("url");exec("UPDATE show_meta SET data=? WHERE key=?",{pack(data),key});reload();setMetadataJob(key,{{"running",false},{"text","Listo. Episodios: "+data.value("episodeSource").toString()},{"fraction",1.0}});
            };
            if(best->isEmpty()){setMetadataJob(key,{{"running",false},{"text","Listo. Algunos episodios siguen sin datos."},{"fraction",1.0}});return;}
            if(setting("metaLang",QString("es-ES"))!="es-ES"){save(best->value("episodes").toList(),{});return;}
            QStringList pages;for(const auto &t:QStringList{title,key})pages<<"Anexo:Episodios de "+t<<"Anexo:Lista de episodios de "+t<<t+"/Lista de episodios"<<"Lista de episodios de "+t;pages.removeDuplicates();QUrl wiki("https://es.wikipedia.org/w/api.php");QUrlQuery q;for(const auto &p:QVariantMap{{"action","query"},{"prop","revisions"},{"rvprop","content"},{"rvslots","main"},{"redirects","1"},{"format","json"},{"formatversion","2"}}.toStdMap())q.addQueryItem(p.first,p.second.toString());q.addQueryItem("titles",pages.join('|'));wiki.setQuery(q);
            fetchJson(wiki,[best,save](QVariant response){QVariantList bestText;QString page;for(const auto &v:response.toMap().value("query").toMap().value("pages").toList()){auto p=v.toMap();const auto rev=p.value("revisions").toList();if(rev.isEmpty())continue;auto parsed=Metadata::wikipedia(rev.first().toMap().value("slots").toMap().value("main").toMap().value("content").toString());if(parsed.size()>bestText.size()){bestText=parsed;page=p.value("title").toString();}}bool usable=false;for(const auto &v:bestText)if(!v.toMap().value("title").toString().isEmpty()||!v.toMap().value("overview").toString().isEmpty())usable=true;save(usable?Metadata::overlayWikipedia(best->value("episodes").toList(),bestText):best->value("episodes").toList(),usable?"https://es.wikipedia.org/wiki/"+QString::fromLatin1(QUrl::toPercentEncoding(page.replace(' ','_'))):QString());});
        });}
    });
}

void Library::cacheImage(const QString &target,const QString &key,const QString &remote){
    if(!remote.startsWith('/')||remote.contains(".."))return;const auto size=key=="backdrop"?QString("w1280"):key.startsWith("season:")?QString("w780"):QString("w500");
    const auto name=QString::fromLatin1(QCryptographicHash::hash((size+remote).toUtf8(),QCryptographicHash::Sha256).toHex())+".jpg";QDir().mkpath(m_dataDir+"/art");const auto destination=m_dataDir+"/art/"+name;
    QString generationKey=target;if(target.startsWith("show:"))generationKey=target.mid(5);else if(target.startsWith("episode:"))generationKey=target.mid(8).section('|',0,0);
    QVariantMap update{{"generationKey",generationKey},{"generation",m_metadataGeneration.value(generationKey)},{"epoch",m_cacheEpoch},{"target",target},{"key",key},{"remote",remote},{"url",QUrl::fromLocalFile(destination).toString()}};
    if(QFileInfo::exists(destination)){m_pendingArt<<update;m_artTimer.start();return;}
    const bool loading=m_imageWaiters.contains(destination);m_imageWaiters[destination]<<update;if(loading)return;
    const int epoch=m_cacheEpoch;auto finished=[this,destination,epoch](const QByteArray &bytes){if(epoch!=m_cacheEpoch)return;const auto updates=m_imageWaiters.take(destination);if(!bytes.isEmpty()){QSaveFile out(destination);if(out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit())m_pendingArt<<updates;}m_artTimer.start();};
    const QUrl url("https://image.tmdb.org/t/p/"+size+remote);if(m_metadataTransport){m_metadataTransport(url,[finished](QVariant value){finished(value.toByteArray());});return;}
    QNetworkRequest request(url);request.setTransferTimeout(20000);auto reply=m_network.get(request);connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 size,qint64){if(size>10*1024*1024)reply->abort();});connect(reply,&QNetworkReply::finished,this,[reply,finished]{finished(reply->error()==QNetworkReply::NoError?reply->readAll():QByteArray());reply->deleteLater();});
}
void Library::flushArtwork(){
    if(m_pendingArt.isEmpty())return;if(!m_db.transaction())return;const auto pending=m_pendingArt;m_pendingArt.clear();bool ok=true;
    for(const auto &v:pending){auto item=v.toMap();if(item.value("epoch").toInt()!=m_cacheEpoch||item.value("generation").toInt()!=m_metadataGeneration.value(item.value("generationKey").toString()))continue;const auto target=item.value("target").toString(),field=item.value("key").toString();QString select,update;QVariantList args;
        if(target.startsWith("show:")){select="SELECT data FROM show_meta WHERE key=?";update="UPDATE show_meta SET data=? WHERE key=?";args<<target.mid(5);}
        else if(target.startsWith("episode:")){auto parts=target.mid(8).split('|');if(parts.size()!=3)continue;select="SELECT data FROM episode_meta WHERE showkey=? AND season=? AND episode=?";update="UPDATE episode_meta SET data=? WHERE showkey=? AND season=? AND episode=?";args<<parts[0]<<parts[1].toInt()<<parts[2].toInt();}
        else{select="SELECT data FROM metadata WHERE path=?";update="UPDATE metadata SET data=? WHERE path=?";args<<target;}
        auto rows=query(select,args);if(rows.isEmpty())continue;auto data=unpack(rows.first().toMap().value("data"));if((field=="poster"||field=="backdrop")&&data.contains(field+"PickRemote")&&data.value(field+"PickRemote")!=item.value("remote"))continue;
        if(field.startsWith("season:")){auto seasons=data.value("seasonPosters").toMap();seasons[field.mid(7)]=item.value("url");data["seasonPosters"]=seasons;}else data[field]=item.value("url");QVariantList values{pack(data)};values<<args;ok=exec(update,values)&&ok;
    }
    if(ok)m_db.commit();else m_db.rollback();reload();
}
void Library::findArtwork(const QString &path,bool backdrop){
    auto file=media(path);const int id=file.value("tmdbId").toInt();if(id<=0){fail("Descarga primero la ficha de TMDB.");return;}m_artwork.clear();emit artworkChanged();const auto endpoint=(file.value("kind")=="EPISODE"?QString("tv/"):QString("movie/"))+QString::number(id)+"/images";
    tmdb(endpoint,{{"include_image_language","es,en,null"}},[this,backdrop](QVariantMap response){for(const auto &v:response.value(backdrop?"backdrops":"posters").toList()){auto image=v.toMap();auto path=image.value("file_path").toString();if(!path.startsWith('/'))continue;m_artwork<<QVariantMap{{"remote",path},{"url","https://image.tmdb.org/t/p/w300"+path},{"language",image.value("iso_639_1")},{"width",image.value("width")},{"height",image.value("height")}};}emit artworkChanged();});
}
void Library::chooseArtwork(const QString &path,const QString &remote,bool backdrop){
    if(!remote.startsWith('/')||remote.contains(".."))return;const auto file=media(path);const bool series=file.value("kind")=="EPISODE";const auto target=series?Catalog::showKey(file):path;const auto rows=query(series?"SELECT data FROM show_meta WHERE key=?":"SELECT data FROM metadata WHERE path=?",{target});if(rows.isEmpty())return;auto data=unpack(rows.first().toMap().value("data"));const auto field=backdrop?QString("backdrop"):QString("poster");data[field+"PickRemote"]=remote;exec(series?"UPDATE show_meta SET data=? WHERE key=?":"UPDATE metadata SET data=? WHERE path=?",{pack(data),target});cacheImage(series?"show:"+target:target,field,remote);
}
qint64 Library::cacheSize()const{qint64 size=0;for(const auto &directory:{"art","thumbs"}){QDirIterator files(m_dataDir+'/'+directory,QDir::Files,QDirIterator::Subdirectories);while(files.hasNext()){files.next();size+=files.fileInfo().size();}}return size;}
void Library::clearMetadata(){exec("DELETE FROM metadata");exec("DELETE FROM show_meta");exec("DELETE FROM episode_meta");QDir(m_dataDir+"/art").removeRecursively();++m_cacheEpoch;for(auto it=m_metadataGeneration.begin();it!=m_metadataGeneration.end();++it)++it.value();++m_searchGeneration;m_searching=false;m_matches.clear();m_metadataJobs.clear();m_absolutePrompt.clear();m_artwork.clear();m_imageWaiters.clear();m_pendingArt.clear();m_artTimer.stop();QDir(m_dataDir+"/thumbs").removeRecursively();emit matchesChanged();emit metadataChanged();emit artworkChanged();reload();}
