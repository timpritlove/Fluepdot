# Architektur

## Projektstruktur

```
software/firmware/
├── main/                    # Applikationscode (ESP-IDF-Komponente "main")
│   ├── main.c               # app_main, Boot-Ablauf
│   ├── system_configuration.c/.h  # Persistente Konfiguration (eigene Flash-Partition)
│   ├── console.c            # Konsolen-Setup (esp_console + linenoise), Konsolen-Task
│   ├── console_commands.c   # Die meisten CLI-Kommandos
│   ├── cmd_ping.c / cmd_host.c / cmd_traceroute.c  # Netzwerk-Diagnose-Kommandos
│   ├── httpd.c              # HTTP-API (esp_http_server)
│   ├── raw_api.c            # UDP-Raw-API (Port 1337)
│   ├── wifi.c               # WLAN AP/Station
│   ├── mdns_util.c          # mDNS-Announcement
│   ├── base64.c, crc16.c, net_util.c  # Hilfsfunktionen
│   └── Kconfig.projbuild    # Kconfig-Defaults für die Werkskonfiguration
├── components/
│   └── flipdot/             # Flipdot-Treiber (Framebuffer, Rendering, SPI/GPIO)
├── partitions.csv           # Eigene Partitionstabelle
└── sdkconfig.defaults       # Build-Defaults
```

Verwaltete Abhängigkeit: `espressif/mdns` (über `idf_component.yml` /
`managed_components`).

## Boot-Ablauf (`main/main.c`, `app_main`)

1. `initialize()`: NVS-Init (nur für ESP-IDF-intern, z. B. WiFi/PHY-Kalibrierung;
   die App-Konfiguration liegt **nicht** in NVS), `esp_netif_init`,
   Default-Event-Loop.
2. `system_configuration_load(&system_configuration)` – lädt den
   Konfigurations-Blob aus der Partition `config` (siehe
   [konfiguration.md](konfiguration.md)). Bei CRC-Fehler werden die
   Kconfig-Defaults geflasht. Fehler hier sind fatal (`ESP_ERROR_CHECK`).
3. `console_initialize(...)` – registriert alle CLI-Kommandos und startet den
   Konsolen-Task.
4. `wifi_initialize(...)` – je nach Konfiguration Disabled / AP / Station.
   Im Station-Modus **blockiert** der Aufruf, bis die Verbindung steht oder
   `WIFI_MAXIMUM_RETRY` (3) Fehlversuche erreicht sind.
5. `mdns_initialize(hostname)` – Announced `_http._tcp` auf Port 80.
6. `// TODO initialize wired connections` – RS485/Flipnet/DMX ist nur ein
   Platzhalter, es existiert kein Code.
7. `flipdot_initialize(...)` + `flipdot_set_power(&flipdot, true)` – Treiber
   und Rendering-Task starten, 12-V-Versorgung der Panels einschalten.
8. `httpd_initialize(...)` – HTTP-Server auf Port 80.
9. `raw_api_initialize()` – UDP-Task auf Port 1337.
10. Endlosschleife mit `vTaskDelay`.

Ab Schritt 4 werden Fehler nur geloggt (`ERROR_SHOW`), der Boot läuft weiter –
ein Gerät ohne WLAN bootet also trotzdem und bleibt per USB erreichbar.

## FreeRTOS-Tasks

| Task | Stack | Priorität | Quelle | Zweck |
|---|---|---|---|---|
| `console` | 4096 | 8 | `console.c` | linenoise-REPL auf UART0 |
| `flipdot_task` | 12288 | 8 | `components/flipdot/flipdot.c` | Wartet auf Dirty-Bit, rendert Framebuffer auf die Hardware |
| `raw_api` | 4096 | 8 | `raw_api.c` | UDP-Empfang, kopiert Frames in den Framebuffer |
| httpd-Tasks | (IDF-Default) | (IDF-Default) | `esp_http_server` | HTTP-Request-Handling |

Inter-Task-Kommunikation zum Renderer läuft über eine FreeRTOS Event Group
(`FLIPDOT_FRAMEBUFFER_DIRTY_BIT`, `FLIPDOT_RENDERING_DONE_BIT`) plus einen
Mutex (`flipdot.semaphore`) für den Hardware-Zugriff.

## Partitionstabelle (`partitions.csv`)

| Name | Typ | SubTyp | Offset | Größe | Zweck |
|---|---|---|---|---|---|
| `nvs` | data | nvs | 0x9000 | 16K | ESP-IDF-intern (WiFi-Kalibrierung etc.) |
| `otadata` | data | ota | 0xd000 | 8K | OTA-Auswahldaten – **ungenutzt**, da keine OTA-Slots existieren |
| `phy_init` | data | phy | 0xf000 | 4K | PHY-Kalibrierung |
| `factory` | app | factory | 0x10000 | 3M | Die Firmware selbst (einziger App-Slot) |
| `config` | 0x42 | 0x23 | 0x310000 | 4K | Fluepdot-Systemkonfiguration (eigener Typ/Subtyp) |

Es gibt **keine** `ota_0`/`ota_1`-Partitionen – netzwerkbasierte OTA-Updates
sind mit dieser Tabelle nicht möglich (siehe
[firmware-update.md](firmware-update.md)).

## Relevante sdkconfig.defaults

- `CONFIG_PARTITION_TABLE_CUSTOM=y` mit `partitions.csv`
- `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` + Stats-Formatting (für `show_tasks`)
- 4 MB Flash (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`)
- `CONFIG_BT_ENABLED=y` / NimBLE aktiviert – **es gibt aber keinen BLE-Code
  in der Applikation**; das kostet nur Flash/RAM
- `CONFIG_LWIP_SNMP_SUPPORT` ist auskommentiert – SNMP ist nicht im Build
