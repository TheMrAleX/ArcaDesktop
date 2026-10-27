#include "NameParser.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
class ParserParityTest:public QObject {
    Q_OBJECT
private slots:
    void android_data(){QTest::addColumn<QVariantMap>("input");QFile file(QString(ARCA_SOURCE_DIR)+"/tests/fixtures/parser/android-golden.json");QVERIFY(file.open(QIODevice::ReadOnly));int index=0;for(const auto &v:QJsonDocument::fromJson(file.readAll()).toVariant().toList())QTest::newRow(qPrintable(QString::number(index++)))<<v.toMap();}
    void android(){QFETCH(QVariantMap,input);auto name=input.value("name").toString();auto r=Names::parse(name,input.value("dirs").toStringList(),input.value("kind").toString(),input.value("root").toString(),input.value("rule").toString());QVariantMap actual{{"kind",r.kind==Names::Kind::Movie?"MOVIE":r.kind==Names::Kind::Episode?"EPISODE":r.kind==Names::Kind::Ignored?"IGNORED":"UNKNOWN"},{"title",r.title},{"year",r.year},{"season",r.season},{"episode",r.episode},{"episodeEnd",r.episodeEnd},{"episodeTitle",r.episodeTitle},{"confidence",r.confidence},{"compactToken",r.compactToken},{"compactNumber",r.compactNumber},{"altSeason",r.altSeason},{"altEpisode",r.altEpisode},{"yearHint",r.yearHint},{"seasonFromFolder",r.seasonFromFolder},{"plainNumber",Names::plainNumber(name)},{"numberHint",Names::numberHint(name)}};const auto expected=input.value("expected").toMap();if(actual!=expected){qWarning().noquote()<<QJsonDocument::fromVariant(input).toJson(QJsonDocument::Compact);qWarning().noquote()<<QJsonDocument::fromVariant(actual).toJson(QJsonDocument::Compact);}QCOMPARE(actual,expected);}
};
QTEST_GUILESS_MAIN(ParserParityTest)
#include "ParserParityTest.moc"
