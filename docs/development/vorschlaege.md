# Vorschläge: Vereinheitlichte Konfiguration, Absicherung, Resilienz

Dieses Kapitel ist **Konzept**, nicht Ist-Zustand. Es beschreibt die im
Projekt beschlossene Richtung für die nächsten Umbauschritte.

## 1. Gemeinsame Konfigurationsschicht (USB + HTTP)

Problem heute: Konfiguration ist nur per USB-CLI änderbar, Änderungen
wirken erst nach `config_save` + `reboot`, Laufzeitoptionen (Timings,
Rendering-Modus via HTTP) sind flüchtig und leben an der persistenten
Konfiguration vorbei.

Vorschlag:

- **Ein zentrales Config-Modul** mit typisierten Einträgen
(Key, Typ, Default, Min/Max, Hot-Apply-Callback). CLI und HTTP werden
reine Frontends über dieselben Getter/Setter.
- **Erweiterte Felder**: HTTP-Port, UDP-Port, HTTP-Auth
(aktiviert/Benutzer/Passwort), Rendering-Timings (ein globales Tripel
statt pro Spalte – pro-Spalte-Timings bleiben als Laufzeit-Override),
Panel-Reihenfolge.
- **HTTP-Endpoints**: `GET /config` (JSON, Passwörter maskiert),
`PUT /config` (Teilobjekt), `POST /config/save`, `POST /config/reset`.
CLI behält die bekannten Kommandos, ergänzt um `config_set <key> <value>`
/ `config_get [key]` als generischen Zugriff.
- **Hot-Apply wo möglich**: Hostname/mDNS, Rendering-Optionen und Ports
(Server-Neustart) sind ohne Reboot anwendbar; nur WLAN-Moduswechsel und
Panel-Layout dürfen weiterhin einen Reboot verlangen (explizit im
API-Response kommunizieren: `"reboot_required": true`).



### Speicherformat und Migration

Der heutige CRC16-Blob ohne Version verliert bei jeder Strukturänderung
die komplette Konfiguration. Zwei Optionen:

1. **NVS verwenden** (empfohlen): Ein Namespace `fluepdot`, ein Key pro
  Eintrag. Vorteile: eingebaute Wear-Leveling-/Commit-Semantik, neue
   Felder sind automatisch abwärtskompatibel (fehlender Key ⇒ Default),
   kein eigener CRC-Code. Die `config`-Partition kann für eine
   Übergangszeit als Migrationsquelle gelesen werden (einmalig beim ersten
   Boot der neuen Firmware: alten Blob lesen → in NVS übernehmen).
2. Blob behalten, aber mit `magic` + `version` + Feld-Migrationen –
  mehr Eigenbau, kein echter Vorteil.



## 2. HTTP-Absicherung

- **HTTP Basic Auth** (RFC 7617) über alle schreibenden Endpoints,
optional auch lesend; per Konfiguration aktivierbar
(`http.auth_enabled`, `http.auth_user`, `http.auth_password`).
Auf `esp_http_server`-Ebene als gemeinsamer Filter in jedem Handler
(Helper `httpd_check_auth(req)`), da der IDF-Server keine Middleware
kennt.
- Passwort-Speicherung: mindestens nicht im `config_show`-Klartext-Dump
(maskieren); Hashing (SHA-256) ist bei Basic Auth serverseitig möglich.
- CORS konfigurierbar machen (`*` nur als Default für Abwärtskompatibilität).
- Die UDP-API bleibt bewusst auth-frei (Streaming), sollte aber per
Konfiguration deaktivierbar sein (`udp.enabled`).



## 3. Konfigurierbare Ports

- `http.port` (Default 80): `httpd_config_t.server_port` setzen.
- `udp.port` (Default 1337): im `raw_api`-Task aus der Konfiguration lesen.
- mDNS-Announcement muss den konfigurierten HTTP-Port übernehmen
(heute hartkodiert 80 in `mdns_util.c`).



## 4. Resilienz

- **WLAN-Reconnect** überarbeiten (Bug F22): IP-Event-Handler dauerhaft
registriert lassen, `retry_count` bei erfolgreichem Connect zurücksetzen,
Backoff statt sofortigem Endlos-Reconnect, AP-Fallback nach n
Fehlversuchen erwägen (Gerät bleibt so immer erreichbar).
- **Boot ohne WLAN** bleibt wie heute möglich; zusätzlich Watchdog für den
Rendering-Task erwägen.
- `httpd`/`raw_api` bei Fehlern neu starten statt Task-Endlosschleifen mit
Fehler-Logs.



## 5. Firmware-Verschlankung (Voraussetzung für OTA)

- Fonts entfernen (siehe [fonts.md](fonts.md)): ~43 KB+. **Erledigt (August 2026).**
- Bluetooth/NimBLE deaktivieren (`CONFIG_BT_ENABLED=n`): spart Flash/RAM,
kein Funktionsverlust (es gibt keinen BLE-Code). **Erledigt (August 2026).**
- RS485-Konfigurationsfelder entfernen oder Feature endlich implementieren –
im Redesign entscheiden.
- Danach Partitionsumbau auf `ota_0`/`ota_1` und OTA-Implementierung
(siehe [firmware-update.md](firmware-update.md)).



## Vorgeschlagene Reihenfolge

1. Offensichtliche Bugfixes (erledigt, siehe [bugs.md](bugs.md))
2. Font-Entfernung + BLE-Abschaltung (erledigt; Image ~64 KB kleiner)
3. Config-Redesign (NVS, gemeinsame Schicht, Ports, HTTP-Auth, Hot-Apply)
4. WLAN-/Task-Resilienz
5. Partitionsumbau + OTA

