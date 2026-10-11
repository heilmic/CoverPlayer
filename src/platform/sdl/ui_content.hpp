#pragma once

#include "coverplayer/platform/platform.hpp"
#include "platform/sdl/ui_profile.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace coverplayer::platform::ui {
// Counts UTF-8 codepoints (not bytes) so text fitting never splits a
// multi-byte sequence, which would otherwise corrupt the trailing
// character of a truncated German umlaut/ID3-tag string.
inline std::size_t utf8Length(const std::string &text) {
    std::size_t count = 0;
    for (const unsigned char character : text)
        if ((character & 0xC0U) != 0x80U)
            ++count;
    return count;
}

inline std::size_t utf8ByteOffsetOfCodepoint(const std::string &text, std::size_t codepointIndex) {
    std::size_t byteIndex = 0;
    std::size_t seen = 0;
    while (byteIndex < text.size() && seen < codepointIndex) {
        ++byteIndex;
        while (byteIndex < text.size() && (static_cast<unsigned char>(text[byteIndex]) & 0xC0U) == 0x80U)
            ++byteIndex;
        ++seen;
    }
    return byteIndex;
}

inline std::string formatTime(double value) {
    const int seconds = std::max(0, static_cast<int>(value));
    const int hours = seconds / 3600;
    const int minutes = (seconds / 60) % 60;
    const int remainder = seconds % 60;
    if (hours > 0)
        return std::to_string(hours) + ":" + (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
               (remainder < 10 ? "0" : "") + std::to_string(remainder);
    return std::to_string(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + std::to_string(remainder);
}

inline const char *sectionLabel(Screen screen) {
    switch (screen) {
    case Screen::Collections:
        return "BIBLIOTHEK";
    case Screen::CollectionManager:
        return "SAMMLUNGEN";
    case Screen::CollectionType:
        return "TYP";
    case Screen::CollectionName:
        return "NAME";
    case Screen::CollectionDelete:
        return "LOESCHEN";
    case Screen::CoverFlow:
        return "COVERFLOW";
    case Screen::AlbumList:
        return "ALBUMLISTE";
    case Screen::Tracks:
        return "TITEL";
    case Screen::Folders:
        return "ORDNER";
    case Screen::Bluetooth:
        return profile.switchUi ? "BIBLIOTHEK" : "BLUETOOTH";
    case Screen::Player:
        return "JETZT LAEUFT";
    }
    return "BIBLIOTHEK";
}
inline const char *primaryHint(Screen screen) {
    const char *hint = "A OEFFNEN   Y VERWALTEN   X SCANNEN";
    switch (screen) {
    case Screen::Player:
        hint = "A PAUSE   OBEN/UNTEN TITEL   B ZURUECK";
        break;
    case Screen::Folders:
        hint = "A OEFFNEN   Y ORDNER WAEHLEN";
        break;
    case Screen::CollectionManager:
        hint = "A EDIT   Y PFAD   X LOESCHEN";
        break;
    case Screen::CollectionType:
        hint = "A TYP WAEHLEN   B ZURUECK";
        break;
    case Screen::CollectionName:
        hint = "A ZEICHEN   Y SPEICHERN   B ABBRECHEN";
        break;
    case Screen::CollectionDelete:
        hint = "A LOESCHEN   B ABBRECHEN";
        break;
    case Screen::Bluetooth:
        if constexpr (!profile.switchUi)
            hint = "A VERBINDEN   X AN/AUS   B ZURUECK";
        break;
    case Screen::CoverFlow:
        hint = "A OEFFNEN   Y LISTE   B ZURUECK";
        break;
    case Screen::AlbumList:
        hint = "A OEFFNEN   Y COVERFLOW   B ZURUECK";
        break;
    case Screen::Tracks:
        hint = "A ABSPIELEN   B ZURUECK";
        break;
    default:
        break;
    }
    return hint;
}
struct HelpRow {
    const char *key = nullptr;
    const char *action = nullptr;
};
struct HelpContent {
    std::array<HelpRow, 7> rows{};
    const char *note = nullptr;
};

inline HelpContent helpContent(Screen screen, Language language) {
    // Switch has no Bluetooth management; preserve its generic browser help
    // when the desktop screenshot tool supplies that synthetic screen.
    if constexpr (profile.switchUi) {
        if (screen == Screen::Bluetooth)
            screen = Screen::CoverFlow;
    }
    HelpContent content;
    auto &rows = content.rows;
    auto &note = content.note;
    note =
        screen == Screen::Player ? "Linker Stick = Steuerkreuz" : "Kreuz / linker Stick: halten = schneller";
    switch (screen) {
    case Screen::Player:
        rows = {{{"A", "Wiedergabe / Pause"},
                 {"LINKS/RECHTS", "10 Sek. spulen"},
                 {"L1/R1", "30 Sek. spulen"},
                 {"OBEN/UNTEN", "Titel wechseln"},
                 {"Y / X", "Lesezeichen setzen / naechstes"},
                 {"START KURZ", "Sleep-Timer"},
                 {"START 2s", "Hintergrundwiedergabe"}}};
        break;
    case Screen::Collections:
        rows = {{{"A", "Sammlung oeffnen"},
                 {"STEUERKREUZ", "Sammlung waehlen"},
                 {"Y", "Sammlungen verwalten"},
                 {"X", "Bibliothek scannen"},
                 {"B", "App bleibt geoeffnet"}}};
        break;
    case Screen::CollectionManager:
        rows = {{{"A", "Typ und Namen bearbeiten"},
                 {"Y", "Pfad bearbeiten / neu anlegen"},
                 {"X", "Sammlung loeschen"},
                 {"L1/R1", "Reihenfolge verschieben"},
                 {"B", "Zurueck zur Bibliothek"}}};
        break;
    case Screen::CollectionType:
        rows = {{{"A", "Sammlungstyp waehlen"}, {"STEUERKREUZ", "Typ auswaehlen"}, {"B", "Zurueck"}}};
        break;
    case Screen::CollectionDelete:
        rows = {{{"A", "Sammlung entfernen"}, {"B", "Abbrechen"}}};
        note = "Mediendateien werden nie geloescht";
        break;
    case Screen::CollectionName:
        rows = {{{"A", "Zeichen anfuegen"},
                 {"X", "Letztes Zeichen loeschen"},
                 {"L1", "Namen komplett leeren"},
                 {"Y", "Sammlung speichern"},
                 {"B", "Abbrechen"}}};
        break;
    case Screen::Bluetooth:
        rows = {{{"A", "Verbinden / trennen"},
                 {"X", "Bluetooth an / aus"},
                 {"Y", "Status aktualisieren"},
                 {"B", "Zurueck"}}};
        note = "Neue Geraete in Knulli koppeln";
        break;
    case Screen::CoverFlow:
    case Screen::AlbumList:
        rows = {{{"A", "Ordner / Medium oeffnen"},
                 {"B", "Eine Ebene zurueck"},
                 {"LINKS/RECHTS", "Eintrag waehlen"},
                 {"Y", "CoverFlow / Liste"},
                 {"X", "Sammlung neu scannen"}}};
        break;
    case Screen::Tracks:
        rows = {{{"A", "Abspielen / Fortsetzen"},
                 {"B", "Zurueck zur Albumansicht"},
                 {"STEUERKREUZ", "Titel auswaehlen"}}};
        note = "Fortsetzbarer Titel ist vorausgewaehlt";
        break;
    case Screen::Folders:
        rows = {{{"A", "Ordner oeffnen"}, {"B", "Zurueck"}, {"Y", "Diesen Ordner auswaehlen"}}};
        break;
    }
    if constexpr (profile.switchUi) {
        if (screen == Screen::Player) {
            rows[5] = {language == Language::German ? "PLUS KURZ" : "TAP PLUS", "Sleep-Timer"};
            rows[6] = {"PLUS 2s", language == Language::German ? "App beenden" : "Quit app"};
        }
        for (auto &row : rows) {
            if (row.key == nullptr)
                break;
            if (std::strcmp(row.key, "L1/R1") == 0)
                row.key = "L/R";
            if (std::strcmp(row.key, "L1") == 0)
                row.key = "L";
        }
    }
    return content;
}

} // namespace coverplayer::platform::ui
