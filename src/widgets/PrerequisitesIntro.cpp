#include "widgets/PrerequisitesIntro.hpp"

#include "util/Assets.hpp"
#include "util/Colors.hpp"
#include "util/Config.hpp"
#include "util/Layout.hpp"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QStringList>
#include <QtConcurrent/QtConcurrentRun>

namespace
{
    constexpr QSize k_box_size {620, 300};

    QRect box_rect(const QSize window_size)
    {
        return util::layout::centered(k_box_size, window_size, 0, 8);
    }

    QRect local_rect(const QSize window_size, const QRect source)
    {
        return util::layout::scaled(source, window_size).translated(box_rect(window_size).topLeft());
    }

    const char* k_recommendation_style =
        "QLabel { background:rgba(255,255,255,0.68); border:1px solid rgba(201,187,170,190);"
        " border-radius:8px; color:#392518; font-family:'Inter'; font-size:13px;"
        " padding:12px 18px; }";

    const char* k_error_style =
        "QLabel { background:rgba(255,245,242,0.88); border:1px solid rgba(192,111,91,190);"
        " border-radius:8px; color:#7F2929; font-family:'Inter'; font-size:13px;"
        " padding:12px 18px; }";

    const char* k_primary_style =
        "QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #54D8FF,stop:1 #08A9D8);"
        " border:1px solid #159FC8; border-radius:6px; color:white; font-family:'Eurostile';"
        " font-weight:900; font-size:17px; outline:none; }"
        "QPushButton:hover { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #77E2FF,stop:1 #18B9E8); }"
        "QPushButton:pressed { background:#0798C5; }"
        "QPushButton:disabled { background:#D8CDC0; border-color:#C9BBAA; color:#9E8E7E; }"
        "QPushButton:focus { outline:none; }";

    const char* k_secondary_style =
        "QPushButton { background:rgba(255,255,255,0.64); border:1px solid #C9BBAA;"
        " border-radius:6px; color:#4F1717; font-family:'Eurostile'; font-weight:900;"
        " font-size:13px; outline:none; }"
        "QPushButton:hover { border-color:#2FB4E0; background:rgba(255,255,255,0.86); }"
        "QPushButton:pressed { background:#EAF7FC; }"
        "QPushButton:focus { outline:none; }";

    int runtime_score(const core::wine::WineInstall& runtime)
    {
        const QString name = runtime.name.toLower();
        if (runtime.type == core::wine::RuntimeType::Wine)
        {
            if (name == QStringLiteral("system wine")) return 10000;
            if (name.contains(QStringLiteral("cachyos"))) return 9000;
            return 8000;
        }

        int score = 0;
        if (name.startsWith(QStringLiteral("proton "))
            && !name.contains(QStringLiteral("experimental"))
            && !name.contains(QStringLiteral("hotfix")))
        {
            score = 10000;
        }
        else if (name.contains(QStringLiteral("ge-proton")))
        {
            score = 9000;
        }
        else if (name.contains(QStringLiteral("experimental")))
        {
            score = 7500;
        }
        else if (name.contains(QStringLiteral("hotfix")))
        {
            score = 7000;
        }
        else
        {
            score = 8000;
        }

        const QRegularExpression version_pattern(QStringLiteral(R"((\d+)(?:\.(\d+))?)"));
        const QRegularExpressionMatch match = version_pattern.match(name);
        if (match.hasMatch())
            score += match.captured(1).toInt() * 100 + match.captured(2).toInt();
        return score;
    }

    QString joined_requirements(const QStringList& requirements)
    {
        if (requirements.isEmpty())
            return {};
        if (requirements.size() == 1)
            return requirements.front();
        if (requirements.size() == 2)
            return requirements.front() + QStringLiteral(" and ") + requirements.back();

        QStringList leading = requirements;
        const QString last = leading.takeLast();
        return leading.join(QStringLiteral(", ")) + QStringLiteral(", and ") + last;
    }
}

PrerequisitesIntro::PrerequisitesIntro(QWidget* parent)
    : ModalOverlay(parent),
      detector(new QFutureWatcher<core::system::SystemProfile>(this))
{
    set_keeps_chrome(false);
    setup_controls();
    connect(detector, &QFutureWatcher<core::system::SystemProfile>::finished,
            this, &PrerequisitesIntro::finish_detection);
}

void PrerequisitesIntro::setup_controls()
{
    const QSize w = window()->size();

    recommendation_title = new QLabel(this);
    recommendation_title->setTextFormat(Qt::PlainText);
    recommendation_title->setAlignment(Qt::AlignCenter);
    recommendation_title->setStyleSheet(
        "color:#4F1717; background:transparent; font-family:'Eurostile';"
        " font-weight:900; font-size:18px;");
    recommendation_title->setGeometry(local_rect(w, {42, 92, 536, 30}));

    recommendation_body = new QLabel(this);
    recommendation_body->setTextFormat(Qt::PlainText);
    recommendation_body->setWordWrap(true);
    recommendation_body->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    recommendation_body->setStyleSheet(k_recommendation_style);
    recommendation_body->setGeometry(local_rect(w, {52, 124, 516, 96}));
    auto* shadow = new QGraphicsDropShadowEffect(recommendation_body);
    shadow->setBlurRadius(util::layout::scaled(15, w));
    shadow->setOffset(0, util::layout::scaled(4, w));
    shadow->setColor(QColor(79, 23, 23, 42));
    recommendation_body->setGraphicsEffect(shadow);

    continue_button = new QPushButton(QStringLiteral("CHECKING..."), this);
    continue_button->setCursor(Qt::PointingHandCursor);
    continue_button->setStyleSheet(k_primary_style);
    continue_button->setGeometry(local_rect(w, {84, 236, 330, 46}));
    continue_button->setEnabled(false);
    connect(continue_button, &QPushButton::clicked,
            this, &PrerequisitesIntro::apply_recommendation);

    choose_own_button = new QPushButton(QStringLiteral("CHOOSE MY OWN"), this);
    choose_own_button->setCursor(Qt::PointingHandCursor);
    choose_own_button->setStyleSheet(k_secondary_style);
    choose_own_button->setGeometry(local_rect(w, {426, 236, 140, 46}));
    connect(choose_own_button, &QPushButton::clicked,
            this, &PrerequisitesIntro::choose_own_requested);
}

void PrerequisitesIntro::start_detection()
{
    if (detector->isRunning())
        return;

    detection_complete = false;
    recommendation_title->setText(QStringLiteral("CHECKING THIS COMPUTER"));
    recommendation_body->setStyleSheet(k_recommendation_style);
    recommendation_body->setText(QStringLiteral(
        "Looking for a usable Wine or Proton setup. Nothing will be installed automatically."));
    continue_button->setText(QStringLiteral("CHECKING..."));
    continue_button->setEnabled(false);

    detector->setFuture(QtConcurrent::run(core::system::detect_system_profile));
}

void PrerequisitesIntro::finish_detection()
{
    system_profile = detector->result();
    runtimes = core::wine::WineRegistry::scan();
    winetricks_ready = core::wine::winetricks_available();
    umu_ready = core::wine::umu_available();
    detection_complete = true;
    update_recommendation();
}

core::wine::RuntimeType PrerequisitesIntro::recommended_runtime() const
{
#if defined(Q_OS_MACOS)
    return core::wine::RuntimeType::Wine;
#else
    const bool proton_found = best_runtime(core::wine::RuntimeType::Proton) != nullptr;
    const bool wine_found = best_runtime(core::wine::RuntimeType::Wine) != nullptr;
    const bool proton_ready = proton_found && umu_ready && winetricks_ready;
    const bool wine_ready = wine_found && winetricks_ready;

    if (proton_ready)
        return core::wine::RuntimeType::Proton;
    if (wine_ready)
        return core::wine::RuntimeType::Wine;
    if (proton_found)
        return core::wine::RuntimeType::Proton;
    return core::wine::RuntimeType::Wine;
#endif
}

const core::wine::WineInstall* PrerequisitesIntro::best_runtime(
    const core::wine::RuntimeType type) const
{
    const core::wine::WineInstall* best = nullptr;
    int best_score = -1;
    for (const auto& runtime : runtimes)
    {
        if (runtime.type != type)
            continue;
        const int score = runtime_score(runtime);
        if (!best || score > best_score)
        {
            best = &runtime;
            best_score = score;
        }
    }
    return best;
}

bool PrerequisitesIntro::recommended_dxvk() const
{
#if defined(Q_OS_MACOS)
    return false;
#else
    return system_profile.vulkan_likely
        && system_profile.gpu_vendor != core::system::GpuVendor::Apple
        && system_profile.gpu_vendor != core::system::GpuVendor::Unknown;
#endif
}

QStringList PrerequisitesIntro::missing_requirements(const core::wine::RuntimeType type) const
{
    QStringList missing;
    if (!best_runtime(type))
    {
        missing << (type == core::wine::RuntimeType::Proton
                        ? QStringLiteral("Proton")
                        : QStringLiteral("Wine"));
    }

    if (type == core::wine::RuntimeType::Proton && !umu_ready)
        missing << QStringLiteral("UMU");

    // Prefix components are installed through Winetricks for both runtimes.
    if (!winetricks_ready)
        missing << QStringLiteral("Winetricks");

    return missing;
}

bool PrerequisitesIntro::profile_ready(QString* blocker) const
{
    if (!detection_complete)
    {
        if (blocker)
            *blocker = QStringLiteral("The system check is still running.");
        return false;
    }

    const auto runtime_type = recommended_runtime();
    const QStringList missing = missing_requirements(runtime_type);
    if (!missing.isEmpty())
    {
        if (blocker)
        {
            const QString missing_text = joined_requirements(missing);
            const QString install_wording = missing.size() == 1
                ? QStringLiteral("Install it")
                : QStringLiteral("Install them");
            if (runtime_type == core::wine::RuntimeType::Proton)
            {
                *blocker = QStringLiteral(
                    "Proton needs Proton and UMU. Winetricks is also required for Alicia's "
                    "Windows components.\nMissing: %1. %2, then restart the launcher.")
                    .arg(missing_text, install_wording);
            }
            else
            {
                *blocker = QStringLiteral(
                    "Pure Wine needs both Wine and Winetricks.\nMissing: %1. "
                    "%2, then restart the launcher.")
                    .arg(missing_text, install_wording);
            }
        }
        return false;
    }
    return true;
}

void PrerequisitesIntro::update_recommendation()
{
    if (!detection_complete)
        return;

    const auto runtime_type = recommended_runtime();
    const QString runtime_label = runtime_type == core::wine::RuntimeType::Proton
        ? QStringLiteral("PROTON") : QStringLiteral("WINE");

    QString blocker;
    if (!profile_ready(&blocker))
    {
        recommendation_title->setText(QStringLiteral("%1 REQUIREMENTS").arg(runtime_label));
        recommendation_body->setStyleSheet(k_error_style);
        recommendation_body->setText(blocker);
        continue_button->setText(QStringLiteral("USE THIS SETUP"));
        continue_button->setEnabled(false);
        return;
    }

    const auto* runtime = best_runtime(runtime_type);
    recommendation_title->setText(QStringLiteral("%1 IS READY").arg(runtime_label));
    recommendation_body->setStyleSheet(k_recommendation_style);
    recommendation_body->setText(QStringLiteral(
        "%1 will be used for Alicia with %2. Nothing will be installed automatically.")
        .arg(runtime ? runtime->name : runtime_label)
        .arg(recommended_dxvk()
                 ? QStringLiteral("faster graphics")
                 : QStringLiteral("compatibility graphics")));
    continue_button->setText(QStringLiteral("USE THIS SETUP"));
    continue_button->setEnabled(true);
}

void PrerequisitesIntro::apply_recommendation()
{
    QString blocker;
    if (!profile_ready(&blocker))
    {
        recommendation_body->setStyleSheet(k_error_style);
        recommendation_body->setText(blocker);
        return;
    }

    const auto runtime_type = recommended_runtime();
    const auto* runtime = best_runtime(runtime_type);
    if (!runtime)
        return;

    auto& config = util::config::Config::instance();
    config.set_setup_runtime_preference(
        runtime_type == core::wine::RuntimeType::Proton
            ? QStringLiteral("proton") : QStringLiteral("wine"));
    config.set_setup_pc_age(QStringLiteral("unknown"));
    config.set_use_dxvk(recommended_dxvk());
    config.set_wine_binary(runtime->path);
    config.set_runtime_selected(true);

    emit accepted();
}

void PrerequisitesIntro::showEvent(QShowEvent* event)
{
    if (!detection_complete)
        start_detection();
    else
        update_recommendation();

    ModalOverlay::showEvent(event);
}

void PrerequisitesIntro::paint_content(QPainter& painter)
{
    const QSize w = window()->size();
    const QRect box = box_rect(w);
    painter.drawPixmap(box, util::assets::images[util::assets::Image::BoxSettings]);

    QFont title_font = util::assets::fonts[util::assets::Font::EurostileExtraBlack];
    title_font.setPixelSize(util::layout::scaled(27, w));
    title_font.setWeight(QFont::Black);
    painter.setFont(title_font);
    painter.setPen(util::colors::k_text_maroon);
    painter.drawText(local_rect(w, {20, 28, 580, 38}), Qt::AlignCenter,
                     QStringLiteral("EASY SETUP"));

    QFont body_font = util::assets::fonts[util::assets::Font::Inter];
    body_font.setPixelSize(util::layout::scaled(14, w));
    body_font.setWeight(QFont::Medium);
    painter.setFont(body_font);
    painter.setPen(util::colors::k_text_body);
    painter.drawText(local_rect(w, {55, 66, 510, 24}),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     QStringLiteral("A safe setup has been selected for this computer."));
}
