# Prüfung der Windows-Portierung

Geprüft am 18.10.2026 auf dem vorhandenen Windows-Rechner:

- Windows-x64-Anwendung erfolgreich mit GCC 16.2.0 und wxWidgets 3.3.4 gebaut.
- Bestehender Protokolltest: CRC, Eingaben, Sollwerte und Spannungslimits bestanden.
- Bestehender Verbindungszustandstest bestanden.
- Bestehender Messwertanzeigetest bestanden.
- Neuer Test für COM-Portnamen einschließlich COM12, Gerätepräfix und ungültigen Namen bestanden.
- Separater GUI-Starttest: Hauptfenster mit COM3-Vorgabe und initialer Zustandsaktualisierung erfolgreich erstellt und beendet.
- Windows-Ressource mit Common-Controls-v6-Manifest eingebunden.

Der Anwender hat die Windows-Version am 18.10.2026 mit seinem SK150 ausprobiert
und die Funktion bestätigt. Welche Einzelfunktionen dabei geprüft wurden, wurde nicht angegeben.

Nicht gezielt geprüft: USB-Abziehen während einer Transaktion, visuelle Darstellung bei unterschiedlichen
Bildschirmauflösungen und der zusätzliche CMake/MSVC-Buildweg.
Die vorhandenen Linux-Pseudoterminaltests wurden auf diesem Windows-Rechner nicht ausgeführt.

Das Paket enthält den vollständigen zugehörigen Anwendungsquellcode im Ordner `source`.
Die Windows-Fassung wird zusammen mit diesem Prüfbericht veröffentlicht.
