# Firmware-Update

## Kurzfassung

Firmware-Updates sind derzeit **nur seriell (USB)** möglich – entweder mit
`esptool.py` oder mit dem Go-basierten `service_utility`. Ein Update über
TCP/IP (OTA) ist mit dem aktuellen Stand **nicht** möglich, wäre aber mit
überschaubarem Aufwand nachrüstbar (siehe unten).

## Weg 1: esptool.py

Benötigt: Python + [esptool](https://github.com/espressif/esptool), die
gebauten Binärdateien (`bootloader.bin`, `partition-table.bin`,
`flipdot-firmware.bin`).

```bash
esptool.py --chip esp32 --before=default_reset --after=hard_reset \
    write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
    0x1000  bootloader/bootloader.bin \
    0x8000  partition_table/partition-table.bin \
    0x10000 flipdot-firmware.bin
```

Offsets (Standard-ESP-IDF-Layout, passend zu `partitions.csv`):

| Offset | Inhalt |
|---|---|
| 0x1000 | Second-Stage-Bootloader |
| 0x8000 | Partitionstabelle |
| 0xd000 | `otadata` (nur nötig, wenn initialisiert werden soll: `ota_data_initial.bin`) |
| 0x10000 | Applikation (factory) |

## Weg 2: service_utility

`software/service_utility/` ist ein eigenständiges Go-Programm, das die
Binärdateien per `go-bindata` einbettet und über
`github.com/fluepke/esptool` seriell flasht:

```bash
./service_utility -serial.port /dev/ttyUSB0
```

Ablauf: Port mit 115200 öffnen → bis zu 3 Verbindungsversuche →
Baudrate auf 921600 erhöhen → Partitionstabelle, Bootloader, App und
`ota_data_initial.bin` schreiben.

**Achtung, historischer Bug:** Bis zur Korrektur im August 2026 flashte das
Utility mit falschen Offsets (Partitionstabelle nach 0x800 statt 0x8000,
Bootloader nach 0xd000 statt 0x1000, und `ota_data_initial.bin`
überschrieb anschließend denselben Offset 0xd000). Die Offsets in
`main.go` sind inzwischen korrigiert; bereits ausgelieferte Binaries des
Utilities sind unbrauchbar und müssen neu gebaut werden
(`go-bindata` + `go build`, siehe Kommentar am Dateianfang von `main.go`).

## Weg 3: Docker-Build + idf.py/make flash

Für Entwickler: Das Repo enthält `Dockerfile` (Build-Umgebung mit ESP-IDF)
und `Dockerfile.build`. Alternativ lokal mit installiertem ESP-IDF:

```bash
cd software/firmware
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

## OTA über TCP/IP: Bewertung

Aktueller Stand:

- Es existiert nur ein verwaister Header `main/include/ota.h`
  (deklariert `simple_ota_task`), ohne zugehörige `.c`-Datei im Build –
  Überrest einer frühen Entwicklungsphase.
- Die Partitionstabelle enthält zwar `otadata` (0xd000), aber **nur einen
  `factory`-Slot (3 MB)** und keine `ota_0`/`ota_1`-Partitionen.
  `esp_https_ota` kann damit nicht arbeiten.

Was für OTA nötig wäre:

1. **Partitionstabelle umbauen**: z. B. `ota_0` + `ota_1` à ~1,5 MB statt
   des 3-MB-factory-Slots (das aktuelle App-Image muss dafür unter die
   Slot-Größe passen; Font-Entfernung und BLE-Abschaltung helfen dabei).
   Achtung: Der Umbau selbst erfordert einmalig ein serielles Flashen,
   und die `config`-Partition bei 0x310000 sollte an ihrem Offset bleiben,
   damit die Konfiguration erhalten bleibt.
2. **OTA-Code**: `esp_https_ota` (Pull von einer URL) oder ein
   HTTP-POST-Endpoint mit `esp_ota_write` (Push). Absicherung mindestens
   per HTTP-Auth, idealerweise signierte Images
   (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`).
3. **Rollback-Strategie**: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, App
   bestätigt sich nach erfolgreichem Boot selbst
   (`esp_ota_mark_app_valid_cancel_rollback`).

Bis dahin gilt: Update = USB-Kabel.
