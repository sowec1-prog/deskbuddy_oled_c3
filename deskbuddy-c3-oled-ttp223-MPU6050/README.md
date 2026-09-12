# DeskBuddy — TTP223 + MPU-6050 varianta

Samostatná varianta DeskBuddy pro:

- kapacitní tlačítko **TTP223** v režimu **Jog / momentary**,
- gyroskop + akcelerometr **GY-521 / MPU-6050**,
- OLED SH1106, počasí, třídenní předpověď a light sleep.

Původní DeskBuddy ani varianta pouze s TTP223 nejsou měněny.

## Zapojení

| Modul | Pin | ESP32-C3 SuperMini |
|---|---|---|
| OLED SH1106 | VCC / GND | 3V3 / GND |
| OLED SH1106 | SDA / SCL | GPIO8 / GPIO9 |
| TTP223 | VCC / GND | 3V3 / GND |
| TTP223 | OUT | GPIO7 |
| GY-521 MPU-6050 | VCC / GND | **3V3 / GND** |
| GY-521 MPU-6050 | SDA / SCL | GPIO8 / GPIO9 |

TTP223 a MPU-6050 musí být napájeny z **3,3 V**. OLED i MPU-6050 sdílí I²C sběrnici GPIO8/GPIO9. Piny `XDA`, `XCL`, `AD0` a `INT` MPU-6050 se pro tuto variantu nezapojují.

## Funkce MPU-6050

Firmware čte akcelerometr každých 80 ms. Změna zrychlení větší než `0,55 g` je vyhodnocena jako zatřesení:

- přepne se na obrazovku očí,
- oči se krátce podívají do stran,
- obnoví se 30minutový časovač neaktivity.

Citlivost lze doladit v `config.h.example` přes `MPU6050_SHAKE_DELTA_G`. Menší číslo reaguje citlivěji.

MPU-6050 je v této verzi čtený během běžného provozu. Light sleep stále probouzí TTP223 na GPIO7; zatřesení samo zařízení z light sleep nebudí, protože pin `INT` není zapojený.

## Arduino IDE

Otevři `deskbuddy_ttp223_mpu6050/deskbuddy_ttp223_mpu6050.ino`, zvol **ESP32C3 Dev Module**, USB CDC On Boot Enabled, Flash Mode DIO, Flash Size 4 MB. Potřebné knihovny: `ArduinoJson`, `Adafruit GFX Library`, `Adafruit SH110X`.

`config.h.example` je bez tajných údajů. Lokální `config.h` zůstává pouze na tomto počítači.
