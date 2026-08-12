# USB-/Serielle Konsole (CLI)

## Transport

- UART0 (`CONSOLE_UART_NUM` = 0), erreichbar über den USB-Serial-Wandler
  (CP2102N, VID 0x1209 / PID 0x4223)
- 115200 Baud, 8-N-1
- ESP-IDF `esp_console` + linenoise (Zeileneditierung, History mit 6
  Einträgen, Tab-Vervollständigung)
- Setup und Registrierung aller Kommandos: `main/console.c`
  (`console_initialize`), Konsolen-Task ebd.

Der Prompt entspricht dem konfigurierten Hostnamen (`fluepdot>`).

## Semantik von Konfigurationsänderungen

Alle `config_*`-Kommandos ändern die Konfiguration nur im RAM.
Persistieren mit `config_save`, anwenden per `reboot`. Ausnahme:
`config_rendering_mode` wirkt zusätzlich sofort auf den laufenden Renderer.

## Kommandoübersicht

### System / Diagnose

| Kommando | Argumente | Verhalten | Implementierung |
|---|---|---|---|
| `help` | – | Listet alle Kommandos | ESP-IDF (`esp_console_register_help_command`) |
| `reboot` | – | `esp_restart()` | `console_commands.c` |
| `show_version` | – | IDF-Version, Chip-Infos, Flash-Größe | `console_commands.c` |
| `show_tasks` | – | FreeRTOS-Taskliste (`vTaskList`) | `console_commands.c` |
| `show_ip` | – | IPv4- und IPv6-Adressen des WLAN-Interfaces | `console_commands.c` |

### Netzwerk-Diagnose

| Kommando | Argumente | Verhalten | Implementierung |
|---|---|---|---|
| `ping` | `[-W timeout] [-i interval] [-s size] [-c count] [-Q tos] <host>` | ICMP Echo | `cmd_ping.c` |
| `host` | `<host>` | DNS-Lookup (A und AAAA) | `cmd_host.c` |
| `traceroute` | `<host>` | ICMP-Traceroute, TTL 1–31, IPv4 | `cmd_traceroute.c` |

### Konfiguration

| Kommando | Argumente | Verhalten | Implementierung |
|---|---|---|---|
| `config_save` | – | Konfiguration in die `config`-Partition schreiben | `console_commands.c` |
| `config_load` | – | Konfiguration aus Flash neu in den RAM laden (keine Re-Initialisierung der Subsysteme) | `console_commands.c` |
| `config_show` | – | Konfiguration ausgeben (inkl. WLAN-Passwort im Klartext) | `console_commands.c` |
| `config_reset` | – | Kconfig-Defaults in den RAM laden (**flasht nicht** – für einen echten Factory-Reset zusätzlich `config_save`) | `console_commands.c` |
| `config_wifi_ap` | `<ssid> [<password>]` | Modus AP + Zugangsdaten setzen | `console_commands.c` |
| `config_wifi_station` | `<ssid> [<password>]` | Modus Station + Zugangsdaten setzen | `console_commands.c` |
| `config_hostname` | `<hostname>` | Hostname setzen (max. 31 Zeichen) | `console_commands.c` |
| `config_panel_layout` | `<panel_size>` × 1–5 | Panelanzahl und -breiten setzen | `console_commands.c` |
| `config_rendering_mode` | `full` \| `differential` | Default-Rendering-Modus setzen; wirkt sofort und wird (nach `config_save`) persistiert | `console_commands.c` |

Nicht per CLI konfigurierbar: WLAN-Modus „disabled“ (nur durch
`config_reset` + `config_save` erreichbar, sofern der Kconfig-Default
„disabled“ ist), sämtliche RS485-Felder, Rendering-Timings, Panel-Reihenfolge,
HTTP-/UDP-Ports.

### Framebuffer / Anzeige

| Kommando | Argumente | Verhalten | Implementierung |
|---|---|---|---|
| `flipdot_clear` | `[--invert]` | Alle Pixel löschen bzw. mit `--invert` alle setzen; setzt Dirty-Flag | `console_commands.c` |
| `framebuf64` | `<base64>` | Base64-dekodierter Roh-Framebuffer (`2 × Breite` Bytes, gleiche Spaltenkodierung wie UDP-API) wird übernommen; setzt Dirty-Flag | `console_commands.c` |
| `show_fonts` | – | Listet einkompilierte mcufont-Fonts | `cmd_font_rendering.c` |
| `render_font` | `[-x <int>] [-y <int>] [-f <font>] <text>` | Rendert Text in den Framebuffer (ohne vorheriges Löschen); setzt Dirty-Flag | `cmd_font_rendering.c` |

## Abweichungen zur offiziellen Doku

`docs/source/command_line.rst` dokumentierte ursprünglich weder `show_ip`
noch `config_rendering_mode` noch `framebuf64` (inzwischen ergänzt).
`getting_started.rst` behauptet, es gebe CLI-Kommandos zum Ändern der
Panel-Reihenfolge – **das ist falsch**, eine solche Schnittstelle existiert
nirgends in der Firmware.
