# Font-Management (Ist-Zustand und Entfernungsplan)

## Ist-Zustand

Die Firmware bündelt den Font-Renderer **mcufont** als vendored Komponente:
`software/firmware/components/mcufont/` (Decoder in `mcufont/decoder/`).

Die Fonts sind **fest in das App-Image einkompiliert** (kein Dateisystem,
keine eigene Partition): `mf_font.c` inkludiert über `MF_FONT_FILE_NAME`
die generierte `fonts.h`, die wiederum die Font-Blobs als C-Arrays einbindet.

Einkompilierte Fonts (ca. **43 KB Flash** gesamt, RAM-Verbrauch
vernachlässigbar, da `const`-Daten):

| Kurzname | Voller Name |
|---|---|
| `DejaVuSans12` | DejaVu Sans Book 12 |
| `DejaVuSans12bw` | DejaVu Sans Book 12 |
| `DejaVuSans12bw_bwfont` | DejaVu Sans Book 12 |
| `DejaVuSerif16` | DejaVu Serif Book 16 |
| `DejaVuSerif32` | DejaVu Serif Book 32 |
| `fixed_5x8` | Misc-Fixed 8 |
| `fixed_7x14` | Misc-Fixed 14 |
| `fixed_10x20` | Misc-Fixed 20 |

Hinweis: Die drei DejaVu-Sans-12-Varianten haben identische `full_name`s –
`mf_find_font` über den vollen Namen ist mehrdeutig.

## Nutzer des Font-Codes

| Schnittstelle | Was |
|---|---|
| CLI `show_fonts` | Fontliste (`main/cmd_font_rendering.c`) |
| CLI `render_font` | Text rendern (`main/cmd_font_rendering.c`) |
| HTTP `GET /fonts` | Fontliste (`main/httpd.c`) |
| HTTP `POST /framebuffer/text` | Framebuffer löschen + Text rendern (`main/httpd.c`) |
| Glue | `main/font_rendering.c` / `main/include/font_rendering.h` (Pixel-Callback, spiegelt Y: `15 - y`) |

Der Framebuffer-/Pixel-/UDP-Pfad hat **keine** Abhängigkeit auf Fonts.

Toter Code: `main/font_rendinger_old.c` ist nicht in `main/CMakeLists.txt`
eingebunden (verwaiste Datei, Tippfehler im Namen inklusive).

## Entfernungsplan

Ziel laut Projektentscheidung: Font-Management komplett aus der Firmware
entfernen und sich auf direkte, effiziente Framebuffer-Manipulation
konzentrieren (Text-Rendering übernimmt künftig der Client).

Zu entfernende Teile:

1. Komponente `software/firmware/components/mcufont/` komplett.
2. `main/font_rendering.c`, `main/include/font_rendering.h`.
3. `main/cmd_font_rendering.c`, `main/include/cmd_font_rendering.h`.
4. `main/font_rendinger_old.c` (ohnehin toter Code).
5. In `main/CMakeLists.txt`: Einträge `cmd_font_rendering.c`,
   `font_rendering.c`.
6. In `main/console.c`: `#include "cmd_font_rendering.h"`,
   `console_register_show_fonts()`, `console_register_render_font()`.
7. In `main/httpd.c`: Handler + URI-Registrierungen für `GET /fonts` und
   `POST /framebuffer/text`, `#include "font_rendering.h"`.
8. In `main/console_commands.c`: `#include "font_rendering.h"` (wird dort
   nicht wirklich gebraucht).
9. Doku: Font-Abschnitte in `docs/source/http_api.rst` und
   `docs/source/command_line.rst` entfernen bzw. als entfernt markieren;
   diese Datei und [http-api.md](http-api.md)/[usb-cli.md](usb-cli.md)
   aktualisieren.

Erwarteter Gewinn: ~43 KB Flash für Fontdaten plus Decoder-Code
(einige KB), zwei HTTP-Endpoints und zwei CLI-Kommandos weniger
Angriffs-/Wartungsfläche.

Migrationshinweis für Nutzer: Text-Rendering clientseitig erledigen und
das Ergebnis als Framebuffer über `POST /framebuffer` (ASCII) oder die
UDP-Raw-API (binär) schicken.
