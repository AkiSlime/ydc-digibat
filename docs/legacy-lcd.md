# Ancien montage LCD 16 pins

Le firmware actuel est passe en mode OLED uniquement. Cette note garde l'ancien cablage LCD au cas ou tu voudrais y revenir plus tard.

Ancien cablage LCD HD44780 en mode 4 bits:

| LCD | ESP32 / alim |
| --- | --- |
| 1 VSS | GND |
| 2 VDD | 5V ou 3V3 selon module |
| 3 VO | curseur potentiometre 10k |
| 4 RS | GPIO 32 |
| 5 RW | GND |
| 6 E | GPIO 18 |
| 11 D4 | GPIO 27 |
| 12 D5 | GPIO 26 |
| 13 D6 | GPIO 25 |
| 14 D7 | GPIO 33 |
| 15 A | VCC via resistance si necessaire |
| 16 K | GND |

Ancien rendu LCD:

```text
CPU:0% DISK:7%
RAM:18% 2.9GB
```
