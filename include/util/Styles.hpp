#pragma once

namespace util::styles
{
    inline constexpr const char* k_field =
        "QLineEdit"
        "{"
        "    background: rgba(255,255,255,0.45);"
        "    border: 1px solid #C9BBAA;"
        "    border-radius: 6px;"
        "    padding: 0 8px;"
        "    color: #4F1717;"
        "    font-family: 'Inter';"
        "}"
        "QLineEdit:disabled"
        "{"
        "    background: rgba(231,224,216,0.60);"
        "    border-color: #D8CDC0;"
        "    color: #9E8E7E;"
        "}";

    inline constexpr const char* k_neutral_button =
        "QPushButton"
        "{"
        "    background: #D8CDC0;"
        "    border: none;"
        "    border-radius: 6px;"
        "    color: #4F1717;"
        "    font-family: 'Eurostile';"
        "    font-weight: 900;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background: #E6DCD0; }"
        "QPushButton:pressed { background: #C9BBAA; }"
        "QPushButton:disabled { background: #E7E0D8; color: #9E8E7E; }";

    inline constexpr const char* k_primary_button =
        "QPushButton"
        "{"
        "    background: #2FB4E0;"
        "    border: none;"
        "    border-radius: 6px;"
        "    color: #FFFFFF;"
        "    font-family: 'Eurostile';"
        "    font-weight: 900;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background: #4FC4EF; }"
        "QPushButton:pressed { background: #168EB8; }"
        "QPushButton:disabled { background: #D8CDC0; color: #9E8E7E; }";

    inline constexpr const char* k_flat_transparent =
        "border:none; background:transparent;";

    inline constexpr const char* k_link_blue =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: none;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; }";

    inline constexpr const char* k_link_blue_lg =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: none;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 15px;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; }";
}
