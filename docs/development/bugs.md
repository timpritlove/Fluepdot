# Bug-Liste (Analyse August 2026)

Status-Legende: **behoben** = im Zuge dieser Analyse gefixt,
**offen** = bekannt, bewusst noch nicht angefasst (meist Teil des geplanten
Redesigns, siehe [vorschlaege.md](vorschlaege.md)).

## Kritisch

| # | Fundort | Beschreibung | Status |
|---|---|---|---|
| F1 | `main/raw_api.c` | `memcpy(&flipdot.framebuffer->columns, ...)` schrieb auf die Adresse des `columns`-Pointers statt in den Pixelpuffer ⇒ Speicherkorruption, UDP-API funktionsunfähig | behoben |

## Treiber / Framebuffer (`components/flipdot`)

| # | Fundort | Beschreibung | Status |
|---|---|---|---|
| F3 | `framebuffer.c` (`get_pixel`, `set_pixel`) | Y-Bounds-Check `y > 16` erlaubte `y == 16` (gültig ist 0–15); bei y=16 wurde Bit 7 (= y=8) verfälscht | behoben |
| F4 | `flipdot.c` (`flipdot_cycle_internal_datastructures`) | `calloc(1, sizeof(flipdot_rendering_options_t))` für ein `framebuffer_t*` (Copy-Paste; überdimensioniert, kein Absturz) | behoben |
| F5 | `flipdot.c` (`flipdot_render`) | `FLIPDOT_ERROR_CHECK` in der Panel-Schleife kehrte bei Fehlern zurück, ohne den Mutex freizugeben ⇒ Renderer dauerhaft blockiert | behoben |
| F6 | `include/flipdot_rendering_options.h` | Kommentare behaupteten „10ms counts“ (real: 50-µs-Schritte) und vertauschten Clear-/Set-Beschreibung; Header inkludierte sich selbst | behoben |
| F20 | `flipdot.c` (`flipdot_render_column`) | Bei `skip_set` werden die Row-Treiber trotzdem ~100 µs bestromt; unnötige Puffer-Neuallokation pro Frame | offen (Optimierung, siehe Treiber-Doku) |

## CLI (`main/console_commands.c`, `main/cmd_*.c`)

| # | Fundort | Beschreibung | Status |
|---|---|---|---|
| F7 | `config_panel_layout` | Off-by-one: `panel_count >= FLIPDOT_MAX_SUPPORTED_PANELS` lehnte die dokumentierten 5 Panels ab (nur 1–4 möglich); außerdem fehlte das `return` nach Parse-Fehlern | behoben |
| F8 | `config_rendering_mode` | Bei Parse-Fehlern wurde das falsche argtable (`console_wifi_args.end`) ausgegeben | behoben |
| F9 | `framebuf64` | Speicherleck bei Größen-Mismatch; NULL-Check erst nach Dereferenzierungspfad; erwartete Größe hartkodiert 230 Bytes statt `2 × Breite` | behoben |
| F10 | `config_wifi_ap` / `config_wifi_station` | Beide Kommandos registrierten dasselbe statische argtable doppelt (Leak der ersten Allokationen) | behoben |
| F11 | `config_wifi_*`, `config_hostname`, `system_configuration.c` | `strnlen(.., sizeof)` + `strncpy`: Eingaben in Maximallänge blieben ohne NUL-Terminierung | behoben |
| F12 | `show_ip` | Kein NULL-Check auf `wifi.netif` ⇒ Absturz (`ESP_ERROR_CHECK`) bei deaktiviertem WLAN | behoben |
| F13 | `cmd_host.c` | `freeaddrinfo` nach fehlgeschlagenem `getaddrinfo` (UB); Rückgabelogik `if (ret_v6 \|\| ret_v4 == 0)` meldete Erfolg, wenn beide Lookups scheiterten | behoben |
| F14 | `cmd_traceroute.c` | `freeaddrinfo` nach Fehlschlag (UB); auf dem Erfolgspfad wurden `addrinfo`, ICMP-Puffer und Socket nie freigegeben; Socket-Erstellung ungeprüft | behoben |
| F15 | `cmd_font_rendering.c` (`render_font`) | Ergebnis von `mf_find_font` ungeprüft ⇒ NULL-Deref bei unbekanntem Fontnamen | behoben |

## HTTP (`main/httpd.c`)

| # | Fundort | Beschreibung | Status |
|---|---|---|---|
| F16 | `PUT /rendering/mode` | `buf` wurde im Erfolgsfall nicht freigegeben (Leak pro Request) | behoben |
| F17 | `POST /framebuffer/text` | `text` wurde nie freigegeben (Erfolg und Fehlerpfade); bei unbekanntem Font zusätzlich `buf`-Leak; Default-Fontname `"DejaVuSans"` existiert nicht (registriert ist `"DejaVuSans12"`) ⇒ Default schlug immer fehl | behoben |
| F18 | `GET`/`POST /rendering/timings` | GET sendete 18 Bytes pro Spalte, POST erwartete 19 ⇒ Roundtrip (GET-Ausgabe zurückposten) hing; `sscanf`-Prüfung `== 0` akzeptierte Teil-Parses | behoben |
| F21 | `httpd_initialize` | Parameter `httpd_handle_t server` wird by-value übergeben; das globale `httpd_server` in `main.c` bleibt NULL (derzeit folgenlos, da der Handle nie wieder gebraucht wird) | offen |

## Netzwerk / Sonstiges

| # | Fundort | Beschreibung | Status |
|---|---|---|---|
| F2 | `main/raw_api.c` | Nach fehlgeschlagenem `socket()`/`bind()` lief der Task trotzdem in `recvfrom` (Endlos-Fehlerschleife); Socket wurde nie geschlossen; Log-Meldung „got/expected“ vertauscht | behoben |
| F19 | `software/service_utility/main.go` | Flash-Offsets falsch: Partitionstabelle 0x800 (statt 0x8000), Bootloader 0xd000 (statt 0x1000), `ota_data_initial.bin` überschrieb denselben Offset 0xd000 | behoben |
| F22 | `main/wifi.c` | Nach dem ersten Connect wird der IP-Event-Handler deregistriert ⇒ `retry_count` wird bei späteren Reconnects nie zurückgesetzt, `STA_LOST_IP` wird nie behandelt; nach `WIFI_MAXIMUM_RETRY` läuft der Hintergrund-Reconnect trotzdem endlos | offen (Teil des Resilienz-Redesigns) |
| F23 | `main/console_commands.c` (`config_reset`) | Setzt nur den RAM-Zustand zurück; Name/Hilfetext („Factory reset“) suggerieren Persistenz | offen (Verhalten dokumentiert in usb-cli.md) |
| F24 | `components/mcufont` | Drei Fonts mit identischem `full_name` („DejaVu Sans Book 12“) ⇒ mehrdeutige Suche | erledigt (Font-Management komplett entfernt) |
| F25 | `sdkconfig.defaults` | NimBLE/Bluetooth aktiviert, obwohl kein BLE-Code existiert (Flash-/RAM-Verschwendung) | behoben (`CONFIG_BT_ENABLED=n`) |
