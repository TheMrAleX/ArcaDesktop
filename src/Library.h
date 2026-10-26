#pragma once
#include <QObject>
#include <QVariantList>
#include <QSqlDatabase>
#include <QFutureWatcher>
#include <QSettings>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QUrl>
#include <memory>
#include <QNetworkAccessManager>

class Library : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList files READ files NOTIFY changed)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(QVariantList shows READ shows NOTIFY changed)
    Q_PROPERTY(QVariantList continuing READ continuing NOTIFY changed)
    Q_PROPERTY(QString profile READ profile NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY scanningChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool onboarded READ onboarded NOTIFY changed)
    Q_PROPERTY(QVariantList matches READ matches NOTIFY matchesChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY matchesChanged)
public:
    explicit Library(const QString &dataDir = {}, QObject *parent = nullptr);
    ~Library() override;
    QVariantList files() const { return m_files; }
    QVariantList folders() const { return m_folders; }
    QVariantList profiles() const { return m_profiles; }
    QVariantList shows() const { return m_shows; }
    QVariantList continuing() const { return m_continuing; }
    QString profile() const;
    bool scanning() const { return m_scanning; }
    bool paused() const { return m_paused; }
    QString scanStatus() const { return m_scanStatus; }
    QString error() const { return m_error; }
    bool onboarded() const;
    QVariantList matches() const { return m_matches; }
    bool searching() const { return m_searching; }
    Q_INVOKABLE void finishWelcome();
    Q_INVOKABLE QVariant setting(const QString &key, const QVariant &fallback = {}) const;
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void addFolder(const QUrl &url, const QString &kind);
    Q_INVOKABLE void removeFolder(int id);
    Q_INVOKABLE void scan();
    Q_INVOKABLE void pauseScan();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void saveProgress(const QString &path, qint64 position, qint64 duration);
    Q_INVOKABLE void markWatched(const QString &path, bool watched);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void correct(const QString &path, const QString &title, int season, int episode, bool folderRule);
    Q_INVOKABLE void accept(const QString &path);
    Q_INVOKABLE void createProfile(const QString &name);
    Q_INVOKABLE void selectProfile(int id);
    Q_INVOKABLE void setFolderVisible(int id, bool visible);
    Q_INVOKABLE void showAllFolders();
    Q_INVOKABLE void hideContinue(const QString &path, bool hidden);
    Q_INVOKABLE void setShowSpoilers(const QString &key, int mode);
    Q_INVOKABLE QVariant playbackSetting(const QString &name, const QString &path, const QVariant &fallback = {}) const;
    Q_INVOKABLE void setPlaybackSetting(const QString &name, const QString &path, const QVariant &value);
    Q_INVOKABLE QString trackLanguage(const QVariant &value) const;
    Q_INVOKABLE QString trackLanguageName(const QVariant &value) const;
    Q_INVOKABLE QString encodeTrack(const QVariantMap &track) const;
    Q_INVOKABLE int pickTrack(const QVariantList &options, const QString &saved) const;
    Q_INVOKABLE int preferredTrack(const QVariantList &options, bool forcedOnly = false) const;
    Q_INVOKABLE bool exportBackup(const QUrl &url);
    Q_INVOKABLE bool importBackup(const QUrl &url);
    Q_INVOKABLE QVariantMap nextEpisode(const QString &path) const;
    Q_INVOKABLE QVariantMap media(const QString &path) const;
    Q_INVOKABLE QVariantList externalSubtitles(const QString &path) const;
    Q_INVOKABLE QVariantList readSubtitles(const QUrl &url) const;
    Q_INVOKABLE void findMetadata(const QString &title, bool series);
    Q_INVOKABLE void useMetadata(const QString &path, int id, bool series);
signals:
    void changed();
    void scanningChanged();
    void scanStatusChanged();
    void errorChanged();
    void matchesChanged();
private:
    void fail(const QString &message);
    bool exec(const QString &sql, const QVariantList &args = {});
    QVariantList query(const QString &sql, const QVariantList &args = {}) const;
    QSqlDatabase m_db;
    QString m_connection, m_dataDir, m_error, m_scanStatus;
    QSettings *m_settings;
    QVariantList m_files, m_folders, m_profiles;
    QVariantList m_shows, m_continuing;
    bool m_scanning = false, m_paused = false;
    struct ScanState;
    std::shared_ptr<ScanState> m_scanState;
    QFutureWatcher<QVariantMap> m_scan;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QNetworkAccessManager m_network;
    QVariantList m_matches;
    bool m_searching = false;
    int m_searchGeneration = 0;
    void tmdb(const QString &endpoint, const QVariantMap &parameters, std::function<void(QVariantMap)> done);
    void cacheImage(const QString &path, const QString &key, const QString &remote);
};
