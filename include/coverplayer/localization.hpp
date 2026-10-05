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
        {"TASTE", "BUTTON"}, {"AKTION", "ACTION"},
        {"LINKS/RECHTS", "LEFT/RIGHT"}, {"OBEN/UNTEN", "UP/DOWN"},
        {"STEUERKREUZ", "D-PAD"}, {"START KURZ", "TAP START"},
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
        {"Wiedergabe / Pause", "Play / pause"},
        {"10 Sek. spulen", "Seek 10 sec"},
        {"30 Sek. spulen", "Seek 30 sec"},
        {"Titel wechseln", "Previous / next track"},
        {"Lesezeichen setzen / naechstes", "Set bookmark / next bookmark"},
        {"Sleep-Timer", "Sleep timer"},
        {"Hintergrundwiedergabe", "Background playback"},
        {"Sammlung oeffnen", "Open collection"},
        {"Sammlung waehlen", "Choose collection"},
        {"Sammlungen verwalten", "Manage collections"},
        {"Bibliothek scannen", "Scan library"},
        {"App bleibt geoeffnet", "App stays open"},
        {"Typ und Namen bearbeiten", "Edit type and name"},
        {"Pfad bearbeiten / neu anlegen", "Change or add folder"},
        {"Sammlung loeschen", "Delete collection"},
        {"Reihenfolge verschieben", "Change order"},
        {"Zurueck zur Bibliothek", "Back to library"},
        {"Sammlungstyp waehlen", "Choose collection type"},
        {"Typ auswaehlen", "Choose type"},
        {"Zurueck", "Back"},
        {"Sammlung entfernen", "Remove collection"},
        {"Abbrechen", "Cancel"},
        {"Mediendateien werden nie geloescht", "Media files are never deleted"},
        {"Zeichen anfuegen", "Add character"},
        {"Letztes Zeichen loeschen", "Delete last character"},
        {"Namen komplett leeren", "Clear name"},
        {"Sammlung speichern", "Save collection"},
        {"Verbinden / trennen", "Connect / disconnect"},
        {"Bluetooth an / aus", "Bluetooth on / off"},
        {"Status aktualisieren", "Refresh status"},
        {"Neue Geraete in Knulli koppeln", "Pair new devices in Knulli"},
        {"Ordner / Medium oeffnen", "Open folder / media"},
        {"Eine Ebene zurueck", "Go up one level"},
        {"Eintrag waehlen", "Choose item"},
        {"CoverFlow / Liste", "CoverFlow / list"},
        {"Sammlung neu scannen", "Scan collection again"},
        {"Abspielen / Fortsetzen", "Play / resume"},
        {"Zurueck zur Albumansicht", "Back to album"},
        {"Titel auswaehlen", "Choose track"},
        {"Fortsetzbarer Titel ist vorausgewaehlt", "Resume track is preselected"},
        {"Ordner oeffnen", "Open folder"},
        {"Diesen Ordner auswaehlen", "Select this folder"},
    };
    for (const auto& entry : entries) if (std::string_view(source) == entry.de) return entry.en;
    return source;
}

} // namespace coverplayer
