#include "Library.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QDebug>

int main(int argc,char **argv){
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    // The FFmpeg backend in Qt 6.4 is a technology preview. Use its stable
    // GStreamer backend on Ubuntu 24.04 / Mint 22 unless explicitly overridden.
    if(qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND"))qputenv("QT_MEDIA_BACKEND","gstreamer");
#endif
    QGuiApplication app(argc,argv);
    app.setOrganizationName("TheMrAleX");app.setApplicationName("Arca");app.setApplicationVersion(ARCA_VERSION);
    QQuickStyle::setStyle("Basic");
    QFontDatabase::addApplicationFont(":/assets/manrope.ttf");
    QFontDatabase::addApplicationFont(":/assets/bricolage.ttf");
    app.setFont(QFont("Manrope",11));
    const bool smoke=app.arguments().contains("--smoke-test");
    QTemporaryDir isolated;
    Library library(smoke?isolated.path():QString());
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("library",&library);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreated,&app,[&](QObject *object,const QUrl &){if(!object)app.exit(1);},Qt::QueuedConnection);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(smoke){
        auto issues=std::make_shared<QStringList>();
        QObject::connect(&engine,&QQmlApplicationEngine::warnings,&app,[issues](const QList<QQmlError> &warnings){for(const auto &warning:warnings)issues->append(warning.toString());});
        if(engine.rootObjects().isEmpty())return 1;
        const int index=app.arguments().indexOf("--smoke-test");
        if(index+1>=app.arguments().size())return 2;
        library.setSetting("scanOnOpen",false);library.addFolder(QUrl::fromLocalFile(app.arguments()[index+1]),"AUTO");
        auto step=std::make_shared<int>(0);auto ticks=std::make_shared<int>(0);
        auto timer=new QTimer(&app);timer->setInterval(250);
        QObject::connect(timer,&QTimer::timeout,&app,[&,step,ticks,issues]{
            if(++*ticks>160){qCritical()<<"SMOKE: tiempo agotado";app.exit(1);return;}
            if(library.scanning())return;
            if(library.files().isEmpty()){qCritical()<<"SMOKE: biblioteca vacía";app.exit(1);return;}
            auto window=engine.rootObjects().first();
            const QStringList pages{"Bienvenida","Inicio","Biblioteca","Buscar","Carpetas","Revisar","Perfiles","Ajustes","Detalles","Archivos"};
            if(*step<pages.size()){
                window->setProperty("selected",pages[*step]=="Detalles"&&!library.shows().isEmpty()?library.shows().first():library.files().first());
                window->setProperty("page",pages[*step]);++*step;return;
            }
            if(*step==pages.size()){
                auto media=library.files().first().toMap();media["startPosition"]=0;
                window->setProperty("playing",media);window->setProperty("page","Reproductor");++*step;return;
            }
            if(++*step<pages.size()+16)return;
            auto player=window->findChild<QMediaPlayer*>("mediaPlayer");
            const bool decoded=player&&player->error()==QMediaPlayer::NoError&&player->position()>=100&&player->videoSink()&&!player->videoSink()->videoSize().isEmpty();
            if(issues->isEmpty()&&!decoded&&*step<pages.size()+48)return;
            if(!issues->isEmpty()||!decoded){
                qCritical()<<"SMOKE: fallo"<<*issues<<(player?player->errorString():"sin reproductor");
                if(player)qCritical()<<"Estado:"<<player->mediaStatus()<<"posición:"<<player->position()<<"duración:"<<player->duration()<<"vídeo:"<<(player->videoSink()?player->videoSink()->videoSize():QSize());
                app.exit(1);return;
            }
            qInfo()<<"SMOKE OK: 10 pantallas y vídeo decodificado";app.exit(0);
        });timer->start();
    }
    return app.exec();
}
