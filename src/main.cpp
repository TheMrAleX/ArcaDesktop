#include "Library.h"
#include "NativeVideo.h"
#include "ThumbnailProvider.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QSGRendererInterface>
#include <QDebug>
#include <QDir>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    qmlRegisterType<NativeVideo>("Arca.Native",1,0,"NativeVideo");
    app.setOrganizationName("TheMrAleX");app.setApplicationName("Arca");app.setApplicationVersion(ARCA_VERSION);
    QQuickStyle::setStyle("Basic");
    QFontDatabase::addApplicationFont(":/assets/manrope.ttf");
    QFontDatabase::addApplicationFont(":/assets/bricolage.ttf");
    app.setFont(QFont("Manrope",11));
    const bool smoke=app.arguments().contains("--smoke-test");
    QTemporaryDir isolated;
    Library library(smoke?isolated.path():QString());
    QQmlApplicationEngine engine;
    auto thumbnails=new ThumbnailProvider(library.thumbnailDirectory());thumbnails->setFiles(library.files());engine.addImageProvider("thumbs",thumbnails);
    QObject::connect(&library,&Library::filesPrepared,&engine,[thumbnails,&library]{thumbnails->setFiles(library.files());});
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
        auto phase=std::make_shared<int>(0);auto stageTicks=std::make_shared<int>(0);auto busDone=std::make_shared<bool>(false);auto step=std::make_shared<int>(0);auto ticks=std::make_shared<int>(0);
        auto timer=new QTimer(&app);timer->setInterval(250);
        QObject::connect(timer,&QTimer::timeout,&app,[&,step,ticks,issues,phase,stageTicks,busDone]{
            if(++*ticks>160){qCritical()<<"SMOKE: tiempo agotado";app.exit(1);return;}
            if(library.scanning())return;
            if(library.files().isEmpty()){qCritical()<<"SMOKE: biblioteca vacía";app.exit(1);return;}
            auto window=engine.rootObjects().first();
            const QStringList pages{"Bienvenida","Inicio","Biblioteca","Buscar","Carpetas","Revisar","Perfiles","Ajustes","Detalles","Archivos"};
            const auto screenshotDir=qEnvironmentVariable("ARCA_SCREENSHOTS");
            if(!screenshotDir.isEmpty()&&*step>0&&*step<=pages.size()){QDir().mkpath(screenshotDir);qobject_cast<QQuickWindow*>(window)->grabWindow().save(screenshotDir+'/'+pages[*step-1]+".png");}
            if(*step<pages.size()){
                window->setProperty("selected",pages[*step]=="Detalles"&&!library.shows().isEmpty()?library.shows().first():library.files().first());
                window->setProperty("page",pages[*step]);++*step;return;
            }
            if(*step==pages.size()){
                auto media=library.files().first().toMap();qInfo()<<"SMOKE: sonda"<<media.value("duration")<<media.value("width");media["startPosition"]=0;
                window->setProperty("playing",media);window->setProperty("page","Reproductor");++*step;return;
            }
            if(++*step<pages.size()+16)return;
            auto player=window->findChild<NativeVideo*>("mediaPlayer");
            const bool decoded=player&&player->error()==NativeVideo::NoError&&player->position()>=100&&player->renderedFrames()>0&&!player->videoSize().isEmpty();
            if(*phase==0&&issues->isEmpty()&&!decoded&&*step<pages.size()+48)return;
            if(!issues->isEmpty()||(*phase==0&&!decoded)){
                qCritical()<<"SMOKE: fallo"<<*issues<<(player?player->errorString():"sin reproductor");
                if(player)qCritical()<<"Estado:"<<player->mediaStatus()<<"posición:"<<player->position()<<"duración:"<<player->duration()<<"vídeo:"<<player->videoSize()<<"fotogramas:"<<player->renderedFrames();
                app.exit(1);return;
            }
            auto check=[&](bool condition,const QString &description){if(!condition){qCritical()<<"SMOKE:"<<description;app.exit(1);}return condition;};
            if(*phase==0){
                if(!check(player->audioTracks().size()==2&&player->subtitleTracks().size()==1,"Faltan las pistas de audio o ASS"))return;
                if(!check(player->activeAudioTrack()==1&&player->activeSubtitleTrack()==0,"No se aplicaron las preferencias de idioma y forzados"))return;
                const auto frame=qobject_cast<QQuickWindow*>(window)->grabWindow();int colors=0;for(int y=2;y<8;++y)for(int x=1;x<10;++x){const auto pixel=frame.pixelColor(frame.width()*x/10,frame.height()*y/10);if(pixel.saturation()>150&&pixel.value()>120)++colors;}
                if(colors<=12){if(++*stageTicks<8)return;check(false,"La superficie no contiene la imagen de vídeo");return;}
                if(!screenshotDir.isEmpty())frame.save(screenshotDir+"/Reproductor.png");
                auto message=QDBusMessage::createMethodCall("org.mpris.MediaPlayer2.arca.instance"+QString::number(app.applicationPid()),"/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player","Pause");
                auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),&app);
                QObject::connect(watcher,&QDBusPendingCallWatcher::finished,&app,[busDone,issues,watcher]{QDBusPendingReply<> reply=*watcher;if(reply.isError())issues->append(reply.error().message());*busDone=true;watcher->deleteLater();});
                ++*phase;*stageTicks=0;return;
            }
            if(*phase==1){
                if(!*busDone){if(++*stageTicks>20)check(false,"MPRIS no responde");return;}
                if(!check(player->playbackState()==NativeVideo::PausedState,"MPRIS no pausa el vídeo"))return;
                player->setPosition(8000);player->setPlaybackRate(1.5);player->setSubtitleDelay(0.3);player->setSubtitleScale(1.3);player->setActiveAudioTrack(0);player->play();++*phase;*stageTicks=0;return;
            }
            if(*phase==2){
                if(++*stageTicks<4)return;
                if(!check(player->position()>=8000&&player->playbackRate()==1.5&&qAbs(player->subtitleDelay()-0.3)<0.01&&player->activeAudioTrack()==0,"Fallo de búsqueda, velocidad, retardo o selección de audio"))return;
                player->stop();++*phase;return;
            }
            if(*phase==3){if(!check(player->playbackState()==NativeVideo::StoppedState,"Stop no detiene el vídeo"))return;player->play();++*phase;*stageTicks=0;return;}
            if(*phase==4){if(++*stageTicks<4)return;if(!check(player->playbackState()==NativeVideo::PlayingState&&player->position()>0,"No se puede reproducir después de detener"))return;}
            qInfo()<<"SMOKE OK: 10 pantallas, vídeo y ASS, idiomas, MPRIS, búsqueda y controles";app.exit(0);
        });timer->start();
    }
    return app.exec();
}
