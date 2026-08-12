# Konfiguration

## Speicherformat

Die Systemkonfiguration wird **nicht** in NVS gespeichert, sondern als ein
einziger gepackter Binär-Blob in einer eigenen 4-KB-Flash-Partition:

| Eigenschaft | Wert | Quelle |
|---|---|---|
| Partitionslabel | `config` | `main/include/system_configuration.h` |
| Typ / Subtyp | `0x42` / `0x23` | ebd., `partitions.csv` |
| Offset / Größe | `0x310000` / 4 KB | `partitions.csv` |
| Format | Ein `system_configuration_t` (packed) ab Offset 0 | `main/system_configuration.c` |
| Integrität | CRC16 über die Struktur (Checksummen-Feld dabei auf 0 gesetzt) | ebd. |

Ablauf:

- **Laden** (`system_configuration_load`): Partition lesen, CRC prüfen. Bei
  CRC-Fehler (z. B. Erstinbetriebnahme oder Strukturänderung) werden die
  Kconfig-Defaults geladen **und sofort geflasht**.
- **Speichern** (`system_configuration_save`): gesamte Partition löschen,
  CRC berechnen, Struktur schreiben.
- Es gibt **keine Versionierung** des Blobs. Jede Änderung am Layout von
  `system_configuration_t` invalidiert die gespeicherte Konfiguration
  (CRC-Fehler → Factory-Reset beim nächsten Boot).

## Anwendungsmodell

CLI-Kommandos ändern nur die Konfiguration **im RAM**. Wirksam wird eine
Änderung erst durch `config_save` + `reboot`. Einzige Ausnahme:
`config_rendering_mode` setzt zusätzlich sofort den Live-Rendering-Modus.

## Struktur `system_configuration_t`

Definiert in `main/include/system_configuration.h` (alle Strukturen
`__attribute__((packed))`):

| Feld | Typ | Default (Kconfig) | Wirkung |
|---|---|---|---|
| `hostname` | `char[32]` | `"fluepdot"` | Netif-Hostname, mDNS-Hostname, Konsolen-Prompt |
| `wifi.mode` | enum (0=disabled, 1=AP, 2=Station) | `WIFI_DISABLED` | WLAN-Betriebsmodus |
| `wifi.ssid` | `char[32]` | `"Fluepdot"` | SSID (AP oder Station) |
| `wifi.password` | `char[64]` | `""` | WPA-PSK; leer ⇒ offener AP |
| `rs485.mode` | enum (0=disabled, 1=Flipnet, 2=DMX) | `RS485_DISABLED` | **Toter Code** – wird gespeichert und angezeigt, aber nie benutzt |
| `rs485.address` | `uint8_t` | 1 (nur Flipnet) | ungenutzt |
| `rs485.baudrate` | `unsigned long` | – (Kconfig ohne Default) | ungenutzt |
| `flipdot.panel_count` | `uint8_t` | 5 | Anzahl Panels (max. 5) |
| `flipdot.panel_size[0..4]` | `uint8_t[5]` | 25, 25, 20, 20, 25 | Panelbreiten in Pixel; Summe = Displaybreite |
| `flipdot.rendering_mode` | enum (0=FULL, 1=DIFFERENTIAL) | 0 (hartkodiert in `system_configuration.c`, kein Kconfig) | Default-Rendering-Modus beim Boot |
| `checksum` | `uint16_t` | berechnet | CRC16 |

## Kconfig-Defaults (`main/Kconfig.projbuild`)

Menü „Flipdot Firmware Defaults“ – diese Werte dienen nur als
Werkskonfiguration, die beim ersten Boot (oder nach CRC-Fehler) in die
Partition geschrieben wird:

- `DEFAULT_SYSTEMCONFIGURATION_WIFI_MODE_{DISABLED,AP,STATION}` (choice, Default: disabled)
- `DEFAULT_SYSTEMCONFIGURATION_WIFI_SSID` (Default `"Fluepdot"`)
- `DEFAULT_SYSTEMCONFIGURATION_WIFI_PASSWORD` (Default `""`)
- `DEFAULT_SYSTEMCONFIGURATION_RS485_MODE_{DISABLED,FLIPNET,DMX}` (choice, Default: disabled)
- `DEFAULT_SYSTEMCONFIGURATION_RS485_BAUDRATE` (int, **kein Default**, nur bei Flipnet)
- `DEFAULT_SYSTEMCONFIGURATION_RS485_ADDRESS` (int 0–255, Default 1, nur bei Flipnet)
- `DEFAULT_SYSTEMCONFIGURATION_FLIPDOT_PANEL_COUNT` (int 1–5, Default 5)
- `DEFAULT_SYSTEMCONFIGURATION_FLIPDOT_PANEL_SIZE1..5` (int 20–25, Defaults 25/25/20/20/25)
- `DEFAULT_SYSTEMCONFIGURATION_HOSTNAME` (Default `"fluepdot"`)

## Was NICHT persistiert wird

Diese Laufzeit-Einstellungen gehen bei jedem Reboot verloren:

- **Rendering-Timings** (`pre_delay`, `clear_delay`, `set_delay` pro Spalte) –
  nur via HTTP `POST /rendering/timings` änderbar
- **Panel-Reihenfolge** (`panel_order`) – es existiert überhaupt keine
  Schnittstelle, um sie zu ändern (weder CLI noch HTTP)
- **HTTP-Port** (80) und **UDP-Port** (1337) – hartkodiert
- Der Framebuffer-Inhalt

## Sicherheit

- `config_show` gibt das WLAN-Passwort im **Klartext** aus (dokumentiertes,
  bewusstes Verhalten).
- HTTP-API und UDP-API sind vollständig **unauthentifiziert**; CORS ist mit
  `Access-Control-Allow-Origin: *` offen.

Vorschläge zur Vereinheitlichung und Absicherung: siehe
[vorschlaege.md](vorschlaege.md).
