#include "desktop.h"
#include <string.h>
const char *desktop_text(const char *text) {
    static const char *map[][2] = {
        {"ISLAND CITY / V0.5", "WINDOWS / V0.5"},
        {"LEFT/RIGHT ROTATE / X RESET VIEW", "LEFT/RIGHT ROTATE / X RESET VIEW"},
        {"CHOOSE A GARAGE CATEGORY BELOW", "CHOOSE A GARAGE CATEGORY"},
        {"A GAS / B BRAKE / R DRIFT / L SMALL NITRO BOOST",
         "W GAS / S BRAKE / SPACE DRIFT / SHIFT NITRO"},
        {"TOUCH FOR CITY MAP", "CLICK MAP / M CITY MAP"},
        {"TOUCH LAND", "CLICK LAND"},
        {"A/B BACK / X CLEAR / Y BONUS ROUTE", "ESC BACK / X CLEAR / R BONUS ROUTE"},
        {"MAP: Y SET ROUTE", "MAP: R SET ROUTE"},
        {"A GAS / B BRAKE / R DRIFT / L BOOST", "W GAS / S BRAKE / SPACE DRIFT / SHIFT BOOST"},
        {"START PAUSE / SELECT SETTINGS", "ESC PAUSE / F2 GRAPHICS / M MAP"},
        {"D-PAD CHOOSE / A OPEN / B BACK", "ARROWS CHOOSE / ENTER OPEN / ESC BACK"},
        {"B CATEGORIES    A FIT / UPGRADE", "ESC CATEGORIES / ENTER FIT"},
        {"B CATEGORIES    A BUY / PREVIEW", "ESC CATEGORIES / ENTER BUY"},
        {"B CANCEL", "ESC CANCEL"},
        {"A BUY + FIT", "ENTER BUY"},
        {"LEFT/RIGHT CHANGE / B BACK", "LEFT/RIGHT CHANGE / ESC BACK"},
        {"CIRCLE PAD OR D-PAD: STEER", "A/D OR LEFT/RIGHT: STEER"},
        {"A: GAS / B: BRAKE THEN REVERSE", "W: GAS / S: BRAKE THEN REVERSE"},
        {"R: DRIFT / L+A: SMALL NITRO BOOST", "SPACE: DRIFT / SHIFT+W: NITRO"},
        {"X: CAMERA / HOLD Y: ROAD RECOVERY", "X: CAMERA / HOLD R: ROAD RECOVERY"},
        {"START: PAUSE / SELECT: SETTINGS", "ESC: PAUSE / F2: GRAPHICS"},
        {"TOUCH RADAR: MAP AND ROUTE PLANNER", "M: MAP / TAB: DASHBOARD / F11: FULL"},
        {"A/B BACK", "ENTER / ESC BACK"},
        {"D-PAD / CIRCLE PAD / TOUCH / A SELECT", "ARROWS / CLICK / ENTER SELECT"},
        {"QUIT TO HOMEBREW", "QUIT TO DESKTOP"},
        {"L/R CATEGORY", "Q/E CATEGORY"}};
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (!strcmp(text, map[i][0]))
            return map[i][1];
    return text;
}
