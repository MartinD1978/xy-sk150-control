# XY-SK150 für Windows 11 (64 Bit)

Diese Fassung ergänzt die Linux-Version 0.2.1 um native Windows-COM-Ports.
Die Oberfläche, Modbus-Register, Speicherplätze und Schutzprüfungen bleiben erhalten.

## Starten

1. Das gesamte Windows-Paket entpacken. Die mitgelieferten DLLs müssen neben `sk150.exe` bleiben.
2. `sk150.exe` starten.
3. Unter **Verbindung → Schnittstelle auswählen …** den COM-Port des USB-Serielladapters wählen.
   Die Liste lässt sich aktualisieren; alternativ z. B. `COM3` oder `COM12` eingeben.
   Die Portnummer steht auch im Windows-Geräte-Manager unter **Anschlüsse (COM & LPT)**.
4. Voreinstellung: 115200 Baud, 8 Datenbits, keine Parität, 1 Stoppbit, Slave 1.
5. Mit dem Steckerknopf verbinden. Beim Start wird weder automatisch verbunden noch ein Gerätewert geschrieben.

Bei jedem Start ist das Software-Spannungslimit wieder auf 12,00 V aktiviert.
Trennen und Beenden schalten einen eingeschalteten Geräteausgang nicht automatisch ab.
Die Schnittstellenwahl und Speicherplätze werden über wxWidgets im Benutzer-Konfigurationsverzeichnis gespeichert.
Ein fehlender COM-Port kann auf einen fehlenden Adaptertreiber hinweisen.

## Selbst bauen (Visual Studio 2022, CMake und Git)

Die Visual-Studio-Komponente **Desktopentwicklung mit C++** muss installiert sein.
Im Quellcodeordner ausführen:

```powershell
cmake -S . -B build -A x64 -DSK150_FETCH_WX=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Die Anwendung liegt danach in `build/Release/sk150.exe`.
Der erste Build lädt wxWidgets 3.3.4 aus dem offiziellen Repository und baut es statisch mit.
Alternativ eine vorhandene wxWidgets-Installation verwenden und `SK150_FETCH_WX` weglassen.
Die bestehende Linux-Bauanleitung und das Makefile bleiben nutzbar.

## Technische Änderungen

- Windows-Portzugriff über CreateFile, DCB und overlapped ReadFile/WriteFile; exklusives Öffnen.
- Gemeinsames Transaktionszeitlimit von 500 ms mit Abbruch ausstehender E/A.
- COM-Portliste über QueryDosDevice; auch COM-Nummern ab 10 unterstützt.
- Plattformabhängige Portvorgabe und Prüfung gespeicherter Schnittstellen.
- CMake und zusätzlicher Test für Windows-Portnamen.

Der Anwender hat die Windows-Fassung am 18.10.2026 am eigenen SK150 ausprobiert und ihre Funktion bestätigt.
Ein vollständiger Test aller Funktionen und Fehlerfälle ist damit nicht dokumentiert.
Lizenz, Urheberhinweise und Logo-Bedingungen aus LICENSE und NOTICE gelten weiter.
