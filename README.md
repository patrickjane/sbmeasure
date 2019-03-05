____
Seite befindet sich noch im Aufbau.
____

# Übersicht
Das SK Messsystem ist ein auf Arduino basiertes, batteriebetriebenes Zeiterfassungssystem für z. B. Seifenkistenrennen. Das System erlaubt die genaue Erfassung von Zeiten mittels einer *Lichtschranke*. Dabei wird die Lichtschranke selbst im Messmodul am Fahrzeug angebracht, und erkennt Reflektoren am Boden beim Überfahren.

Das Messmodul erlaubt die Erfassung und Speicherung mehrerer Messungen (standardmässig bis zu 30), welche im EEPROM gespeichert

# Hardware
Benötigt werden folgende Komponenten:

- Arduino UNO R3 Orginal
- Keypad-LCD Shild 1602 für Arduino
- Reflexionslichtschranke Sick WL9-2P630S12

Bei der Verwendung von nachgebauten Arduinos kommen evtl. schlechtere Zeitmesser zum Einsatz, welche zu einer deutlichen Abweichung der Zeitmessung führen können.

# Software
Der angehängte Arduino Sketch `soapbox_measurement.ino` beinhaltet die Software für die Zeitmessung. Nach dem Upload zeigt das Messsystem die aktuelle Versionsnummer:

```
|SK Messsystem   |
|v1.0.0          |
```

und startet dann im Hauptmenü:

```
|> Zeitmessung   |
|  Ergebnisse    |
```
