#pragma once

#include <QByteArray>
#include <QEvent>
#include <QString>
#include <QPointer>

#include "util/ModalOverlay.hpp"

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QShowEvent;
class QTextBrowser;
class QTimer;
class QUrl;

class RulesAgreement final : public util::modal_overlay::ModalOverlay
{
    Q_OBJECT

public:
    explicit RulesAgreement(QWidget* parent = nullptr);

signals:
    void accepted();

protected:
    void paint_content(QPainter& painter) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void setup_controls();
    void load_rules();
    void finish_rules_request(QNetworkReply* reply);
    void show_document(const QByteArray& source, bool save_cache);
    void show_load_failure(const QString& reason);
    void start_cooldown();
    void update_agree_button();
    void retranslate_content();
    void set_button_pixmap(const QPixmap& pixmap);
    void set_button_text(const QString& source);
    [[nodiscard]] QString rules_url() const;
    [[nodiscard]] QString cache_path() const;
    [[nodiscard]] bool load_cached_document();
    [[nodiscard]] static QString prepare_document(const QByteArray& source);
    [[nodiscard]] static QString rewrite_links(const QString& html);

    QTextBrowser* rules_text {};
    QPushButton* agree_button {};
    QLabel* agree_button_label {};
    QNetworkAccessManager* network {};
    QPointer<QNetworkReply> reply;
    QTimer* request_timeout {};
    QTimer* cooldown_timer {};
    QString document_html;
    QString load_error;
    int seconds_remaining {10};
    bool document_ready {};
    bool has_scrolled_to_end {};
    bool loading {};
};
