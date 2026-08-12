# HTTP-API

## Server

- ESP-IDF `esp_http_server`, gestartet in `main/httpd.c`
  (`httpd_initialize`)
- Port: **80**, hartkodiert über `HTTPD_DEFAULT_CONFIG()` – nicht
  konfigurierbar
- **Keine Authentifizierung**
- CORS: jede Antwort trägt `Access-Control-Allow-Origin: *`
- mDNS announced `_http._tcp` auf Port 80 (`main/mdns_util.c`)

## Framebuffer-Encoding (Wire-Format)

ASCII-Zeilen, nur drei Zeichen erlaubt:

- `X` (0x58) = Pixel gesetzt (hell)
- Leerzeichen (0x20) = Pixel gelöscht (dunkel)
- `\n` (0x0A) = Zeilenende (kein CR)

Jede Zeile hat exakt `Breite` Zeichen plus `\n`. Es gibt exakt 16 Zeilen.
Die Zeilen werden von **y=15 (oben) bis y=0 (unten)** übertragen – der
Framebuffer-Ursprung (0,0) liegt unten links.

## Endpoints

Alle Handler in `main/httpd.c`.

| Methode | URI | Request | Response | Bemerkung |
|---|---|---|---|---|
| GET | `/framebuffer` | – | Framebuffer im o. g. ASCII-Format (chunked) | Auch zur Geometrie-Ermittlung nutzbar |
| POST | `/framebuffer` | Body: kompletter ASCII-Framebuffer | wie GET nach Übernahme | Setzt Dirty-Flag |
| GET | `/pixel` | Query `x`, `y` (dezimal) | 1 Zeichen (`X`/Leerzeichen) + NUL | |
| POST | `/pixel` | Query `x`, `y` | kodierter Pixel | Setzt Pixel auf hell, Dirty-Flag |
| DELETE | `/pixel` | Query `x`, `y` | kodierter Pixel | Setzt Pixel auf dunkel, Dirty-Flag |
| GET | `/rendering/mode` | – | ASCII-Integer + `\n` (0=FULL, 1=DIFFERENTIAL) | |
| PUT | `/rendering/mode` | Body: ASCII-Integer (max. 10 Bytes) | wie GET | Wirkt sofort; wird **nicht** in `system_configuration` übernommen und übersteht keinen Reboot |
| GET | `/rendering/timings` | – | pro Spalte 3 Zeilen `%05d\n` (pre/clear/set, Einheit 50 µs) | |
| POST | `/rendering/timings` | Body: gleiches Format | wie GET | Nicht persistent |
| GET | `/rendering/wait` | – | `ok` oder HTTP 500 | Wartet bis zu 3 s auf `FLIPDOT_RENDERING_DONE_BIT` |

Entfernt (August 2026): `GET /fonts` und `POST /framebuffer/text` – das
Font-Management wurde komplett aus der Firmware entfernt (siehe
[fonts.md](fonts.md)). Text-Rendering erfolgt clientseitig, das Ergebnis wird
per `POST /framebuffer` geschickt.

## Timings-Format

Pro Spalte exakt drei Zeilen, jede mit fünf Dezimalziffern (mit Nullen
aufgefüllt) plus `\n`:

1. `pre_delay` – Wartezeit vor dem Rendern der Spalte
2. `clear_delay` – Bestromungsdauer des Column-Clear-Treibers
3. `set_delay` – Bestromungsdauer der Row-Treiber

Einheit: **50-µs-Schritte**. Default ist 160 ⇒ **8000 µs** pro Clear- bzw.
Set-Puls. (Die offizielle Doku behauptete fälschlich 1600 µs als Default.)

## Bekannte Eigenheiten

- `PUT /rendering/mode` ändert nur die Laufzeit-Option. Der beim Boot
  geladene Default kommt aus `system_configuration.flipdot.rendering_mode`
  (nur per CLI `config_rendering_mode` + `config_save` änderbar).
- `POST /framebuffer/text` löscht immer den gesamten Framebuffer, bevor der
  Text gerendert wird.
- Es gibt keinen Endpoint für die Panel-Reihenfolge (`panel_order`), keinen
  für die Konfiguration (Hostname, WLAN) und keinen für Firmware-Updates.
