# DeskBuddy — varianta s kapacitním tlačítkem TTP223

Tato složka je samostatná varianta stávajícího DeskBuddy. Všechny obrazovky, počasí, třídenní předpověď, 20sekundový návrat na oči a light sleep zůstávají stejné.

Změna je pouze vstupní tlačítko: TTP223 v režimu **Jog / momentary**.

## Zapojení

| TTP223 | ESP32-C3 SuperMini |
|---|---|
| VCC | **3V3** |
| GND | GND |
| OUT | GPIO7 |

Nepřipojuj VCC na 5 V: výstup OUT by pak mohl mít 5 V a poškodit GPIO7.

## Důležité nastavení modulu

Použij **Jog / momentary**, nikoli **self-locking**. Výstup OUT musí být HIGH jen po dobu dotyku. Varianta má proto v `config.h.example` nastaveno:

```cpp
#define BUTTON_ACTIVE_LOW false
```

TTP223 při dotyku přivede na GPIO7 logickou 1. Krátký dotyk přepne stránku; dotyk delší než 3 sekundy otevře lokální Wi-Fi nastavení. V light sleep dotyk GPIO7 zařízení probudí; tento první dotyk pouze probouzí displej, nepřepíná stránku.

## Arduino IDE

1. Otevři `deskbuddy_ttp223/deskbuddy_ttp223.ino`.
2. Zvol **ESP32C3 Dev Module** a port zařízení.
3. Nastav USB CDC On Boot: Enabled, Flash Mode: DIO, Flash Size: 4 MB.
4. Musí být nainstalované knihovny `ArduinoJson`, `Adafruit GFX Library` a `Adafruit SH110X`.

Lokální `config.h` je určen jen pro tento počítač a obsahuje stávající lokální konfiguraci. `config.h.example` zůstává bez přihlašovacích údajů.
