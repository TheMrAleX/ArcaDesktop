#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>
class NativeVideo;
class MprisRoot:public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)
public:
    explicit MprisRoot(QObject *parent):QDBusAbstractAdaptor(parent){}
    bool canQuit()const{return true;} bool canRaise()const{return true;} bool hasTrackList()const{return false;}
    QString identity()const{return "Arca";} QString desktopEntry()const{return "io.github.TheMrAleX.Arca";}
    QStringList supportedUriSchemes()const{return {"file"};} QStringList supportedMimeTypes()const{return {"video/mp4","video/x-matroska","video/webm","video/x-msvideo"};}
public slots:
    void Raise(); void Quit();
};
class MprisPlayer:public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
public:
    explicit MprisPlayer(NativeVideo *video);
    QString playbackStatus()const;double rate()const;void setRate(double);
    QVariantMap metadata()const;double volume()const;void setVolume(double);qlonglong position()const;
    double minimumRate()const{return 0.25;}double maximumRate()const{return 4;}
    bool canGoNext()const;bool canGoPrevious()const{return false;}bool canPlay()const;bool canPause()const;bool canSeek()const;bool canControl()const{return true;}
public slots:
    void Next();void Previous();void Pause();void PlayPause();void Stop();void Play();void Seek(qlonglong offset);void SetPosition(const QDBusObjectPath &track,qlonglong position);void OpenUri(const QString &uri);
signals:
    void Seeked(qlonglong position);
private:
    void notify(const QStringList &properties);
    NativeVideo *m_video;
};
class Mpris:public QObject {
    Q_OBJECT
public:
    explicit Mpris(NativeVideo *video);
    ~Mpris()override;
private:
    QString m_service;
};
