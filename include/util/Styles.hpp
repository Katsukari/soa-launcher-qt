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
        "QPushButton:hover { background: #E6DCD0; }";

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
        "QPushButton:hover { background: #4FC4EF; }";

    inline constexpr const char* k_flat_transparent =
        "border:none; outline:none; background:transparent;";

    inline constexpr const char* k_link_blue =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    border: none;"
        "    outline: none;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; }"
        "QPushButton:focus { outline: none; border: none; }";

    inline constexpr const char* k_link_blue_lg =
        "QPushButton"
        "{"
        "    background: transparent;"
        "    color: #2FB4E0;"
        "    font-family: 'Inter';"
        "    font-size: 15px;"
        "    text-decoration: underline;"
        "}"
        "QPushButton:hover { color: #6FD4EF; outline: none; border: none; }";
}