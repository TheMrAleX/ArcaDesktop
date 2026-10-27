#include "Mpris.h"
#include "NativeVideo.h"
#include <QCoreApplication>
#include <QQuickWindow>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QUrl>
static QString trackId(const NativeVideo *video){return "/io/github/TheMrAleX/Arca/track/"+QString::fromLatin1(QCryptographicHash::hash(video->source().toEncoded(),QCryptographicHash::Sha256).toHex());}
void MprisRoot::Raise(){auto video=qobject_cast<NativeVideo*>(parent());if(video&&video->window()){video->window()->show();video->window()->raise();video->window()->requestActivate();}}
void MprisRoot::Quit(){QCoreApplication::quit();}
MprisPlayer::MprisPlayer(NativeVideo *video):QDBusAbstractAdaptor(video),m_video(video){
    connect(video,&NativeVideo::playbackStateChanged,this,[this]{notify({"PlaybackStatus","CanPlay","CanPause","CanSeek"});});
    connect(video,&NativeVideo::sourceChanged,this,[this]{notify({"Metadata"});});connect(video,&NativeVideo::durationChanged,this,[this]{notify({"Metadata"});});
    connect(video,&NativeVideo::volumeChanged,this,[this]{notify({"Volume"});});connect(video,&NativeVideo::playbackRateChanged,this,[this]{notify({"Rate"});});
    connect(video,&NativeVideo::mediaInfoChanged,this,[this]{notify({"Metadata","CanGoNext"});});
}
QString MprisPlayer::playbackStatus()const{return m_video->playbackState()==NativeVideo::PlayingState?"Playing":m_video->playbackState()==NativeVideo::PausedState?"Paused":"Stopped";}
double MprisPlayer::rate()const{return m_video->playbackRate();}void MprisPlayer::setRate(double value){if(value>0)m_video->setPlaybackRate(value);}
QVariantMap MprisPlayer::metadata()const{if(m_video->source().isEmpty())return {};return {{"mpris:trackid",QVariant::fromValue(QDBusObjectPath(trackId(m_video)))},{"mpris:length",m_video->duration()*1000},{"xesam:url",m_video->source().toString()},{"xesam:title",m_video->mediaTitle().isEmpty()?QFileInfo(m_video->source().toLocalFile()).completeBaseName():m_video->mediaTitle()}};}
double MprisPlayer::volume()const{return m_video->volume();}void MprisPlayer::setVolume(double value){m_video->setVolume(value);}qlonglong MprisPlayer::position()const{return m_video->position()*1000;}
bool MprisPlayer::canGoNext()const{return m_video->hasNext();}bool MprisPlayer::canPlay()const{return !m_video->source().isEmpty();}bool MprisPlayer::canPause()const{return m_video->mediaStatus()==NativeVideo::LoadedMedia||m_video->mediaStatus()==NativeVideo::BufferedMedia;}bool MprisPlayer::canSeek()const{return m_video->duration()>0;}
void MprisPlayer::Next(){if(canGoNext())emit m_video->nextRequested();}void MprisPlayer::Previous(){}
void MprisPlayer::Pause(){m_video->pause();}void MprisPlayer::Play(){m_video->play();}void MprisPlayer::PlayPause(){if(m_video->playbackState()==NativeVideo::PlayingState)Pause();else Play();}void MprisPlayer::Stop(){m_video->stop();}
void MprisPlayer::Seek(qlonglong offset){m_video->setPosition(qBound<qint64>(0,m_video->position()+offset/1000,m_video->duration()));emit Seeked(position());}
void MprisPlayer::SetPosition(const QDBusObjectPath &track,qlonglong at){if(track.path()!=trackId(m_video)||at<0||at/1000>m_video->duration())return;m_video->setPosition(at/1000);emit Seeked(at);}
void MprisPlayer::OpenUri(const QString &uri){Q_UNUSED(uri)} // Opening files belongs to the library, so progress keeps its identity.
void MprisPlayer::notify(const QStringList &properties){QVariantMap changed;for(const auto &name:properties)changed[name]=property(name.toLatin1().constData());auto message=QDBusMessage::createSignal("/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","PropertiesChanged");message<<QString("org.mpris.MediaPlayer2.Player")<<changed<<QStringList();QDBusConnection::sessionBus().send(message);}
Mpris::Mpris(NativeVideo *video):QObject(video){new MprisRoot(video);new MprisPlayer(video);auto bus=QDBusConnection::sessionBus();m_service="org.mpris.MediaPlayer2.arca.instance"+QString::number(QCoreApplication::applicationPid());if(bus.registerService(m_service))bus.registerObject("/org/mpris/MediaPlayer2",video,QDBusConnection::ExportAdaptors);else m_service.clear();
}
Mpris::~Mpris(){if(!m_service.isEmpty()){auto bus=QDBusConnection::sessionBus();bus.unregisterObject("/org/mpris/MediaPlayer2");bus.unregisterService(m_service);}}
