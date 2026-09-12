# DeskBuddy pro ESP32-C3 SuperMini

- **ESP32-C3 SuperMini** 
- **SH1106 OLED 128×64**, I²C adresa `0x3C`
- zapojení podle aktuálního potvrzení: `SDA → GPIO8`, `SCL → GPIO9`
- **Touch / Button**, → GPIO7

## Co firmware dělá

- animovaná stránka s očima,
- hodiny z NTP,
- aktuální počasí a **3denní předpověď** z OpenWeatherMap, pokud je v lokálním nastavení API klíč,
- po 20 s bez stisku návrat z hodin/počasí/předpovědi na oči,
- po 30 minutách bez aktivity zhasne OLED a ESP32 přejde do **light sleep**; vždy při celé hodině se na 30 s ukážou hodiny a pak znovu usne,
- **tlačítko na GPIO7**: krátký stisk přepíná oči → hodiny → aktuální počasí → 3denní předpověď; podržení 3 s otevře znovu lokální Wi‑Fi setup portal.
- lokální Wi‑Fi setup portal, pokud nejsou uložené funkční údaje.


## První spuštění

Po uploadu vznikne Wi‑Fi síť:

- síť: `DeskBuddy-C3-Setup`
- heslo: `deskbuddy`
- otevři v telefonu: `http://192.168.4.1`

Zadej vlastní Wi‑Fi. Klíč OpenWeatherMap je volitelný; bez něj fungují oči, hodiny a status, pouze stránka počasí vypíše, že klíč chybí.

## Arduino IDE

1. Otevři `deskbuddy_c3_oled/deskbuddy_c3_oled.ino`.
2. Vyber desku **ESP32C3 Dev Module**.
3. Vyber port a podle dostupných voleb nastav **USB CDC On Boot: Enabled**, Flash Mode **DIO**, Flash Size **4 MB**.
4. V Library Manageru nainstaluj: `ArduinoJson`, `Adafruit GFX Library`, `Adafruit SH110X`.
5. Nahraj a otevři Serial Monitor na **115200 baud**.

## Elektrické zapojení OLED

| OLED | ESP32-C3 SuperMini |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |
| tlačítko | GPIO7 → GND (interní pull-up) |

Před napájením ověř popisky VCC/GND na skutečném OLED modulu. Firmware předpokládá běžné tlačítko mezi GPIO7 a GND (interní pull-up). Pokud po uploadu tlačítko hlásí stisk trvale nebo opačně, změň v `config.h.example` `BUTTON_ACTIVE_LOW` na `false`.

Přidána verze s touch ttp223 každá verze má svojí složku
## Zapojení

| TTP223 | ESP32-C3 SuperMini |
|---|---|
| VCC | **3V3** |
| GND | GND |
| OUT | GPIO7 |

