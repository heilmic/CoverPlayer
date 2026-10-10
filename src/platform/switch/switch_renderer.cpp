// Compiled only for Switch and the explicitly enabled desktop preview tool.
#include "platform/sdl/sdl_renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace coverplayer::platform {
namespace {
constexpr SDL_Color primary{244,247,251,255};
constexpr SDL_Color secondary{158,168,184,255};
constexpr SDL_Color mint{92,211,151,255};
constexpr SDL_Color warning{242,190,92,255};
constexpr SDL_Color active{86,163,255,255};

std::string timeLabel(double value) {
    const int total = std::max(0, static_cast<int>(value));
    const auto two = [](int n) { return (n < 10 ? "0" : "") + std::to_string(n); };
    return total >= 3600 ? std::to_string(total/3600) + ":" + two(total/60%60) + ":" + two(total%60)
        : std::to_string(total/60) + ":" + two(total%60);
}
}

void SdlRenderer::renderSwitchUi() {
    // Native 720p canvas, with a 48px horizontal safe margin. All screens
    // share the same header/footer and type hierarchy; artwork stays square.
    auto fill = [&](SDL_Rect rect, SDL_Color color) {
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer_, &rect);
    };
    auto text = [&](const std::string& value, int x, int y, int width, SDL_Color color = primary, TTF_Font* font = nullptr) {
        drawFittedText(value, x, y, textCapacity(width, font), color, font);
    };
    SDL_SetRenderDrawColor(renderer_,12,15,22,255);
    SDL_RenderClear(renderer_);
    fill({0,0,1280,80}, {24,30,42,255});
    fill({0,78,1280,3}, mint);
    drawText("COVER",48,26,mint);
    drawText("PLAYER",150,26,primary);
    const char* section = "BIBLIOTHEK";
    switch (view_.screen) {
        case Screen::CoverFlow: section="COVERFLOW"; break;
        case Screen::AlbumList: section="ALBUMLISTE"; break;
        case Screen::Player: section="JETZT LAEUFT"; break;
        case Screen::Tracks: section="TITEL"; break;
        case Screen::CollectionManager: section="SAMMLUNGEN"; break;
        case Screen::CollectionType: section="TYP"; break;
        case Screen::CollectionName: section="NAME"; break;
        case Screen::CollectionDelete: section="LOESCHEN"; break;
        case Screen::Folders: section="ORDNER"; break;
        default: break;
    }
    text(tr(language_,section),420,26,530,secondary);
    if (sleepMinutes_>0) text(std::to_string(sleepMinutes_)+"m",1000,26,100,warning);
    if (batteryPercent_) {
        drawBatteryIcon(1120,35,*batteryPercent_);
        text(std::to_string(*batteryPercent_)+"%",1156,26,80,secondary);
    }

    if (view_.screen==Screen::CoverFlow) {
        if (view_.items.empty()) text(view_.message,48,310,1184,secondary);
        else renderCoverFlow();
    } else if (view_.screen==Screen::Player) {
        drawCover(48,132,400,400);
        const int x=500, width=732;
        const int titleLines=drawWrappedText(view_.title,x,132,width,50,3,primary,titleFont_);
        const int subtitleY=132+titleLines*50+20;
        const int subtitleLines=drawWrappedText(view_.subtitle,x,subtitleY,width,36,2,secondary);
        const int bookmarkY=subtitleY+subtitleLines*36+20;
        text(std::string(tr(language_,"LESEZEICHEN"))+"  "+std::to_string(bookmarkCount_),x,bookmarkY,width,secondary);
        if (!view_.message.empty()) text(view_.message,x,bookmarkY+44,width,{242,118,109,255});
        else if (!notice_.empty()) text(notice_,x,bookmarkY+44,width,warning);
        fill({48,558,1184,10},{42,49,62,255});
        if (playbackDurationSeconds_>0) {
            const double fraction=std::clamp(playbackPositionSeconds_/playbackDurationSeconds_,0.0,1.0);
            fill({48,558,static_cast<int>(1184*fraction),10},active);
        }
        drawPlaybackSymbol(640,608,playbackPaused_);
        text(timeLabel(playbackPositionSeconds_),48,592,300,secondary);
        const auto duration=playbackDurationSeconds_>0 ? timeLabel(playbackDurationSeconds_) : "--:--";
        int w=0,h=0; TTF_SizeUTF8(font_,duration.c_str(),&w,&h);
        drawText(duration.c_str(),1232-w,592,secondary);
    } else if (view_.screen==Screen::CollectionName) {
        text(tr(language_,"NAME DER SAMMLUNG"),48,104,1184,secondary);
        fill({48,150,1184,56},{31,38,50,255});
        fill({48,150,5,56},mint);
        text(view_.subtitle,68,162,1144);
        for (std::size_t i=0;i<view_.items.size();++i) {
            const int x=48+static_cast<int>(i%10)*118;
            const int y=236+static_cast<int>(i/10)*52;
            if (i==view_.selected) fill({x,y,110,46},{45,82,70,255});
            int w=0,h=0; TTF_SizeUTF8(font_,view_.items[i].c_str(),&w,&h);
            text(view_.items[i],x+std::max(8,(110-w)/2),y+7,94,i==view_.selected?primary:secondary);
        }
        text(view_.message,48,606,1184,warning);
    } else if (view_.screen==Screen::Tracks) {
        if (!view_.coverPath.empty()) drawCover(48,110,144,144);
        const int headingX=view_.coverPath.empty()?48:224;
        text(view_.title,headingX,108,1232-headingX,primary,titleFont_);
        if (view_.selected<view_.items.size())
            drawWrappedText(view_.items[view_.selected],headingX,164,1232-headingX,36,2,secondary);
        text(view_.message,headingX,244,1232-headingX,warning);
        drawSelectableList(view_.items,view_.selected,{48,1184,44,290,7,textCapacity(1152),false,true});
    } else if (view_.screen==Screen::Collections || view_.screen==Screen::AlbumList || view_.screen==Screen::CollectionManager) {
        text(view_.title,48,108,1184,primary,titleFont_);
        drawCover(48,184,304,304);
        drawSelectableList(view_.items,view_.selected,{400,832,80,184,5,textCapacity(800),true});
        text(view_.message,48,602,1184,secondary);
    } else {
        const bool hasCover=!view_.coverPath.empty();
        const int x=hasCover?400:48;
        const int width=1232-x;
        if (hasCover) drawCover(48,184,304,304);
        text(view_.title,x,108,width,primary,titleFont_);
        drawSelectableList(view_.items,view_.selected,{x,width,46,184,9,textCapacity(width-32),false});
        text(view_.message,48,602,1184,warning);
    }

    fill({0,642,1280,78},{19,23,32,255});
    const char* hint="A OEFFNEN   Y VERWALTEN   X SCANNEN";
    switch (view_.screen) {
        case Screen::CoverFlow: hint="A OEFFNEN   Y LISTE   B ZURUECK"; break;
        case Screen::AlbumList: hint="A OEFFNEN   Y COVERFLOW   B ZURUECK"; break;
        case Screen::Tracks: hint="A ABSPIELEN   B ZURUECK"; break;
        case Screen::Player: hint="A PAUSE   OBEN/UNTEN TITEL   B ZURUECK"; break;
        case Screen::Folders: hint="A OEFFNEN   Y ORDNER WAEHLEN"; break;
        case Screen::CollectionManager: hint="A EDIT   Y PFAD   X LOESCHEN"; break;
        case Screen::CollectionType: hint="A TYP WAEHLEN   B ZURUECK"; break;
        case Screen::CollectionName: hint="A ZEICHEN   Y SPEICHERN   B ABBRECHEN"; break;
        case Screen::CollectionDelete: hint="A LOESCHEN   B ABBRECHEN"; break;
        default: break;
    }
    text(tr(language_,hint),48,view_.screen==Screen::Player?646:664,960,secondary);
    if (view_.screen==Screen::Player)
        text(language_==Language::German?"PLUS KURZ SLEEP / LANG BEENDEN":"TAP PLUS SLEEP / HOLD PLUS QUIT",48,682,960,secondary);
    fill({1016,658,216,48},{37,71,61,255});
    text(language_==Language::German?"MINUS HILFE":"MINUS HELP",1028,666,192);

    if (helpVisible_) {
        fill({0,82,1280,560},{7,9,14,255});
        fill({32,96,1216,530},{19,25,34,255});
        text(tr(language_,"BEDIENUNG"),56,108,1168,mint,titleFont_);
        std::array<std::pair<const char*,const char*>,7> rows{};
        const char* note="Kreuz / linker Stick: halten = schneller";
        switch (view_.screen) {
            case Screen::Player:
                rows={{{"A","Wiedergabe / Pause"},{"LINKS/RECHTS","10 Sek. spulen"},{"L/R","30 Sek. spulen"},{"OBEN/UNTEN","Titel wechseln"},{"Y / X","Lesezeichen setzen / naechstes"},{language_==Language::German?"PLUS KURZ":"TAP PLUS","Sleep-Timer"},{"PLUS 2s",language_==Language::German?"App beenden":"Quit app"}}};
                note="Linker Stick = Steuerkreuz"; break;
            case Screen::Collections: rows={{{"A","Sammlung oeffnen"},{"STEUERKREUZ","Sammlung waehlen"},{"Y","Sammlungen verwalten"},{"X","Bibliothek scannen"},{"B","App bleibt geoeffnet"}}}; break;
            case Screen::CollectionManager: rows={{{"A","Typ und Namen bearbeiten"},{"Y","Pfad bearbeiten / neu anlegen"},{"X","Sammlung loeschen"},{"L/R","Reihenfolge verschieben"},{"B","Zurueck zur Bibliothek"}}}; break;
            case Screen::CollectionType: rows={{{"A","Sammlungstyp waehlen"},{"STEUERKREUZ","Typ auswaehlen"},{"B","Zurueck"}}}; break;
            case Screen::CollectionDelete: rows={{{"A","Sammlung entfernen"},{"B","Abbrechen"}}}; note="Mediendateien werden nie geloescht"; break;
            case Screen::CollectionName: rows={{{"A","Zeichen anfuegen"},{"X","Letztes Zeichen loeschen"},{"L","Namen komplett leeren"},{"Y","Sammlung speichern"},{"B","Abbrechen"}}}; break;
            case Screen::Tracks: rows={{{"A","Abspielen / Fortsetzen"},{"B","Zurueck zur Albumansicht"},{"STEUERKREUZ","Titel auswaehlen"}}}; note="Fortsetzbarer Titel ist vorausgewaehlt"; break;
            case Screen::Folders: rows={{{"A","Ordner oeffnen"},{"B","Zurueck"},{"Y","Diesen Ordner auswaehlen"}}}; break;
            default: rows={{{"A","Ordner / Medium oeffnen"},{"B","Eine Ebene zurueck"},{"LINKS/RECHTS","Eintrag waehlen"},{"Y","CoverFlow / Liste"},{"X","Sammlung neu scannen"}}}; break;
        }
        for (std::size_t i=0;i<rows.size() && rows[i].first;++i) {
            const int y=178+static_cast<int>(i)*52;
            if (i%2==0) fill({48,y-4,1184,48},{25,32,42,255});
            fill({56,y,244,40},{39,72,64,255});
            text(tr(language_,rows[i].first),68,y+3,220);
            text(tr(language_,rows[i].second),332,y+3,880);
        }
        text(tr(language_,note),56,554,1168,secondary);
        text(language_==Language::German?"PLUS+MINUS  Beenden (Hilfe zu)":"PLUS+MINUS  Quit (close help)",56,596,1168,warning);
        fill({0,642,1280,78},{19,23,32,255});
        text(language_==Language::German?"Y ENGLISH":"Y DEUTSCH",48,664,480,mint);
        text(language_==Language::German?"B / MINUS SCHLIESSEN":"B / MINUS CLOSE",850,664,382,secondary);
    }
    SDL_RenderPresent(renderer_);
}

} // namespace coverplayer::platform
