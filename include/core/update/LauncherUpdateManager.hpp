#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QCryptographicHash;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace core::update
{
    class LauncherUpdateManager final : public QObject
    {
        Q_OBJECT

    public:
        explicit LauncherUpdateManager(QObject* parent = nullptr);
        ~LauncherUpdateManager() override;

        void check_for_updates();
        void download_and_install();
        void cancel_download();

        [[nodiscard]] bool update_available() const;
        [[nodiscard]] bool update_required() const;
        [[nodiscard]] QString available_version() const;
        [[nodiscard]] QString release_message() const;
        [[nodiscard]] QString platform_key() const;
        [[nodiscard]] QString downloaded_path() const;

    signals:
        void check_started();
        void no_update_available();
        void update_found();
        void check_failed(const QString& reason);
        void download_started();
        void download_progress(qint64 received, qint64 total);
        void installer_started(const QString& path);
        void update_failed(const QString& reason);

    private:
        void reset_release();
        void finish_check(QNetworkReply* reply);
        void begin_download();
        void consume_download_data();
        void finish_download();
        void fail_download(const QString& reason);
        void install_downloaded_package();
        void install_linux_appimage();
        void open_macos_installer();
        [[nodiscard]] QString github_api_url() const;
        [[nodiscard]] QString choose_download_path() const;
        [[nodiscard]] bool insecure_urls_allowed() const;
        [[nodiscard]] bool validate_package_url(const QUrl& url) const;
        [[nodiscard]] static int compare_versions(const QString& left, const QString& right);
        [[nodiscard]] static QString detected_platform_key();
        [[nodiscard]] static QString safe_file_name(const QString& value);
        static void cleanup_previous_linux_package();

        QNetworkAccessManager* network {};
        QPointer<QNetworkReply> check_reply;
        QPointer<QNetworkReply> download_reply;
        QTimer* check_timeout {};
        QTimer* download_timeout {};
        QFile download_file;
        QCryptographicHash* download_hash {};
        QString release_version;
        QString minimum_version;
        QString message;
        QString package_kind;
        QString package_file_name;
        QString final_download_path;
        QString partial_download_path;
        QUrl package_url;
        QByteArray expected_sha256;
        qint64 expected_size {-1};
        qint64 downloaded_size {};
        bool required {};
        bool downloading {};
        bool download_write_failed {};
    };
}
