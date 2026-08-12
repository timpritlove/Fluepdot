# Fluepdot Firmware – Entwickler-Dokumentation (Status quo)

Diese Dokumentation beschreibt den **Ist-Zustand** der Fluepdot-Firmware
(`software/firmware`) aus Entwicklersicht. Sie dient als Grundlage für die
geplante Überarbeitung (Konfigurations-Redesign, Font-Entfernung, OTA etc.)
und ist unabhängig von der offiziellen Nutzer-Dokumentation unter
`docs/source/` (Sphinx, Englisch).

Stand der Analyse: August 2026, Branch `master`.

## Inhalt

| Datei | Inhalt |
|---|---|
| [architektur.md](architektur.md) | Boot-Ablauf, Tasks, Komponenten, Partitionstabelle |
| [konfiguration.md](konfiguration.md) | Speicherformat, alle Konfigurationsfelder, Kconfig-Defaults |
| [usb-cli.md](usb-cli.md) | Alle Kommandos der seriellen Konsole |
| [http-api.md](http-api.md) | Alle HTTP-Endpoints, Framebuffer-Encoding |
| [udp-protokoll.md](udp-protokoll.md) | UDP-Raw-API auf Port 1337 |
| [flipdot-treiber.md](flipdot-treiber.md) | Hardware-Ansteuerung, Rendering-Pipeline, Parallelisierungs-Analyse |
| [fonts.md](fonts.md) | Font-Management (mcufont) und Entfernungsplan |
| [firmware-update.md](firmware-update.md) | Flash-Vorgang, Tools, OTA-Bewertung |
| [bugs.md](bugs.md) | Gefundene Bugs mit Status |
| [vorschlaege.md](vorschlaege.md) | Vorschläge: Konfigurations-Vereinheitlichung, HTTP-Auth, Ports |

## Kurzüberblick

Die Firmware läuft auf einem ESP32 (Fluepboard) und steuert bis zu 5
BVG-Flipdot-Panels (Höhe fix 16 Pixel, Breite je Panel 20–25 Pixel) über
eine SPI-getriebene Schieberegisterkette an.

Schnittstellen im aktuellen Build:

- **USB/UART-Konsole** (115200 8-N-1): Konfiguration, Diagnose, Framebuffer
- **HTTP-API** (Port 80, hartkodiert, keine Authentifizierung)
- **UDP-Raw-API** (Port 1337, hartkodiert, binäres Framebuffer-Push)
- **mDNS** (`_http._tcp` auf Port 80)

In der offiziellen Doku erwähnt, aber im aktuellen Build **nicht vorhanden**:
SNMP, Bluetooth LE, RS485/Flipnet/DMX (nur Konfigurationsfelder, kein Code),
OTA-Updates.
