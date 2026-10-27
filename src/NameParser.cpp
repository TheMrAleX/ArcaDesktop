#include "NameParser.h"
#include <QDate>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace Names {
static QRegularExpression rx(const QString &s) {
    return QRegularExpression(s, QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
}
static const QString strong = R"(\d{3,4}[pi]|4k|8k|uhd|bluray|blu-ray|bdrip|brrip|bdremux|bd|web-dl|webdl|web dl|webrip|hdtv|hdrip|dvdrip|dvdr|dvd5|dvd9|remux|microhd|tvrip|x26[45]|h ?26[45]|h\.26[45]|hevc|xvid|divx|av1|avc|vp9|10bit|10 bit|8bit|hdr10\+?|hdr10plus|hdr|dovi|dolby vision|sdr|dts-hd|dts|e-?ac3|ac3|aac2?|ddp?5?|truehd|atmos|flac|opus|amzn|dsnp|hmax|atvp|nf)";
static const QString weak = R"(esp|spa|spanish|castellano|cast|lat|latino|vose|vos|dual|multi|subs|subbed|sub|eng|english|proper|repack|extended|internal|limited|unrated|uncut|directors cut|imax|remastered|complete|5\.1|7\.1|2\.0|5 1|7 1|2 0)";
static const auto technical = rx("(?<![\\p{L}\\p{N}])(" + strong + ")(?![\\p{L}\\p{N}])");
static const auto junk = rx("(?<![\\p{L}\\p{N}])(" + strong + "|" + weak + ")(?![\\p{L}\\p{N}])");
static const auto years = rx(R"((?<!\d)((?:19|20)\d{2})(?!\d))");
static const auto parenYear = rx(R"([(\[]((?:19|20)\d{2})[)\]])");
static const auto sxe = rx(R"((?<![\p{L}\p{N}])S(\d{1,2})[ ._-]?E(\d{1,3})(?:[ ._-]?(?:E|-E?)(\d{1,3}))?(?!\d))");
static const auto nxn = rx(R"((?<![\p{L}\p{N}])(\d{1,2})x(\d{2,3})(?:[-x](\d{2,3}))?(?!\d))");
static const auto spanish = rx(R"(Temporada\s*(\d{1,2}).{0,40}?(?:Cap(?:[ií]tulo)?|Episodio|Ep)\.?\s*(\d{1,3})(?!\d))");
static const auto spaced = rx(R"((?<![\p{L}\p{N}])(?:S|Season|Temp(?:orada)?\.?)\s*(\d{1,2})\s*(?:E|Ep(?:isod(?:e|io))?\.?|Cap(?:[ií]tulo)?\.?)\s*(\d{1,3})(?!\d))");
static const auto capCompact = rx(R"((?<![\p{L}])Cap(?:[ií]tulo)?\.?\s*(\d{3,4})(?!\d))");
static const auto sCompact = rx(R"((?<![\p{L}\p{N}])S(\d{3,4})(?!\d))");
static const auto bareCompact = rx(R"((?<![\p{L}\p{N}])(\d{3})(?![\p{L}\p{N}]))");
static const auto episodeOnly = rx(R"((?<![\p{L}\p{N}])(?:E|Ep|Episod(?:e|io)|Cap(?:[ií]tulo)?)\.?\s*(\d{1,3})(?!\d))");
static const auto onlyNumber = rx(R"(^\s*(\d{1,3})\s*$)");
static const auto leadingNumber = rx(R"(^\s*(\d{1,3})(?:\s*[-.]\s*|\s+)(.*)$)");
static const auto dashNumber = rx(R"(\s-\s(\d{1,3})(?:v\d)?(?:\s|$))");
static bool plausible(int year) { return year >= 1900 && year <= QDate::currentDate().year() + 1; }

QString key(QString value) {
    value = value.toLower().normalized(QString::NormalizationForm_D);
    static const auto marks=rx(R"(\p{M}+)"),symbols=rx("[^a-z0-9]+");
    value.remove(marks);
    return value.replace(symbols," ").trimmed();
}
QString normalize(QString value) {
    auto brackets = rx(R"(\[([^\]]*)\])");
    auto matches = brackets.globalMatch(value);
    QList<QRegularExpressionMatch> found;
    while (matches.hasNext()) found.prepend(matches.next());
    for (const auto &m : found) {
        const auto inner = m.captured(1);
        value.replace(m.capturedStart(), m.capturedLength(), rx(R"(cap|temporada|episodio|s\d{1,2}e\d|\d{1,2}x\d{2})").match(inner).hasMatch() ? " " + inner + " " : " ");
    }
    value.replace(rx(R"(\{[^}]*\})"), " ");
    value.replace(rx(R"((?<=\d)\.(?=\d(?!\d{2})))"), "#DOT#");
    value.replace('.', ' ').replace('_', ' ').replace("#DOT#", ".");
    return value.simplified();
}
QString clean(QString value) {
    value = normalize(value);
    value.remove(rx(R"((?<=\S)-[A-Za-z0-9]{2,12}$)"));
    const auto tech = technical.match(value);
    if (tech.hasMatch()) value.truncate(tech.capturedStart());
    value.replace(rx(R"(\(\s*\))"), " ");
    value = value.simplified();
    auto tail = rx("(?:[\\s\\-]+)(" + weak + ")$");
    for (auto m = tail.match(value); m.hasMatch() && m.capturedStart() > 0; m = tail.match(value))
        value = value.left(m.capturedStart()).trimmed();
    value.remove(rx(R"(^[ \-–—:,(+#.]+|[ \-–—:,(+#.]+$)"));
    if (value.endsWith(')') && !value.contains('(')) value.chop(1);
    return value.simplified();
}
bool isVideo(const QString &name) {
    static const QSet<QString> extensions = {"mkv","mp4","m4v","avi","mov","webm","ts","m2ts","mts","wmv","flv","mpg","mpeg","3gp","ogv","vob","divx","rmvb"};
    return extensions.contains(QFileInfo(name).suffix().toLower());
}
bool ignoredDir(const QString &name) {
    static const QSet<QString> ignored = {"extras","extra","featurettes","behind the scenes","behindthescenes","deleted scenes","trailers","trailer","samples","sample","muestras","interviews","shorts","bonus"};
    return name.startsWith('.') || ignored.contains(name.trimmed().toLower());
}
int seasonFromDir(const QString &value) {
    const auto name=value.trimmed();
    if (rx(R"(^(specials|especiales|season 0+|temporada 0+)$)").match(name.trimmed()).hasMatch()) return 0;
    auto m = rx(R"(^\s*(\d{1,2})\s*$)").match(name);
    if (!m.hasMatch()) m = rx(R"(^(?:season|temporada|temp|series|serie|staffel|saison|libro|book|parte|part|vol|volumen|volume|s|t)\.?\s*[._-]?\s*(\d{1,2})(?:\D.*)?$)").match(name);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}
static QPair<QString,int> titleYear(const QString &value) {
    const auto norm = normalize(value);
    auto matches = parenYear.globalMatch(value);
    QRegularExpressionMatch last;
    while (matches.hasNext()) { auto m = matches.next(); if (plausible(m.captured(1).toInt())) last = m; }
    if (last.hasMatch() && !clean(value.left(last.capturedStart())).isEmpty()) return {clean(value.left(last.capturedStart())),last.captured(1).toInt()};
    matches = years.globalMatch(norm);
    while (matches.hasNext()) { auto m = matches.next(); if (m.capturedStart() > 0 && plausible(m.captured(1).toInt())) last = m; }
    if (last.hasMatch() && !clean(norm.left(last.capturedStart())).isEmpty()) return {clean(norm.left(last.capturedStart())),last.captured(1).toInt()};
    return {clean(norm),0};
}
static QString mask(QString value) {
    for (const auto &pattern : {junk, years, rx(R"((?<=\S)-[A-Za-z0-9]{2,12}$)")}) {
        auto it = pattern.globalMatch(value);
        while (it.hasNext()) { auto m = it.next(); value.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), ' ')); }
    }
    return value;
}
int plainNumber(const QString &name) {
    const auto norm = normalize(QFileInfo(name).completeBaseName());
    for (const auto &p : {sxe,nxn,spaced,spanish,capCompact,sCompact}) if (p.match(norm).hasMatch()) return 0;
    const auto value = mask(norm).trimmed();
    for (const auto &p : {onlyNumber,dashNumber,leadingNumber}) { const auto m = p.match(value); if (m.hasMatch()) return m.captured(1).toInt(); }
    return 0;
}
int numberHint(const QString &name) {
    const auto norm = normalize(QFileInfo(name).completeBaseName());
    auto m = rx(R"((?<![\p{L}])(?:e|ep|cap|capitulo|capítulo|episodio|episode)\s*\.?\s*(\d{1,3})(?![\p{N}]))").match(norm);
    if (!m.hasMatch()) m = rx(R"((?<![\p{L}\p{N}])(\d{1,3})(?![\p{L}\p{N}]))").match(norm);
    return m.captured(1).toInt();
}
bool plainNumbering(const QList<int> &numbers) {
    QSet<int> distinct;
    for (int n : numbers) if (n > 0 && n <= 999) distinct.insert(n);
    if (distinct.size() < 3 || distinct.size()*4 < numbers.size()*3) return false;
    auto bounds = std::minmax_element(distinct.begin(), distinct.end());
    return *bounds.second - *bounds.first + 1 <= distinct.size()*2;
}
QMap<int,Result> plainNumberingFixes(const QList<QPair<QString,Result>> &files,const QString &directory,const QString &fallback) {
    QList<QPair<int,int>> candidates;QList<int> numbers;
    for(int i=0;i<files.size();++i){const auto &entry=files[i];const auto &p=entry.second;const bool bare=p.compactNumber&&p.compactToken==QString::number(p.compactNumber);bool open=p.kind==Kind::Unknown?!p.yearHint:p.kind==Kind::Episode?p.confidence<80||bare:p.kind==Kind::Movie?!p.year&&!p.yearHint&&onlyNumber.match(normalize(QFileInfo(entry.first).completeBaseName())).hasMatch():false;const int number=open?plainNumber(entry.first):0;if(number){candidates.append({i,number});numbers<<number;}}
    if(!plainNumbering(numbers))return {};
    QString title;int most=0;QHash<QString,int> titles;
    for(const auto &candidate:candidates){const auto &name=files[candidate.first].second.title;if(!name.isEmpty()&&!onlyNumber.match(name.trimmed()).hasMatch()){int count=++titles[name];if(count>most){most=count;title=name;}}}
    if(title.isEmpty())title=titleYear(directory.isNull()?fallback:directory).first;
    if(title.trimmed().isEmpty()||onlyNumber.match(title.trimmed()).hasMatch())return {};
    QMap<int,Result> output;const int season=seasonFromDir(directory);
    for(const auto &candidate:candidates){auto p=files[candidate.first].second;p.kind=Kind::Episode;p.title=title;p.season=season>=0?season:1;p.episode=candidate.second;p.episodeEnd=0;p.confidence=86;p.reason.clear();p.compactToken.clear();p.compactNumber=0;p.altSeason=-1;p.altEpisode=0;p.seasonFromFolder=season>=0;output[candidate.first]=p;}return output;
}
QString resolveCompactGroup(const QList<int> &numbers) {
    QSet<int> unique(numbers.begin(), numbers.end()), see, sse;
    if (unique.size() < 2) return {};
    for (int n : unique) { if (n%10 == 0) return "SEE"; see.insert(n/100); sse.insert(n/10); }
    return sse.size() < see.size() ? "SSE" : "SEE";
}
QMap<QPair<QString,int>,Renumber> absoluteOffsets(const QList<EpisodeRef> &episodes) {
    QMap<QString,QMap<int,QList<EpisodeRef>>> shows;
    for(const auto &episode:episodes)shows[episode.show][episode.season].append(episode);
    QMap<QPair<QString,int>,Renumber> output;
    for(auto show=shows.cbegin();show!=shows.cend();++show) {
        const auto &seasons=show.value();
        for(auto season=seasons.cbegin();season!=seasons.cend();++season) {
            if(season.key()<2)continue;
            QSet<int> unique;
            bool fromFolder=true;
            for(const auto &episode:season.value()){unique.insert(episode.episode);fromFolder=fromFolder&&episode.seasonFromFolder;}
            if(unique.isEmpty())continue;
            auto bounds=std::minmax_element(unique.cbegin(),unique.cend());
            const int first=*bounds.first,last=*bounds.second;
            if(first<=1)continue;
            if(seasons.contains(season.key()-1)) {
                int previous=0;for(const auto &episode:seasons[season.key()-1])previous=qMax(previous,episode.episode);
                if(first==previous+1)output.insert({show.key(),season.key()},{previous,true});
            } else if(fromFolder&&unique.size()>=3&&last-first+1<=unique.size()*3/2+1)
                output.insert({show.key(),season.key()},{first-1,false});
        }
    }
    return output;
}
Result parse(const QString &name, const QStringList &dirs, const QString &kind, const QString &root, const QString &rule, bool ignoreSamples) {
    Result r;
    if (name.startsWith('.') || (ignoreSamples && rx(R"((?<![a-z])(sample|muestra)(?![a-z]))").match(name).hasMatch())) {
        r.kind = Kind::Ignored; r.reason = "Ignorado por la regla de muestras"; return r;
    }
    for (const auto &dir : dirs) if (ignoreSamples && ignoredDir(dir)) { r.kind = Kind::Ignored; r.reason = "Ignorado: carpeta de extras"; return r; }
    const auto base = QFileInfo(name).completeBaseName();
    const auto norm = normalize(base);
    int dirSeason = -1, seasonIndex = -1;
    for (int i=0; i<dirs.size(); ++i) if (seasonFromDir(dirs[i]) >= 0) { seasonIndex=i; dirSeason=seasonFromDir(dirs[i]); }
    const auto showDir = seasonIndex > 0 ? dirs[seasonIndex-1] : seasonIndex == 0 ? root : dirs.isEmpty() ? QString() : dirs.last();
    auto show = titleYear(showDir);
    auto hint = years.globalMatch(norm);
    while (hint.hasNext()) { int y=hint.next().captured(1).toInt(); if (plausible(y)) r.yearHint=y; }
    const auto parenthesizedHint=parenYear.match(base);if(parenthesizedHint.hasMatch())r.yearHint=parenthesizedHint.captured(1).toInt();
    auto movie = [&]() {
        auto t = titleYear(base); r.kind=Kind::Movie; r.title=t.first; r.year=t.second; r.confidence=t.second ? (parenYear.match(base).hasMatch()?96:94) : 82;
        if (!r.year && (r.title.isEmpty() || QSet<QString>{"movie","pelicula","video","film"}.contains(key(r.title)) || onlyNumber.match(r.title).hasMatch()) && !dirs.isEmpty()) {
            auto d=titleYear(dirs.last()); r.title=d.first; r.year=d.second; r.confidence=d.second?90:80;
        } else if (!r.year && !dirs.isEmpty()) { auto d=titleYear(dirs.last()); if (d.second && key(d.first)==key(r.title)) {r.title=d.first;r.year=d.second;r.confidence=92;} }
        if (r.title.isEmpty()) {r.kind=Kind::Unknown;r.confidence=0;r.reason="El nombre no dice nada";}
        r.yearHint=r.year;
        if (kind != "MOVIES") {r.confidence=r.year ? qMin(r.confidence,88):60; if (!r.year) r.reason="No sé si es una película o un episodio";}
        return r;
    };
    if (kind=="MOVIES") return movie();
    int start=-1,end=0;
    for (const auto &pattern : {sxe,nxn,spanish,spaced}) {
        auto m=pattern.match(norm);
        if (m.hasMatch()) {start=m.capturedStart();end=m.capturedEnd();r.season=m.captured(1).toInt();r.episode=m.captured(2).toInt();r.episodeEnd=m.captured(3).toInt()>r.episode?m.captured(3).toInt():0;r.confidence=pattern.pattern()==sxe.pattern()?98:pattern.pattern()==spaced.pattern()?93:95;break;}
    }
    auto compact = [&](const QRegularExpressionMatch &m, const QString &prefix, int confidence) {
        int n=m.captured(1).toInt();
        if (n%100==0) return false;
        start=m.capturedStart();end=m.capturedEnd();r.compactNumber=n;r.compactToken=prefix+m.captured(1);r.season=n/100;r.episode=n%100;r.confidence=confidence;
        if (m.captured(1).size()==4) r.confidence=qMax(confidence,85);
        else if (!rule.isEmpty()) {if(rule=="SSE"){r.season=n/10;r.episode=n%10;}r.confidence=100;}
        else if(dirSeason==r.season) r.confidence=92;
        else if(n%10==0) r.confidence=qMax(confidence,80);
        else {r.altSeason=n/10;r.altEpisode=n%10;r.reason=QString("«%1» puede ser T%2·E%3 o T%4·E%5").arg(r.compactToken).arg(r.season).arg(r.episode,2,10,QChar('0')).arg(r.altSeason).arg(r.altEpisode);}
        return true;
    };
    const auto masked=mask(norm);
    if(start<0) {auto m=capCompact.match(masked);if(m.hasMatch())compact(m,"Cap.",70);}
    if(start<0) {auto m=sCompact.match(masked);if(m.hasMatch())compact(m,"S",62);}
    if(start<0) {auto m=episodeOnly.match(masked);if(m.hasMatch()){start=m.capturedStart();end=m.capturedEnd();r.season=dirSeason;r.episode=m.captured(1).toInt();r.confidence=dirSeason>=0?90:40;r.seasonFromFolder=dirSeason>=0;}}
    if(start<0 && dirSeason>=0) for(const auto &p:{onlyNumber,leadingNumber,dashNumber}) {auto m=p.match(masked);if(m.hasMatch()){start=m.capturedStart();end=m.capturedEnd(1);r.season=dirSeason;r.episode=m.captured(1).toInt();r.confidence=p.pattern()==onlyNumber.pattern()?85:p.pattern()==leadingNumber.pattern()?80:82;r.seasonFromFolder=true;break;}}
    if(start<0){auto m=bareCompact.match(masked);if(m.hasMatch())compact(m,"",55);}
    if(start<0){auto m=dashNumber.match(masked);if(m.hasMatch()){start=m.capturedStart();end=m.capturedEnd(1);r.season=1;r.episode=m.captured(1).toInt();r.confidence=60;r.reason="Parece un episodio; asumo temporada 1";}}
    if(start>=0){
        auto fileTitle=clean(norm.left(start));auto withoutYear=fileTitle;withoutYear.remove(rx(R"(\s*\(?((?:19|20)\d{2})\)?$)"));if(!withoutYear.trimmed().isEmpty())fileTitle=withoutYear.trimmed();
        bool useDir=!show.first.isEmpty()&&(fileTitle.isEmpty()||seasonIndex>=0||kind=="SERIES"||key(show.first).startsWith(key(fileTitle))||key(fileTitle).startsWith(key(show.first)));
        r.title=useDir?show.first:fileTitle;r.year=useDir?show.second:0;
        if(r.title.isEmpty())r.title=titleYear(root).first;
        r.episodeTitle=clean(norm.mid(end));
        if(r.episodeTitle.size()<2||!r.episodeTitle.contains(rx("\\p{L}"))||rx("^("+weak+")$").match(r.episodeTitle).hasMatch())r.episodeTitle.clear();
        r.kind=r.season>=0?Kind::Episode:Kind::Unknown;
        if(r.season<0){r.year=0;r.episodeTitle.clear();r.confidence=30;r.reason=QString("Veo el episodio %1 pero no la temporada").arg(r.episode);}
        if(r.title.isEmpty()){r.confidence=qMin(r.confidence,40);r.reason="No sé de qué serie es";}
        return r;
    }
    if(kind=="SERIES"){r.title=clean(norm);auto without=r.title;without.remove(rx(R"(\s*\(?((?:19|20)\d{2})\)?$)"));if(!without.trimmed().isEmpty())r.title=without.trimmed();r.reason=r.yearHint?QString("Sin coincidencia · parece una película de %1").arg(r.yearHint):"Sin temporada ni episodio en el nombre";return r;}
    return movie();
}
}
