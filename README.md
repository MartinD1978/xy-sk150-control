# XY-SK150 Steuerung V0.2 – Debian 13

© 2026 Martin Dietl · Software: Martin-Dietl-Softwarelizenz 1.0 · MD-Logo ausgenommen

C++17-Anwendung mit Live-Messwerten, einstellbarer Spannung und Strombegrenzung, optionalem
Spannungslimit, Ausgang EIN/AUS, CV/CC-Anzeige und getrennten OVP/OCP- und
OPP-Einstellungen für M0. Verbindung und Modbus-Slave sind konfigurierbar.
Standard: `/dev/ttyUSB0`, Modbus RTU, 115200 Baud, 8N1, Slave 1.

## Lizenz und Kontakt

Es gilt die **Martin-Dietl-Softwarelizenz, Version 1.0** in `LICENSE`.
Der Quellcode ist zugänglich (Source Available); dies ist keine MIT- oder
OSI-anerkannte Open-Source-Lizenz.

- Private, nichtkommerzielle Nutzung: kostenlos, ohne Meldung.
- Firmeninterne/betriebliche Nutzung: kostenlos nach einmaliger E-Mail-Meldung
  pro Unternehmen, auch bei mehreren Nutzern, Standorten oder Installationen.
  Selbstständige melden sich einmal für ihren Betrieb. Name und Hinweis auf die
  interne Verwendung der XY-SK150 Steuerung genügen. Meldung vor oder spätestens
  bei Nutzungsbeginn; eine Antwort muss nicht abgewartet werden.
- Verkauf, Einbindung in kommerzielle Produkte und weitere kommerzielle Verwertung
  außerhalb der erlaubten internen Nutzung: nur mit vorheriger ausdrücklicher Zustimmung.
- Bei erlaubter Weitergabe muss der vollständige zugehörige Quellcode einschließlich
  Änderungen und Build-Dateien unter derselben Lizenz mitgegeben werden.
  Privat/interne Änderungen müssen nicht veröffentlicht werden.
- **© 2026 Martin Dietl**, Lizenztext und `NOTICE` bleiben erhalten.
- Das MD-Logo ist ausgenommen; unveränderte Anzeige und Weitergabe innerhalb
  dieser Software sind erlaubt, eigenständige Nutzung wird nicht eingeräumt.

**Firmenmeldung und Genehmigungsanfragen: dietl.martin@mail.de**
Die Software verschickt keine E-Mails oder Nutzungsdaten. Die Meldung erfolgt manuell.
Die Kurzfassung ersetzt nicht den vollständigen Lizenztext.
Die neue Lizenz gilt für die mit ihr bereitgestellte Fassung; sie entzieht keine
bereits an früheren MIT-Fassungen eingeräumten Rechte.

## Bauen und starten

```sh
sudo apt update
sudo apt install build-essential libwxgtk3.2-dev python3
make
./sk150
```

[Debian-13-Paket libwxgtk3.2-dev](https://packages.debian.org/trixie/libwxgtk3.2-dev)
liefert `wx-config`. Bei fehlendem Zugriff auf den USB-Serielladapter:

```sh
ls -l /dev/ttyUSB0
sudo usermod -aG dialout "$USER"
```

Danach vollständig ab- und wieder anmelden. Die Anwendung als normaler Benutzer
starten. Andere Programme mit Zugriff auf den Port vorher schließen. Der Port
wird exklusiv geöffnet; zuvor geöffnete fremde Dateideskriptoren bleiben davon
unberührt. Nur ein Modbus-Master darf aktiv sein.

## Bedienung

- **Datei → Beenden** beendet die Anwendung.
- **Verbindung → Schnittstelle auswählen …** bietet eine aktualisierbare Liste
  mit `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/ttyS*` und `/dev/serial/by-id/*`.
  Ein absoluter Pfad kann zusätzlich manuell eingetragen werden. Außerdem sind
  Baudrate, Datenbits (5–8), Parität (keine/gerade/ungerade), Stoppbits (1/2) und
  Modbus-Slave-Adresse (1–247) einstellbar. Verfügbare Standardbaudraten: 1200,
  2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400. Für das SK150 sind
  weiterhin 115200 / 8N1 / Slave 1 vorbelegt; Modbus RTU verwendet normalerweise
  8 Datenbits. Die Parameter müssen zur tatsächlichen Gerätekonfiguration passen.
  Der Treiber muss die gewählte Kombination unterstützen; die Übernahme wird
  nach dem Konfigurieren geprüft. Nicht unterstützte Kombinationen ergeben einen
  Verbindungsfehler statt stiller Ersatzwerte.
  Die Auswahl wird in `sk150-control.conf` im Benutzer-Konfigurationsverzeichnis
  gespeichert. Während einer laufenden Verbindung ist sie gesperrt. Beim Start
  wird die gespeicherte Auswahl geladen, aber nicht automatisch verbunden.
  Ungültige gespeicherte Zahlenwerte führen zu den Standardparametern zurück.
  Diese Einstellungen konfigurieren nur den Rechner; die Register für Baudrate
  und Slave-Adresse im SK150 werden niemals geschrieben.
- Der große **Stecker-Button rechts oben** verbindet oder trennt. Rot bedeutet getrennt,
  grün bedeutet frische Messwerte und fehlerfreie Antworten auf alle abgefragten
  Register. Bei Kommunikations- oder Öffnungsfehlern blinkt er rot, mit einem
  Wechsel zwischen hellem und dunklem Rot alle 500 ms. Ein Fehlertext und ein
  Tooltip ergänzen die Farbe. Ein Klick bei Fehler trennt bzw. quittiert den
  Verbindungsversuch; ein weiterer Klick startet einen neuen Versuch.
- **Einstellungen → Spannungslimit …** öffnet den Dialog mit Aktivierungshaken,
  maximaler Spannung, **Übernehmen** und **Abbrechen**. Bei jedem Programmstart
  gilt wieder **12,00 V aktiviert**. Innerhalb des Programmlaufs bleibt die Wahl
  auch beim Trennen erhalten. Ohne Haken gilt der Gerätebereich bis 36,00 V.
  Ein kompakter Hinweis in der Infobox zeigt das wirksame Softwarelimit.
- Das Übernehmen eines Limits schreibt nichts ans Gerät und senkt einen schon
  höheren Gerätewert nicht automatisch ab. In diesem Fall wird gewarnt und EIN
  bleibt gesperrt. Abbrechen verwirft die Dialogänderungen. Während eines laufenden
  Schreibauftrags lässt sich das Limit nicht ändern.
- **Einstellungen → OVP/OCP …** öffnet die getrennten Schutzwerteinstellungen
  für M0; siehe unten. Öffnen und Schließen schreiben keine Schutzwerte.
- **Einstellungen → Leistungsbegrenzung …** ist ein eigener Dialog für die
  Überleistungs-Abschaltschwelle OPP in M0, getrennt von OVP/OCP und ohne CW.
- Der Fenstertitel zeigt die ausgewählte Schnittstelle, zum Beispiel
  **XY-SK150 · /dev/ttyUSB0**. Schnittstellenzeile, reiner Verbindungstext und
  die Baud-/8N1-Zeile entfallen im Hauptfenster; dort zeigt das Stecker-Icon die Verbindung.
- Die Infobox sitzt linksbündig über der Spannungsanzeige und reicht bis zum
  rechten Rand des Stromfelds. Abgerundete Ecken, heller Hintergrund und dezenter
  Schatten heben sie ab. Fehler sind rot und fett. Sie enthält Fehler, Limit,
  Schutzstatus, zuletzt beobachtete Auslösung und Schreibbestätigungen.
  Die doppelte Überschrift im Fenster entfällt.
- **Gerät → Informationen …** zeigt die laufend aktualisierte Eingangsspannung;
  im Hauptfenster steht sie nicht mehr.
- **Hilfe → Über …** zeigt das eingebettete MD-Logo, V0.2, Martin Dietl und den
  Lizenzhinweis. **Lizenz anzeigen …** öffnet den vollständigen Lizenztext und den
  Logo-Ausschluss. Eine C++-/Plattformzeile wird nicht angezeigt.
- Unter der Verbindungsanzeige stehen drei gleich gestaltete Messfelder
  nebeneinander: links **Spannung**, in der Mitte **Strom**, rechts **Leistung**.
  Die Leistung stammt direkt aus Register `0x0004` (0,01 W pro Schritt).
  Alle Felder haben schwarze Flächen, weiße Schrift und abgerundete, leicht
  erhobene Rahmen mit anthrazitfarbener Lichtkante und Schatten. Die Beschriftung
  steht innerhalb des Rahmens linksbündig über dem Wert. V, A und W haben eigene,
  feste Positionen am rechten Innenrand und bewegen sich nicht mit den Ziffern.
- **CV** sitzt oben rechts im Spannungsfeld, **CC** oben rechts im Stromfeld.
  Der aktive Modus ist rot und fett, der inaktive gut lesbar grau. Bei unbekanntem
  oder veraltetem Status sind beide grau. CV = Konstantspannung,
  CC = Konstantstrom. Der Modus wird aus Register `0x0011` gelesen;
  die Umschaltung übernimmt das Gerät. Die Umschaltung selbst verändert keine Sollwerte.
- Direkt unter Spannung und Strom stehen die jeweiligen Eingabefelder mit
  **Setzen** und Schieberegler. Punkt und Komma sind erlaubt. Spannung: höchstens
  zwei Nachkommastellen; Strombegrenzung: höchstens drei. **Setzen** oder Enter
  überträgt nur den jeweiligen Wert. Eingaben werden nicht vom Polling überschrieben.
- Die Regler haben feste Skalen **0–36 V** und **0–7,2 A**, mit Schritten von
  **0,1 V** bzw. **0,05 A**. Eingabefelder erlauben feinere Werte. Beide bleiben
  synchron. Ziehen schreibt noch nichts. Der Schieber endet am höchsten zulässigen
  Schritt: Spannung höchstens am Softwarelimit und strikt unter OVP, Strom strikt
  unter OCP. Bei OCP 0,800 A ist der höchste Reglerschritt beispielsweise 0,750 A.
  Blaue senkrechte Markierungen zeigen das aktive Spannungslimit, rote OVP/OCP.
  Die Beschriftungen stehen in einer Zeile über den Reglern; darunter nur die Skala.
  Eine OVP über 36 V wird am Skalenende mit ihrem tatsächlichen Wert beschriftet.
- Der einzelne **⏻-Taster** unten rechts schaltet den Ausgang um. Rot bedeutet
  AUS, grün EIN; die Anzeige folgt ausschließlich dem zurückgelesenen Gerätestatus.
  Er hat wie der Verbindungsbutton eine erhabene graue Außenfläche, abgerundete
  Farbfläche und weißes Symbol. Bei unbekanntem Zustand ist er deaktiviert.
  Zwischen Taster und Trennlinie liegen ebenso 14 Pixel wie zwischen Linie und
  Speicherbuttons. Ein Einschalten ist nur innerhalb der geprüften Grenzen möglich.
- Ganz unten stehen **zehn Speicherplätze in einer Reihe**, daneben **…** zum
  Bearbeiten. Jeder Platz zeigt Nummer, Spannung und Strombegrenzung. Im Dialog
  aktivieren und bearbeiten, dann **Speichern**. Die Plätze werden in derselben
  Benutzerkonfiguration wie die Schnittstelle gespeichert und nach Neustart geladen.
  Anfangs sind sie leer. Öffnen, Bearbeiten oder Programmstart schreibt nichts ans Gerät.
- **Ein Klick auf einen gültigen Speicherplatz überträgt beide Sollwerte direkt**,
  ohne weiteren Setzen-Klick. Er schaltet den Ausgang nicht automatisch ein oder aus.
  Plätze über dem Softwarelimit oder bei/über OVP/OCP sind deaktiviert und rot,
  ebenso konfigurierte Plätze bei unbekannten/veralteten Schutzwerten. Nach neuen
  Messwerten oder geänderten Grenzen wird die Freigabe neu geprüft.
- **Trennen** und Beenden ändern keine Geräteeinstellungen und schalten den
  Ausgang nicht automatisch ab. Ein schon gestarteter Auftrag kann noch enden.

Das Feld `10,00` und die Stromvorgabe `0,500` sind nur Eingabevorgaben, keine Messwerte. Weder Start noch
Verbinden, Wiederverbinden oder Beenden schreibt automatisch einen Sollwert.
Limit und Haken bleiben innerhalb eines Programmlaufs auch beim Wiederverbinden
erhalten, werden aber nicht über Programmneustarts gespeichert.

Die Zeile „Letzte gültige Messung“ wurde entfernt. Es gibt keinen laufenden
Sekundenzähler. Bei Fehlern bleiben die letzten Istwerte sichtbar und werden
grau dargestellt; Status und Ausgangsanzeige kennzeichnen die ungültige Verbindung.

## OVP/OCP im Gerät (Datengruppe M0)

Der Dialog **Einstellungen → OVP/OCP …** bietet zwei getrennte Einstellungen:

- **OVP setzen**: Überspannungs-Abschaltschwelle, 0 bis 38,00 V,
  maximal zwei Nachkommastellen. Register `0x0053`, Einheit 0,01 V.
- **OCP setzen**: Überstrom-Abschaltschwelle, 0 bis 7,200 A,
  maximal drei Nachkommastellen. Register `0x0054`, Einheit 0,001 A.

Die Register sind in der Originaltabelle als Schutzwerte der Speichergruppe M0
dokumentiert. Die Software ruft keine Speichergruppe auf und wechselt sie nicht.
Daher die aktive Datengruppe sowie die angezeigten Schutzwerte am Gerät prüfen;
das Rücklesen von M0 allein beweist nicht die aktive Abschaltwirkung.

Die Werte werden im Hintergrund nur gelesen. Beim Öffnen des Dialogs werden
die ersten gültigen Werte einmal in die Eingabefelder übernommen. Weitere
Messzyklen aktualisieren **Gelesen aus M0**, überschreiben aber keine bearbeitete
Eingabe. Nach Schließen und erneutem Öffnen werden die Felder neu befüllt. Bei unbekannten oder ungültigen Gerätewerten
bleiben die Setzen-Tasten gesperrt.

Eine Setzen-Taste schreibt nur ihren eigenen Schutzwert. Der andere Schutzwert,
Spannungssollwert und Ausgangszustand werden dabei nicht geschrieben. Vorher
wird gelesen, danach werden Schreib-Echo und Registerwert geprüft. Bei Fehlern
gibt es keine automatische Wiederholung. Die Werte werden nicht automatisch
auf Werkseinstellungen oder auf unser Softwarelimit gesetzt.

OVP/OCP sind Abschaltschwellen, nicht die normale CV/CC-Regelung. Die OCP-Eingabe
ändert nicht die normale Strombegrenzung in `0x0001`. Das Software-Spannungslimit
und OVP sind separate Einstellungen; eine OVP-Einstellung bis 38 V ist auch bei
aktivem 12-V-Softwarelimit möglich. Dem Rohwert 0 wird hier keine besondere
Bedeutung wie „deaktiviert“ zugeschrieben; sein Verhalten ist am Gerät zu prüfen.
CW wird nicht aktiviert oder konfiguriert.

Die Register, Skalierung und Wertebereiche stammen aus der Dokumentation. Die
neue Funktion wurde mit einem simulierten Gerät geprüft, nicht an echter Hardware.

## Leistungsbegrenzung OPP (Datengruppe M0)

**Einstellungen → Leistungsbegrenzung …** zeigt den gelesenen OPP-Wert und ein
Eingabefeld in Watt. **OPP setzen** schreibt nur Register `0x0055`; **Schließen**
schreibt nichts. Bereich nach der verwendeten XY-SK150-Anleitung: **0–150,0 W**,
Auflösung **0,1 W** (eine Nachkommastelle, Punkt oder Komma). Beispielsweise
entspricht 125,0 W dem Rohwert 1250. Ungültige Werte werden abgelehnt.

Vor dem Schreiben wird gelesen, das Schreib-Echo geprüft und danach erneut
gelesen und verglichen. Ein fehlgeschlagener oder unbestätigter Auftrag wird
nicht automatisch wiederholt. Ein Lesefehler sperrt die Setzen-Taste; erstmaliges
Öffnen und spätere Messzyklen überschreiben keine bearbeitete Eingabe.

OPP ist eine Abschaltschwelle, kein Konstantleistungsmodus. Die CW-Register
`0x0022` und `0x0023` bleiben unberührt. Wie OVP/OCP liegt dieser Schutzwert in
M0; die Anwendung ruft keine Datengruppe auf. Aktive Gruppe und Schutzwirkung
sind am konkreten Gerät zu prüfen. Für den Rohwert 0 wird kein besonderes
Deaktivierungsverhalten angenommen. Abweichende SK150-Varianten mit einem anderen
Leistungsbereich wurden hier nicht verifiziert.

## Ausgelösten Schutz anzeigen

Register `0x0010` wird ausschließlich mit Funktion `0x03` gelesen:
0 = keine Auslösung gemeldet, 1 = OVP, 2 = OCP, 3 = OPP, 4 = LVP,
5 = OAH, 6 = OHP, 7 = OTP, 8 = OEP, 9 = OWH, 10 = ICP, 11 = ETP.
Die Zuordnung steht in Anmerkung 4 der Originalanleitung.

In der Infobox bleibt eine gemeldete Auslösung rot und fett sichtbar, auch wenn
die Einstellungsdialoge geschlossen sind.
Unbekannte Codes erscheinen als Zahl und werden nicht als Normalzustand behandelt.
Bei Lesefehlern oder veralteten Messwerten steht dort „unbekannt / veraltet“.
Zusätzlich bleibt die zuletzt seit dem Verbinden beobachtete Auslösung sichtbar,
auch wenn das Gerät wieder 0 meldet. Beim erneuten Verbinden beginnt diese
Beobachtung neu; sie ist kein dauerhaft gespeichertes Ereignisprotokoll.
Ein Wert von 0 beweist nicht, dass vor der Verbindung nie ein Schutz ausgelöst hat;
sehr kurze Ereignisse zwischen Lesezyklen können unbemerkt bleiben.

Es wird weder automatisch quittiert noch wieder eingeschaltet. Eine Reset-Funktion
wird nicht aus der bloßen Schreibbarkeit des Registers abgeleitet.

## Begrenzung und Fehlerbehandlung

Spannung und Strom werden in GUI und Kommunikationsschicht als Wertepaar geprüft:
Spannung ≤ Softwarelimit und < OVP, Strom ≤ 7,200 A und < OCP.
Vor jedem neuen Sollwert-Schreibtelegramm werden OVP/OCP frisch gelesen; Fehler
verwerfen den Auftrag. Die Grenzprüfung verwendet die dokumentierten **M0**-Register
0x0053/0x0054. Die Software wechselt keine Speichergruppe; die aktive Gruppe und
Schutzwirkung am Gerät weiterhin prüfen. Rohwert 0 wird konservativ als Grenze 0
behandelt, nicht als deaktivierter Schutz. Es wird kein Sicherheitsabstand gegen
reale Überschwinger behauptet.


GUI und Protokollcode prüfen dasselbe für den Auftrag übernommene Limit. Solange
ein Auftrag läuft, ist der Menüpunkt zum Spannungslimit gesperrt, sodass sich die
Grenze nicht während des Schreibens ändert. Ungültige Eingaben können im
Limitdialog nicht übernommen werden. AUS bleibt bei gültiger Verbindung verfügbar.

Auch die Protokollfunktion prüft vor EIN anhand frisch gelesener Werte, dass
Soll- und Ausgangsspannung das aktive Maximum nicht überschreiten. Unabhängig
vom Haken sind Schreibsollwerte über 36,00 V ausgeschlossen.

Die Softwaregrenze ersetzt keinen hardwareseitigen Überspannungsschutz und
verhindert keine Änderungen am Gerät oder durch andere Programme. Wenn das
12-V-LED-Band weiterhin angeschlossen ist, das Limit bei höchstens 12,00 V lassen.
Bei Kommunikationsverlust bleibt der Gerätezustand bestehen.

Ein Hintergrundthread besitzt den Port. Die GUI übernimmt dessen Zustand alle
100 ms. Nach einem Lesezyklus folgt eine Pause von 250 ms; vor jeder Transaktion
mindestens 5 ms bzw. bei kleinen Baudraten mindestens 3,5 vollständige Zeichenzeiten.
Jede Transaktion hat ein Gesamtzeitlimit von 500 ms.
Teilübertragungen, CRC, Slave, Funktionscode, Länge, Modbus-Ausnahmen und
Schreib-Echo werden geprüft. Das ist kein Echtzeitbetrieb.

Vor einem Schreibversuch wird gelesen, danach werden Bestätigung und
zurückgelesener Sollwert bzw. Ausgangszustand geprüft. Ein einzelner Sollwert erzeugt ein Schreibtelegramm; ein Speicherplatz höchstens
zwei (Funktion 0x06 für Register 0x0000 und 0x0001). Diese beiden Änderungen sind
nicht atomar: Eine niedrigere Strombegrenzung wird zuerst gesetzt, ansonsten die
Spannung zuerst. Bei Fehlern kann nur ein Teil übernommen sein. Dann Gerätewerte
prüfen; es gibt weder automatische Wiederholung noch Rücksetzen. Fehlerhafte oder unbestätigte Schreibaufträge
werden niemals automatisch wiederholt; das Gerät könnte den Wert trotz
fehlender Bestätigung bereits übernommen haben.

Bei Fehlern der Messregister oder mehr als 1,5 s alten Messungen sind
Schreibzugriffe gesperrt. Fehler nur beim Ausgangsstatus sperren die Schaltknöpfe;
Bei Fehlern des CV/CC-Status sind beide Modusanzeigen grau. Auch Fehler nur bei
einem Zusatzregister lassen den Verbindungsbutton rot blinken. Lesen wird weiter versucht.
Nach USB-Abziehen bei Bedarf **Trennen**, dann **Verbinden**.
Modbus RTU hat keine Transaktionsnummer; extrem verspätete Antworten oder ein
weiterer Master lassen sich nicht in allen Fällen eindeutig zuordnen.

## Verwendete Register

| Holding Register | Inhalt | Einheit / Werte | Zugriff |
|---|---|---|---|
| `0x0000` | Spannungssollwert | 0,01 V | lesen, schreiben bis zum aktiven Maximum |
| `0x0001` | Strombegrenzung | 0,001 A | lesen, schreiben unter OCP |
| `0x0002` | Ausgangsspannung | 0,01 V | nur lesen |
| `0x0003` | Ausgangsstrom | 0,001 A | nur lesen |
| `0x0004` | Ausgangsleistung | 0,01 W | nur lesen |
| `0x0005` | Eingangsspannung | 0,01 V | nur lesen |
| `0x0010` | Schutzstatus / Auslösung | Codes siehe oben | nur lesen |
| `0x0011` | Regelungsstatus CVCC | 0 = CV, 1 = CC | nur lesen |
| `0x0012` | Ausgang ONOFF | 0 = AUS, 1 = EIN | lesen, schreiben |
| `0x0053` | OVP in M0 | 0,01 V, bis 3800 | lesen, schreiben |
| `0x0054` | OCP in M0 | 0,001 A, bis 7200 | lesen, schreiben |
| `0x0055` | OPP in M0 | 0,1 W, bis 1500 | lesen, schreiben |

Die Register `0x0000–0x0005` wurden vom Auftraggeber verifiziert. CVCC und ONOFF
sind in der [XY-SK150-Originalanleitung, Seiten 7–8](https://manuals.plus/m/7c757d44c69a38bc06ef590be1693ae011352babc70de2e7c55ddc625178631b_optim.pdf)
dokumentiert, einschließlich CV/CC-Zuordnung in Anmerkung 5 und Ein/Aus in
Anmerkung 6. Die aktuelle Menüversion wurde hier mit einem Simulator geprüft;
ein Test des gesamten Umbaus an echter Hardware steht noch aus.

Funktion `0x03` liest die sechs Messregister gemeinsam und die beiden Statusregister
separat; OVP/OCP in M0 werden gemeinsam gelesen.
Funktion `0x06` schreibt ausschließlich `0x0000`, `0x0001`, `0x0012`, `0x0053`, `0x0054`
oder `0x0055`. Alle Anfragen, CRC-Prüfungen und Schreib-Echos verwenden die
konfigurierte Slave-Adresse. Adresse 0 (Broadcast) ist ausgeschlossen.
Es gibt keine allgemeine Register-Schreibschnittstelle, keine Broadcasts und
keine Speichergruppen-Aufrufe. Registerwerte sind Big Endian; CRC16/Modbus
wird Low Byte zuerst übertragen.

## Tests und Dateien

```sh
make test
```

Die Tests verwenden ausschließlich Pseudoterminals, niemals `/dev/ttyUSB0`.
Sie prüfen alle 3601 erlaubten Rohsollwerte im Gerätebereich, aktivierte und
deaktivierte Limits, 0-V- und 24-V-Limits, ungültige Eingaben, Überläufe,
Grenzverletzungen ohne gesendetes Schreibtelegramm, CRC, fragmentierte Antworten,
falsche Slave-/Funktions-/Längenangaben, Modbus-Ausnahmen, Timeout, falsches
Schreib-Echo, Ausgangsschaltung und die CV/CC-Werte 0/1 sowie ungültige Werte.
Zusätzlich werden alle erlaubten OVP/OCP-Rohwerte, Eingaben mit Punkt/Komma,
Registeradressen, Skalierung, Rückleseabweichungen und Zeitüberschreitungen geprüft.
Verbotene Schutzwerte dürfen kein Telegramm senden; Schreibfehler keine Wiederholung.
Die Schutzstatustests prüfen alle dokumentierten Codes, unbekannte Codes, CRC-Fehler
und Timeout sowie ausschließlich lesenden Zugriff auf `0x0010`.
Die zusätzlichen OPP-Tests prüfen Registeradresse, Skalierung, alle erlaubten
Rohwerte, Grenzverletzungen, ungültiges Rücklesen, Timeout und Lesefehler vor dem
Schreiben. Die seriellen Integrationstests prüfen 9600 Baud / 8N2 / Slave 17,
inklusive CRC und Ablehnung einer falschen Antwortadresse. Gültige und ungültige
Verbindungsparameter werden ebenfalls geprüft.
Die Textanzeige wird in `C` und `C.UTF-8` geprüft. Deutsche Texte werden explizit
aus UTF-8 dekodiert; printf-Formattexte enthalten nur ASCII, um den früheren
Locale-abhängigen Absturz zu vermeiden.

Erfolgreich mit GCC und wxWidgets 3.2.8 auf Linux x86_64 gebaut. Alle genannten
Tests bestanden. Zusätzlich wurden die Menüs und Dialoge mit GTK auf einem
virtuellen Bildschirm gestartet, geprüft und visuell kontrolliert. Beim Auswählen, Öffnen, Übernehmen des Softwarelimits und Abbrechen wird
kein Schreibbefehl gesendet; die gezielten Tests für Speicher, Strom, Spannung und
Ausgang werden ausschließlich auf einem simulierten Gerät ausgeführt. Test des Umbaus an echter Hardware steht noch aus.

Optionaler GUI-Integrationstest (virtueller Bildschirm, kein SK150 notwendig):

```sh
sudo apt install xvfb xauth imagemagick
make gui-test
testshots=$(mktemp -d)
xvfb-run -a python3 tests/gui_serial_test.py ./gui-test "$testshots"
```

Der Test nutzt temporäre Konfiguration und ein Pseudoterminal, prüft die
Portauswahl samt Speicherung, Übernehmen/Abbrechen von Verbindungsparametern,
19200 Baud / 8N2 / Slave 17 am simulierten Gerät, das Standardlimit,
Übernehmen/Abbrechen sowie Aktivieren/Deaktivieren des Limits, OVP/OCP- und
OPP-Verfügbarkeit, Logo/Version/Lizenz, Verbindungszustände, Blinkphasen und
Trennen nach einem Öffnungsfehler. Die Infobox-Abmessungen und die Position von
Limit und Schutzstatus in der Infobox werden geprüft. Außerdem
stellt er sicher, dass reine Dialog-/Limitaktionen ausschließlich lesen. Er prüft
außerdem Speicherpersistenz, rote Sperrung, Reglerschritte, direkten Speicherabruf,
Strom setzen, Enter auf Spannung und beide Zustände des Ausgangstasters. Die erzeugten
Testbilder enthalten simulierte Werte.

- `src/main.cpp`: Hauptfenster, Menüs, Hintergrundthread und Statusverwaltung
- `src/dialogs.hpp`: Verbindungsparameter, Spannungslimit, OVP/OCP, OPP, Über/Lizenz und Symbolbuttons
- `src/controls.hpp`: Regler mit Grenzmarken und Speicherplatzdialog
- `src/setpoints.hpp`: gemeinsame Sollwert- und Grenzprüfungen
- `src/connection.hpp`: Verbindungszustände
- `src/resources.hpp`: automatisch eingebettete Ressourcen
- `assets/MD_PCB_Logo.svg`: Original-Logo, mit gesonderten Logobedingungen
- `LICENSE`, `NOTICE`: Softwarelizenz und Logo-Ausnahme
- `tools/embed_resources.py`: erzeugt die eingebetteten Ressourcen beim Build
- `src/modbus.hpp`, `src/modbus.cpp`: serieller Port, Protokoll und Spannungslimit
- `src/display.hpp`: Messwert- und CV/CC-Texte
- `tests/`: Protokoll-, Anzeige- und Pseudoterminaltests
- `Makefile`: Build ohne zusätzliche Modbus-Bibliothek

Das ZIP enthält Quellcode und Tests. Im vorhandenen Arbeitsordner liegt außerdem
die frisch gebaute Anwendung. Zum Entfernen der lokalen Build-Ergebnisse:

```sh
make clean
```
