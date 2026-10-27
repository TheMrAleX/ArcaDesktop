#include "Metadata.h"
#include "NameParser.h"
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

QVariantList Metadata::ranges(const QVariantList &counts){
    QMap<int,int> seasons;for(const auto &v:counts){auto c=v.toMap();const int season=c.value("season",c.value("season_number")).toInt(),count=c.value("count",c.value("episode_count")).toInt();if(season>0&&count>0)seasons[season]=count;}
    QVariantList result;int n=0;for(auto it=seasons.cbegin();it!=seasons.cend();++it){result<<QVariantMap{{"season",it.key()},{"first",n+1},{"last",n+it.value()},{"count",it.value()}};n+=it.value();}return result;
}
QVariantMap Metadata::absoluteEpisode(int number,const QVariantList &ranges){for(const auto &v:ranges){const auto r=v.toMap();if(number>=r.value("first").toInt()&&number<=r.value("last").toInt())return {{"season",r.value("season")},{"episode",number-r.value("first").toInt()+1}};}return {};}
bool Metadata::looksAbsolute(const QVariantList &files,const QVariantList &ranges){
    if(ranges.size()<2||files.isEmpty())return false;QMap<int,int> counts;for(const auto &v:ranges){auto r=v.toMap();counts[r.value("season").toInt()]=r.value("count").toInt();}
    bool beyond=false;const int total=ranges.last().toMap().value("last").toInt();
    for(const auto &v:files){auto f=v.toMap();const int s=f.value("season").toInt(),n=f.value("episode").toInt();if(n<1||n>total)return false;if(!counts.contains(s)||n>counts.value(s))beyond=true;}return beyond;
}
static QMap<int,int> countsOf(const QVariantList &counts){QMap<int,int> out;for(const auto &v:counts){auto c=v.toMap();out[c.value("season",c.value("season_number")).toInt()]=c.value("count",c.value("episode_count")).toInt();}return out;}
bool Metadata::covers(const QVariantList &files,const QVariantList &counts){auto available=countsOf(counts);for(const auto &v:files){auto f=v.toMap();if(available.value(f.value("season").toInt())<f.value("episode").toInt())return false;}return true;}
int Metadata::structureScore(const QVariantList &files,const QVariantList &counts){auto available=countsOf(counts);QMap<int,int> have;for(const auto &v:files){auto f=v.toMap();int s=f.value("season").toInt();have[s]=qMax(have.value(s),f.value("episode").toInt());}int score=0;for(auto it=have.cbegin();it!=have.cend();++it)score+=!available.contains(it.key())?-6:available.value(it.key())<it.value()?-3:4;return score;}
QString Metadata::plainHtml(QString text){text.replace(QRegularExpression("<[^>]+>",QRegularExpression::DotMatchesEverythingOption)," ");text.replace("&amp;","&").replace("&quot;","\"").replace("&#39;","'").replace("&nbsp;"," ");return text.simplified();}
static QString cleanWiki(QString text){
    text.remove(QRegularExpression("<!--.*?-->",QRegularExpression::DotMatchesEverythingOption));text.remove(QRegularExpression("<ref\\b.*?</ref>|<ref[^>]*/>",QRegularExpression::CaseInsensitiveOption|QRegularExpression::DotMatchesEverythingOption));
    const QRegularExpression nested("\\{\\{[^{}|]+\\|([^{}]*)\\}\\}");
    for(int n=0;n<4;++n){auto matches=nested.globalMatch(text);QList<QRegularExpressionMatch> found;while(matches.hasNext())found.prepend(matches.next());for(const auto &m:found)text.replace(m.capturedStart(),m.capturedLength(),m.captured(1).section('|',-1));}
    text.replace(QRegularExpression("\\[\\[[^\\]|]+\\|([^\\]]+)\\]\\]"),"\\1");text.replace(QRegularExpression("\\[\\[([^\\]]+)\\]\\]"),"\\1");text.replace("'''","").replace("''","");return Metadata::plainHtml(text);
}
QVariantList Metadata::wikipedia(const QString &text){
    const QRegularExpression start("\\{\\{\\s*Lista de episodios\\b",QRegularExpression::CaseInsensitiveOption);
    QVariantList result;int from=0;
    while(true){auto m=start.match(text,from);if(!m.hasMatch())break;int depth=0,i=m.capturedStart();bool complete=false;
        while(i<text.size()-1){auto pair=text.mid(i,2);if(pair=="{{"){++depth;i+=2;continue;}if(pair=="}}"){--depth;i+=2;if(depth==0){complete=true;break;}continue;}++i;}
        from=qMax(i,int(m.capturedEnd()));if(!complete)continue;
        const auto block=text.mid(m.capturedStart(),i-m.capturedStart());
        const QRegularExpression marker("^\\s*\\|\\s*([^=\\n]+?)\\s*=",QRegularExpression::MultilineOption);
        auto matches=marker.globalMatch(block);QList<QRegularExpressionMatch> marks;while(matches.hasNext())marks<<matches.next();QVariantMap fields;
        for(int p=0;p<marks.size();++p){const auto &mark=marks[p];auto key=Names::key(mark.captured(1));key.remove(' ');int end=p+1<marks.size()?marks[p+1].capturedStart():block.size()-2;fields[key]=cleanWiki(block.mid(mark.capturedEnd(),end-mark.capturedEnd()).trimmed());}
        result<<QVariantMap{{"title",fields.value("titulo")},{"overview",fields.value("resumenbreve")}};
    }return result;
}
QVariantList Metadata::tvmaze(const QVariantList &episodes){QVariantList result;for(int i=0;i<episodes.size();++i){auto ep=episodes[i].toMap();if(!ep.contains("season")||ep.value("number").isNull())continue;result<<QVariantMap{{"id",ep.value("id")},{"season",ep.value("season")},{"episode",ep.value("number")},{"absolute",i+1},{"title",ep.value("name")},{"overview",plainHtml(ep.value("summary").toString())},{"airDate",ep.value("airdate")},{"runtime",ep.value("runtime")}};}return result;}
QVariantList Metadata::overlayWikipedia(QVariantList episodes,const QVariantList &text){for(auto &v:episodes){auto ep=v.toMap();const int i=ep.value("absolute").toInt()-1;if(i>=0&&i<text.size()){auto overlay=text[i].toMap();for(const auto &key:{"title","overview"})if(!overlay.value(key).toString().trimmed().isEmpty())ep[key]=overlay.value(key);v=ep;}}return episodes;}
QVariantList Metadata::candidates(const QVariantList &hits,const QString &title,int year){
    const auto key=Names::key(title);QVariantList exact,contained;
    for(const auto &v:hits){auto hit=v.toMap();const auto a=Names::key(hit.value("title",hit.value("name")).toString()),b=Names::key(hit.value("original_title",hit.value("original_name")).toString());if(a==key||b==key)exact<<hit;else if(key.size()>=4&&(a.contains(key)||key.contains(a)||(!b.isEmpty()&&(b.contains(key)||key.contains(b)))))contained<<hit;}
    auto pool=exact.isEmpty()?contained:exact;if(year>0){QVariantList matching;for(const auto &v:pool){auto hit=v.toMap();int y=hit.value("year",hit.value("release_date",hit.value("first_air_date")).toString().left(4)).toInt();if(y>0&&qAbs(year-y)<=1)matching<<hit;}if(!matching.isEmpty())pool=matching;}return pool;
}
