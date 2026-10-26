#include "TrackChoice.h"
#include <QLocale>
#include <QRegularExpression>
#include <QHash>

QString TrackChoice::language(const QVariant &value) {
    QString code;
    if(value.metaType().id()==QMetaType::Int||value.metaType().id()==QMetaType::UInt||value.metaType().id()==QMetaType::Double||value.metaType().id()==QMetaType::Float||value.metaType().flags().testFlag(QMetaType::IsEnumeration)) {
        if(value.toInt()<=int(QLocale::C))return {};
        code=QLocale(QLocale::Language(value.toInt())).bcp47Name();
    } else code=value.toString().trimmed().toLower();
    if(code.isEmpty()||code=="und"||code=="unknown")return {};
    static const QHash<QString,QString> aliases{{"spa","es"},{"esp","es"},{"cast","es"},{"castellano","es"},{"spanish","es"},{"español","es"},{"lat","es-419"},{"latino","es-419"},{"es-mx","es-419"},{"eng","en"},{"english","en"},{"fra","fr"},{"fre","fr"},{"deu","de"},{"ger","de"},{"ita","it"},{"por","pt"},{"jpn","ja"},{"kor","ko"},{"zho","zh"},{"chi","zh"},{"rus","ru"},{"cat","ca"},{"eus","eu"},{"baq","eu"},{"glg","gl"},{"nld","nl"},{"dut","nl"},{"pol","pl"},{"swe","sv"},{"tur","tr"},{"ara","ar"}};
    if(aliases.contains(code))return aliases.value(code);
    if(code=="es-419")return code;
    if(code=="pt-br")return "pt-BR";
    const QLocale locale(code);
    if(locale.language()==QLocale::C)return {};
    return code.section('-',0,0);
}
QString TrackChoice::languageName(const QVariant &value) {
    const auto code=language(value);
    if(code.isEmpty())return "Idioma desconocido";
    if(code=="es-419")return "Español latino";
    auto label=QLocale(code).nativeLanguageName();
    if(!label.isEmpty())label[0]=label[0].toUpper();
    return label;
}
QString TrackChoice::normalizeId(QString id) { return id.remove(QRegularExpression("^\\d+:")); }
QString TrackChoice::encode(const QVariantMap &track) {
    const auto lang=language(track.value("lang"));
    return (lang.isEmpty()?QString("und"):lang)+'|'+(track.value("forced").toBool()?QString("f"):QString())+'|'+normalizeId(track.value("id").toString());
}
int TrackChoice::pick(const QVariantList &options,const QString &saved) {
    if(saved.trimmed().isEmpty()||saved=="off")return -1;
    const auto parts=saved.split('|');
    const auto lang=language(parts.value(0)),id=normalizeId(parts.mid(2).join('|'));
    if(!id.isEmpty())for(int i=0;i<options.size();++i){const auto track=options[i].toMap();if(normalizeId(track.value("id").toString())==id&&language(track.value("lang"))==lang)return i;}
    const bool forced=parts.value(1)=="f";
    for(int i=0;i<options.size();++i){const auto track=options[i].toMap();if(language(track.value("lang"))==lang&&track.value("forced").toBool()==forced)return i;}
    return -1;
}
int TrackChoice::preferred(const QVariantList &options,const QStringList &languages,bool forcedOnly) {
    for(const auto &code:languages){const auto wanted=language(code);if(wanted.isEmpty())continue;
        for(int i=0;i<options.size();++i){const auto track=options[i].toMap();if(forcedOnly&&!track.value("forced").toBool())continue;
            if(language(track.value("lang")).section('-',0,0)==wanted.section('-',0,0))return i;
        }
    }return -1;
}
