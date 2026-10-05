#pragma once

#include <string_view>

namespace coverplayer {

enum class Language { German, English };

inline Language languageFromCode(std::string_view code) {
    return code == "en" ? Language::English : Language::German;
}

inline const char* languageCode(Language language) {
    return language == Language::English ? "en" : "de";
}

// German is the source language. User-provided album, track, folder and device
// names never pass through this table.
inline const char* tr(Language language, const char* source) {
    if (language == Language::German) return source;
    struct Entry { const char* de; const char* en; };
    static constexpr Entry entries[] = {
        {"SAMMLUNGEN", "COLLECTIONS"}, {"BIBLIOTHEK", "LIBRARY"},
        {"JETZT LAEUFT", "NOW PLAYING"}, {"SAMMLUNG", "COLLECTION"}, {"TYP", "TYPE"},
        {"NAME", "NAME"}, {"LOESCHEN", "DELETE"},
        {"ALBUMLISTE", "ALBUM LIST"}, {"TITEL", "TRACKS"},
        {"ORDNER", "FOLDERS"}, {"WIEDERGABE", "PLAYBACK"},
        {"SAMMLUNGSORDNER", "COLLECTION FOLDER"},
        {"SAMMLUNGEN VERWALTEN", "MANAGE COLLECTIONS"},
        {"NAME DER SAMMLUNG", "COLLECTION NAME"},
        {"SAMMLUNGSTYP", "COLLECTION TYPE"},
        {"SAMMLUNG LOESCHEN?", "DELETE COLLECTION?"},
        {"BLUETOOTH-KOPFHOERER", "BLUETOOTH HEADPHONES"},
        {"BIBLIOTHEK WIRD EINGELESEN", "SCANNING LIBRARY"},
        {"Noch keine Sammlung konfiguriert", "No collections configured yet"},
        {"Keine Medien in dieser Sammlung", "No media in this collection"},
        {"Ordner ist nicht erreichbar", "Folder is unavailable"},
        {"Bitte einen Namen eingeben", "Please enter a name"},
        {"Ordner wird bereits verwendet", "Folder is already in use"},
        {"Sammlung konnte nicht gespeichert werden", "Could not save collection"},
        {"Sammlung konnte nicht geloescht werden", "Could not delete collection"},
        {"Reihenfolge konnte nicht gespeichert werden", "Could not save order"},
        {"Mindestens eine Sammlung bleibt erhalten", "At least one collection must remain"},
        {"Ordner waehlen und Sammlung benennen", "Choose a folder and name the collection"},
        {"Bluetooth wird gelesen ...", "Reading Bluetooth status..."},
        {"Keine gekoppelten Kopfhoerer gefunden", "No paired headphones found"},
        {"Bluetooth-Status aktualisiert", "Bluetooth status updated"},
        {"Bluetooth eingeschaltet", "Bluetooth turned on"},
        {"Bluetooth ausgeschaltet", "Bluetooth turned off"},
        {"Bluetooth konnte nicht geschaltet werden", "Could not change Bluetooth power"},
        {"Verbunden - Lautstaerke synchronisiert", "Connected - volume matched"},
        {"Getrennt - Lautstaerke synchronisiert", "Disconnected - volume matched"},
        {"Audio bleibt pausiert: Lautstaerke nicht sicher synchronisiert", "Playback paused: volume could not be matched safely"},
        {"Audioausgabe nicht verfuegbar", "Audio output unavailable"},
        {"LESEZEICHEN GESPEICHERT", "BOOKMARK SAVED"},
        {"LESEZEICHEN", "BOOKMARKS"}, {"FERTIG", "DONE"},
        {"GEHOERT", "LISTENED"}, {"MEDIEN", "ITEMS"},
        {"EINTRAEGE", "ITEMS"},
        {"HOERBUCH", "AUDIOBOOK"}, {"HOERSPIEL", "RADIO PLAY"},
        {"MUSIK", "MUSIC"}, {"ALLGEMEIN", "OTHER"},
        {"[FEHLT]", "[MISSING]"}, {"[ORDNER]", "[FOLDER]"},
        {"[FERTIG]", "[DONE]"}, {"[+]  NEUE SAMMLUNG", "[+]  NEW COLLECTION"},
        {"A  ENDGUELTIG LOESCHEN", "A  DELETE COLLECTION"},
        {"B  ABBRECHEN", "B  CANCEL"},
        {"AN", "ON"}, {"AUS", "OFF"}, {"AKTIV", "ACTIVE"},
        {"NICHT AKTIV", "INACTIVE"}, {"[AKTIV]", "[ACTIVE]"},
        {"[VERBUNDEN]", "[CONNECTED]"},
        {"LEER", "SPACE"},
        {"NO COVER", "NO COVER"},
        {"BEDIENUNG", "CONTROLS"},
        {"SPRACHE: DEUTSCH", "LANGUAGE: ENGLISH"},
        {"Y ENGLISH", "Y DEUTSCH"},
        {"SELECT HILFE", "SELECT HELP"},
        {"B / SELECT SCHLIESSEN", "B / SELECT CLOSE"},
        {"START+SELECT  App beenden (Hilfe zu)", "START+SELECT  Exit app (close help first)"},
        {"A OEFFNEN   Y VERWALTEN   X SCANNEN", "A OPEN   Y MANAGE   X SCAN"},
        {"A PLAY/PAUSE   B ZURUECK", "A PLAY/PAUSE   B BACK"},
        {"START KURZ SLEEP / LANG HINTERGRUND", "TAP START SLEEP / HOLD FOR BACKGROUND"},
        {"A OEFFNEN   Y ORDNER WAEHLEN", "A OPEN   Y SELECT FOLDER"},
        {"A BEARBEITEN   Y PFAD   B ZURUECK", "A EDIT   Y PATH   B BACK"},
        {"A TYP WAEHLEN   B ZURUECK", "A SELECT TYPE   B BACK"},
        {"A ZEICHEN   Y SPEICHERN   B ABBRECHEN", "A CHARACTER   Y SAVE   B CANCEL"},
        {"A LOESCHEN   B ABBRECHEN", "A DELETE   B CANCEL"},
        {"A VERBINDEN   X AN/AUS   B ZURUECK", "A CONNECT   X ON/OFF   B BACK"},
        {"A OEFFNEN   Y LISTE   B ZURUECK", "A OPEN   Y LIST   B BACK"},
        {"A OEFFNEN   Y COVERFLOW   B ZURUECK", "A OPEN   Y COVERFLOW   B BACK"},
        {"A ABSPIELEN   B ZURUECK", "A PLAY   B BACK"},
        {"A  Wiedergabe / Pause", "A  Play / pause"},
        {"Links/Rechts  10 Sek. spulen", "Left/Right  Seek 10 sec"},
        {"L1/R1  30 Sek. spulen", "L1/R1  Seek 30 sec"},
        {"Oben/Unten  Titel wechseln", "Up/Down  Previous/next track"},
        {"Y  Lesezeichen setzen; X  naechstes", "Y  Add bookmark; X  next bookmark"},
        {"START kurz  Sleep-Timer", "Tap START  Sleep timer"},
        {"START 2 Sek.  Hintergrundwiedergabe", "Hold START 2 sec  Background playback"},
        {"A  Sammlung oeffnen", "A  Open collection"},
        {"Steuerkreuz  Sammlung waehlen", "D-pad  Choose collection"},
        {"Y  Sammlungen verwalten", "Y  Manage collections"},
        {"X  Bibliothek scannen", "X  Scan library"},
        {"B  Bleibt in der App", "B  Stay in the app"},
        {"A  Typ und Namen bearbeiten", "A  Edit type and name"},
        {"Y  Pfad bearbeiten / neu anlegen", "Y  Change or add folder"},
        {"X  Sammlung loeschen", "X  Delete collection"},
        {"L1/R1  Reihenfolge verschieben", "L1/R1  Change order"},
        {"B  Zurueck zur Bibliothek", "B  Back to library"},
        {"A  Sammlungstyp waehlen", "A  Choose collection type"},
        {"Steuerkreuz  Typ auswaehlen", "D-pad  Choose type"},
        {"B  Zurueck", "B  Back"},
        {"A  Sammlung entfernen", "A  Remove collection"},
        {"B  Abbrechen", "B  Cancel"},
        {"Mediendateien werden nie geloescht", "Media files are never deleted"},
        {"A  Zeichen anfuegen", "A  Add character"},
        {"X  Letztes Zeichen loeschen", "X  Delete last character"},
        {"L1  Namen komplett leeren", "L1  Clear name"},
        {"Y  Sammlung speichern", "Y  Save collection"},
        {"A  Verbinden / trennen", "A  Connect / disconnect"},
        {"X  Bluetooth an / aus", "X  Bluetooth on / off"},
        {"Y  Status aktualisieren", "Y  Refresh status"},
        {"Neue Geraete in Knulli koppeln", "Pair new devices in Knulli"},
        {"A  Ordner / Medium oeffnen", "A  Open folder / media"},
        {"B  Eine Ebene zurueck", "B  Go up one level"},
        {"Links/Rechts  Eintrag waehlen", "Left/Right  Choose item"},
        {"Y  CoverFlow / Liste", "Y  CoverFlow / list"},
        {"X  Sammlung neu scannen", "X  Scan collection again"},
        {"A  Abspielen / Fortsetzen", "A  Play / resume"},
        {"B  Zurueck zur Albumansicht", "B  Back to album"},
        {"Steuerkreuz  Titel auswaehlen", "D-pad  Choose track"},
        {"Fortsetzbarer Titel ist vorausgewaehlt", "Resume track is preselected"},
        {"A  Ordner oeffnen", "A  Open folder"},
        {"Y  Diesen Ordner auswaehlen", "Y  Select this folder"},
    };
    for (const auto& entry : entries) if (std::string_view(source) == entry.de) return entry.en;
    return source;
}

} // namespace coverplayer
