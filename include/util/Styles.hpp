#pragma once

// Shared Qt stylesheet strings for settings controls.
// These were duplicated across LauncherSettings / WineSettings / AdvancedSettings;
// kept in one place so the look of fields and buttons changes everywhere at once.

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

    // Neutral cream button (browse "...", GitHub, Show Log).
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

    // Primary teal button (Generate)
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
}