# Flipdot-Treiber und Rendering-Pipeline

Komponente: `software/firmware/components/flipdot/`
(`flipdot.c`, `framebuffer.c`, `rendering_options.c` + Header in `include/`).

## Hardware-Ansteuerung

### Peripherie

- **SPI (HSPI, 8 MHz)**: taktet einen 24-Bit-Zustand in drei kaskadierte
  74HCT595-Schieberegister (IC5–IC7) auf dem Fluepboard.
  MOSI = GPIO13, SCLK = GPIO14 (MISO = GPIO12 ist unbeschaltet).
- **GPIO**: `RCLK` = GPIO19 (Übernahme ins Ausgangsregister),
  `SRCLR` = GPIO18 (Register löschen), `OE` = GPIO15 (Output Enable),
  `PWR_ON` = GPIO21 (schaltet die 12-V-Versorgung der Panels).
- Timing per **Busy-Wait** (`esp_rom_delay_us`) – kein RMT, kein
  Hardware-Timer.

### Registerbelegung (`flipdot_io_state_t`, 3 Bytes, packed)

| Feld | Bits | Bedeutung |
|---|---|---|
| `select` | 5 | Panel-Select, aktiv low (0-Bit ⇒ SELECT-Leitung auf +12 V) |
| `clock` | 1 | Spaltentakt des Panels (invertiert) |
| `reset` | 1 | Panel-Reset (invertiert) |
| `clear` | 1 | Low-Side-Treiber „Column Clear“ (löscht eine ganze Spalte) |
| `rows` | 16 | High-Side-Treiber der 16 Zeilen |

Hardware-Schutz: `clear` und `rows` dürfen **nie gleichzeitig** aktiv sein
(brennt Sicherung F1 durch); `flipdot_write_registers` verweigert das.

### Ablauf eines Panel-Updates

1. **Panel selektieren** (`flipdot_select_panel`): alle Selects weg +
   `reset` pulsen, dann genau ein Select-Bit aktivieren.
2. **Pro Spalte** (`flipdot_render_column`), von x=0 bis Panelbreite:
   - optionaler `pre_delay` (× 50 µs)
   - Spaltentakt pulsen (schiebt die „1“ im Spalten-Schieberegister des
     Panels weiter)
   - **Clear-Zyklus** (falls nötig): `clear` aktivieren, `clear_delay` × 50 µs
     warten, deaktivieren – löscht die **ganze Spalte** auf dunkel
   - **Set-Zyklus**: `rows` = 16-Bit-Spaltenmuster anlegen,
     `set_delay` × 50 µs warten, `rows` = 0

Es gibt also **keinen Einzelpixel-Flip auf Hardware-Ebene** – die kleinste
physische Einheit ist die Spalte (Clear ganze Spalte, Set aller
Hell-Pixel der Spalte gleichzeitig über die 16 Row-Treiber).

### Timing

Defaults (in `flipdot_rendering_options.h`): `pre_delay` = 0,
`clear_delay` = `set_delay` = 160 Ticks à 50 µs = **8 ms** pro Puls.

Untere Schranke für einen vollen Refresh (nur Coil-Zeit):
`Breite × (8 ms + 8 ms)` ⇒ bei 115 Spalten ≈ **1,84 s**, plus pro Spalte
mehrere SPI-Transfers mit je ~300 µs Overhead (SRCLR-Puls 100 µs,
RCLK-Puls 100 µs, Settle 100 µs, mehrfach pro Spalte).

## Framebuffer-Modell

```c
typedef struct {
    uint16_t* columns;  // eine uint16 pro Spalte, 16 Pixel
    uint8_t width;
} framebuffer_t;
```

- Ursprung (0,0) unten links; Bit-Mapping siehe
  [udp-protokoll.md](udp-protokoll.md).
- ASCII-Kodierung (`X`/Leerzeichen) für HTTP und Konsole.

### Dreifach-Pufferung und Dirty-Flag

`flipdot_t` hält drei Framebuffer:

1. `framebuffer` – öffentlich, wird von HTTP/CLI/UDP beschrieben
2. `framebuffer_internal` – Snapshot, aus dem gerendert wird
3. `framebuffer_internal_old` – vorheriger Snapshot für Differential-Diff

Ablauf: Schreiber ändern `framebuffer` und rufen `flipdot_set_dirty_flag`
(Event-Group-Bit). `flipdot_task` wacht auf, `flipdot_render` nimmt den
Mutex, rotiert die Puffer (`flipdot_cycle_internal_datastructures` –
free + calloc + copy bei jedem Frame) und rendert. Danach wird
`FLIPDOT_RENDERING_DONE_BIT` gesetzt (von `GET /rendering/wait` genutzt).

### Rendering-Modi

| Modus | Verhalten |
|---|---|
| `FULL` (0) | Jede Spalte jedes Panels bekommt volle Clear- und Set-Pulse |
| `DIFFERENTIAL` (1) | Ganzer Frame ohne Änderungen ⇒ komplett übersprungen; Panels ohne Änderungen ⇒ übersprungen; pro Spalte: Clear nur, wenn mindestens ein Pixel hell→dunkel wechselt; Set-Puls verkürzt (100 µs), wenn die Spalte leer oder unverändert ist |

Wichtig: Differential arbeitet **spaltengranular**, nicht pixelgranular.
Auch unveränderte Spalten müssen weiterhin getaktet werden (das
Spalten-Schieberegister im Panel muss weiterlaufen), nur die Pulse werden
auf ~100 µs verkürzt. Muss ein einzelnes Pixel gelöscht werden, wird die
ganze Spalte gelöscht und alle Hell-Pixel der Spalte neu gesetzt.

### Rendering-Optionen

`flipdot_rendering_options_t`: `mode`, pro Spalte
`{pre,clear,set}_delay`, `panel_order` (Default 0..n-1). Nur `mode` ist
persistierbar; Timings sind flüchtig (HTTP), `panel_order` ist über keine
Schnittstelle änderbar.

## Analyse: Parallele Panel-Updates

**Die Hardware erlaubt keine parallelen Panel-Updates.** Gründe:

1. **Ein gemeinsamer Steuerbus**: Alle Panels teilen sich Clock, Reset,
   Clear und die 16 Row-Leitungen über das Flachbandkabel (J3). Nur die
   SELECT-Leitung unterscheidet die Panels. Zwei gleichzeitig selektierte
   Panels würden identische Spaltendaten erhalten – unterschiedliche
   Inhalte parallel zu schreiben ist elektrisch unmöglich.
2. **Eine Schieberegisterkette**: Die drei 74HCT595 bilden einen einzigen
   globalen IO-Zustand; es gibt keine unabhängigen Treiberstufen pro Panel.
3. **Stromversorgung**: Die Doku fordert ein 12-V-Netzteil ≥ 3 A; Kommentar
   in `flipdot_gpio.h`: „Only one panel should be selected at a time, to
   account for the maximum amount of power the flipdot PCB can drive.“
   Gleichzeitiges Bestromen mehrerer Panels würde Treiber und Netzteil
   überlasten.

**Was stattdessen möglich ist** (Software-Optimierungen ohne
Hardware-Änderung):

- **Differential-Modus konsequent nutzen** – unveränderte Panels kosten
  bereits heute nichts, unveränderte Spalten nur den Takt-Overhead.
- **Timings senken**: Die 8 ms Default sind konservativ. Die offizielle
  Doku nennt 1600 µs als „üblicherweise ausreichend“ – ein Faktor 5 an
  möglichem Durchsatz, konfigurierbar pro Spalte via
  `POST /rendering/timings`. Kandidat für persistente Konfiguration.
- **Skip-Pulse eliminieren**: Bei `skip_set` wird `rows` trotzdem für
  ~100 µs bestromt (`flipdot.c`), bei `skip_clear` 100 µs gewartet.
  Der Takt ließe sich auch ganz ohne Bestromungsfenster weiterschieben.
- **SPI-Overhead reduzieren**: Pro Spalte werden 4–6 vollständige
  Registerschreibvorgänge (je SRCLR-Puls + SPI + RCLK-Puls + 100 µs
  Settle) ausgeführt. Die drei fixen 100-µs-Delays pro Schreibvorgang sind
  vermutlich deutlich zu konservativ für 74HCT595-Register.
- **Puffer-Rotation ohne malloc/free**: `flipdot_cycle_internal_datastructures`
  allokiert pro Frame neu; Ringtausch der Pointer wäre ausreichend.

**Effizienz von Einzelpixel-Updates**: `POST /pixel` ändert nur den
Software-Framebuffer und setzt das Dirty-Flag – der Rendering-Task rendert
danach den Frame. Im Differential-Modus wird dabei nur das betroffene Panel
angefasst, aber alle Spalten des Panels getaktet; das Setzen eines
einzelnen Pixels kostet einen Set-Puls für dessen Spalte, das Löschen einen
Clear- plus Set-Puls (ganze Spalte neu). Für Einzelpixel-Interaktion ist
das akzeptabel; ein „Pixel-Streaming“ mit hoher Rate sollte stattdessen
ganze Frames über die UDP-API schicken und Differential rendern lassen.

## Bekannte Schwächen im Treiber

Siehe [bugs.md](bugs.md): Y-Bounds-Check erlaubte `y == 16`, Semaphore-Leak
bei Renderfehlern, falscher `sizeof` in der Puffer-Rotation, irreführende
Header-Kommentare („10ms counts“ statt 50-µs-Schritte, Clear/Set-Kommentare
vertauscht).
