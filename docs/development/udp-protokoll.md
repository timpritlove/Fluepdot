# UDP-Raw-API

Implementierung: `main/raw_api.c`, gestartet als FreeRTOS-Task `raw_api`
aus `app_main`.

## Eckdaten

| Eigenschaft | Wert |
|---|---|
| Port | **1337/UDP**, hartkodiert |
| Socket | `AF_INET6` mit `IPV6_V6ONLY=0` ⇒ Dual-Stack (IPv4 + IPv6) |
| Authentifizierung | keine |
| Antwort | keine (fire and forget) |

## Paketformat

Ein Datagramm = ein kompletter Framebuffer:

- Länge: exakt `2 × Breite` Bytes (z. B. 230 Bytes bei Breite 115).
  Pakete mit abweichender Länge (zu kurz **oder** zu lang) werden ohne
  Antwort verworfen (nur Log-Warnung), damit fremde Pakete nicht als
  Matrixdaten interpretiert werden. Der Empfangspuffer ist bewusst ein Byte
  größer als ein Frame: lwIPs `recvfrom()` ignoriert `MSG_TRUNC` und meldet
  nie mehr als die Puffergröße – ohne das Extra-Byte würden übergroße
  Pakete auf exakt `2 × Breite` Bytes gekürzt und fälschlich akzeptiert.
- Inhalt: das rohe `columns`-Array des Framebuffers – pro Spalte ein
  `uint16_t` (Little-Endian, ESP32-nativ), Spalte 0 zuerst (x von links
  nach rechts).

### Bit-Belegung einer Spalte

Die 16 Bits einer Spalte kodieren die Pixel wie in
`components/flipdot/framebuffer.c` (`flipdot_framebuffer_set_pixel`):

- `y < 8` (untere Hälfte): Bit-Offset `7 - y`
- `y >= 8` (obere Hälfte): Bit-Offset `15 - (y - 8)` = `23 - y`

Also: Bit 7…0 = y 0…7, Bit 15…8 = y 8…15. Ursprung (0,0) unten links.

Das ist exakt dieselbe Kodierung, die das CLI-Kommando `framebuf64`
(base64-verpackt) verwendet.

## Verhalten

Nach Empfang eines gültigen Pakets wird der Inhalt per `memcpy` in
`flipdot.framebuffer->columns` kopiert und das Dirty-Flag gesetzt; der
Rendering-Task überträgt den Frame anschließend auf die Hardware.

Hinweis: Bis zur Korrektur im August 2026 kopierte der Code fälschlich auf
die Adresse des `columns`-Pointers (`memcpy(&...->columns, ...)`) und
korrumpierte damit die Framebuffer-Struktur statt Pixel zu schreiben — die
UDP-API war damit effektiv funktionsunfähig (siehe [bugs.md](bugs.md), F1).

## Einordnung

- Die UDP-API ist der effizienteste Weg, den Framebuffer zu setzen
  (ein Paket, kein HTTP-Overhead, kein ASCII-Encoding).
- Sie war in der offiziellen Doku (`docs/source/`) bislang **komplett
  undokumentiert**.
- Kein Header, keine Sequenznummern, keine Teilupdates – für
  Streaming-Anwendungen genügt das, verlorene Pakete bedeuten verlorene
  Frames.
