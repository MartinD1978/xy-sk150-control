# XY-SK150 Steuerung V0.2

Erste öffentliche Fassung für Debian 13 mit nativer wxWidgets-Oberfläche.

Spannung, Strom und Leistung werden live angezeigt. Spannung und Strombegrenzung
lassen sich über Eingabefelder, Schieberegler oder zehn dauerhaft gespeicherte
Sollwertpaare einstellen. Die Software berücksichtigt das Spannungslimit und die
gelesenen OVP/OCP-Werte. Ungültige Speicherplätze sind rot gesperrt.

Verbindung und Ausgang besitzen eigene Symboltaster. Schnittstellenparameter,
Schutzwerte und Geräteinformationen sind über Menüs erreichbar.

## Bauen

```sh
sudo apt install build-essential libwxgtk3.2-dev python3
make
./sk150
```

Standardverbindung: `/dev/ttyUSB0`, 115200 Baud, 8N1, Modbus-Slave 1.
Beim Start wird nicht automatisch verbunden oder geschrieben. Das Software-
Spannungslimit startet aktiviert bei 12,00 V. Einzelheiten stehen in README.md.

## Prüfung

Protokolltests und GUI-Integrationstests mit einem simulierten seriellen Gerät
bestanden. Die Software ist kein Echtzeitsystem; Schutzwerte werden aus M0 gelesen.
Die tatsächliche Schutzwirkung und aktive Datengruppe am eigenen Gerät prüfen.

## Lizenz

Martin-Dietl-Softwarelizenz 1.0: private Nutzung kostenlos; firmeninterne Nutzung
kostenlos mit einmaliger E-Mail-Meldung pro Unternehmen. Weitergehende kommerzielle
Verwertung benötigt vorherige Zustimmung. Bei Weitergabe muss der zugehörige
Quellcode mitgegeben werden. Maßgeblich sind LICENSE und NOTICE.

© 2026 Martin Dietl · dietl.martin@mail.de
