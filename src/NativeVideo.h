#pragma once
#include <QQuickFramebufferObject>
#include <QUrl>
#include <QVariantList>
#include <memory>
#include <atomic>

struct MpvState;
class NativeVideo : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(int mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
    Q_PROPERTY(int error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(bool hardware READ hardware WRITE setHardware NOTIFY hardwareChanged)
    Q_PROPERTY(double subtitleDelay READ subtitleDelay WRITE setSubtitleDelay NOTIFY subtitleDelayChanged)
    Q_PROPERTY(double subtitleScale READ subtitleScale WRITE setSubtitleScale NOTIFY subtitleScaleChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(int activeAudioTrack READ activeAudioTrack WRITE setActiveAudioTrack NOTIFY tracksChanged)
    Q_PROPERTY(int activeSubtitleTrack READ activeSubtitleTrack WRITE setActiveSubtitleTrack NOTIFY tracksChanged)
    Q_PROPERTY(int fitMode READ fitMode WRITE setFitMode NOTIFY fitModeChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle WRITE setMediaTitle NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool hasNext READ hasNext WRITE setHasNext NOTIFY mediaInfoChanged)
    Q_PROPERTY(int renderedFrames READ renderedFrames NOTIFY frameRendered)
public:
    enum PlaybackState { StoppedState, PlayingState, PausedState }; Q_ENUM(PlaybackState)
    enum MediaStatus { NoMedia, LoadingMedia, LoadedMedia, BufferedMedia, EndOfMedia }; Q_ENUM(MediaStatus)
    enum MediaError { NoError, ResourceError }; Q_ENUM(MediaError)
    explicit NativeVideo(QQuickItem *parent = nullptr);
    ~NativeVideo() override;
    Renderer *createRenderer() const override;
    QString mediaTitle() const { return m_title; }
    void setMediaTitle(const QString &title) { if(m_title==title)return;m_title=title;emit mediaInfoChanged(); }
    bool hasNext() const { return m_hasNext; }
    void setHasNext(bool next) { if(m_hasNext==next)return;m_hasNext=next;emit mediaInfoChanged(); }
    QUrl source() const { return m_source; }
    void setSource(const QUrl &url);
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    int playbackState() const { return m_status==NoMedia||m_status==EndOfMedia?StoppedState:m_paused?PausedState:PlayingState; }
    int mediaStatus() const { return m_status; }
    int error() const { return m_error.isEmpty()?NoError:ResourceError; }
    QString errorString() const { return m_error; }
    double volume() const { return m_volume; }
    void setVolume(double volume);
    double playbackRate() const { return m_rate; }
    void setPlaybackRate(double rate);
    bool hardware() const { return m_hardware; }
    void setHardware(bool enabled);
    double subtitleDelay() const { return m_delay; }
    void setSubtitleDelay(double seconds);
    double subtitleScale() const { return m_scale; }
    void setSubtitleScale(double scale);
    QVariantList audioTracks() const { return m_audio; }
    QVariantList subtitleTracks() const { return m_subs; }
    int activeAudioTrack() const;
    void setActiveAudioTrack(int index);
    int activeSubtitleTrack() const;
    void setActiveSubtitleTrack(int index);
    int fitMode() const { return m_fit; }
    void setFitMode(int mode);
    int renderedFrames() const { return m_frames.load(); }
    QSize videoSize() const;
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setPosition(qint64 milliseconds);
    Q_INVOKABLE void addSubtitle(const QUrl &url);
    Q_INVOKABLE void retrySoftware();
    Q_INVOKABLE QStringList screens() const;
    Q_INVOKABLE void moveToScreen(int index);
signals:
    void mediaInfoChanged();
    void sourceChanged(); void positionChanged(); void durationChanged();
    void playbackStateChanged(); void mediaStatusChanged(); void errorChanged();
    void volumeChanged(); void playbackRateChanged(); void hardwareChanged();
    void subtitleDelayChanged(); void subtitleScaleChanged(); void tracksChanged();
    void fitModeChanged(); void frameRendered();
    void nextRequested(); void previousRequested();
public slots:
    void processEvents();
    void reportFrame();
    void rendererReady();
private:
    friend class VideoRenderer;
    void command(const QStringList &args);
    void property(const char *name, const QVariant &value);
    void refreshTracks();
    void setStatus(MediaStatus status);
    void load(qint64 at = 0);
    std::shared_ptr<MpvState> m_state;
    QUrl m_source;
    QVariantList m_audio, m_subs;
    qint64 m_position=0,m_duration=0;
    double m_volume=0.8,m_rate=1,m_delay=0,m_scale=1;
    bool m_paused=false,m_hardware=true,m_retried=false;
    int m_aid=-1,m_sid=-1,m_fit=0;
    MediaStatus m_status=NoMedia;
    QString m_error,m_decodeLog,m_title;
    bool m_hasNext=false;
    std::atomic<int> m_frames{0};
    qint64 m_pendingStart=0;
};
