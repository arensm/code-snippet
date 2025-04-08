# Universal U-Bracket

A customizable U-shaped bracket for mounting devices like power supplies, USB hubs, and small electronics. Designed in OpenSCAD, this model can be easily adapted by adjusting key parameters.

---

## 🔧 What is it?
This is a universal U-shaped bracket – designed for things like power supplies, USB hubs, small electronic devices, etc. The construction is mirrored on both sides (using the `mirror()` command), making it symmetrical.

### 🇩🇪 Was ist das?
Dies ist eine universelle U-förmige Halterung – gedacht für Netzteile, USB-Hubs, kleine elektronische Geräte usw. Die Konstruktion ist durch den `mirror()`-Befehl beidseitig gespiegelt und somit symmetrisch.

---

## 📐 Basic Structure

### 1. Front Panel
A flat beam formed by two `cube()` elements joined with `hull()` to create a smooth face. This is the surface where the device will rest.

### 🇩🇪 Frontplatte
Ein flacher Balken, geformt aus zwei `cube()`-Elementen, die mit `hull()` verbunden sind, um eine glatte Fläche zu erzeugen. Dies ist die Fläche, an der das Gerät anliegt.

### 2. Mounting Tab (Bottom)
A small extension directly attached to the front panel, with a screw hole for fastening.
- Accepts either cylindrical or countersunk screw heads depending on the `headType` parameter.
- Screw head and shaft diameters are adjustable via `headD` and `screwD`.

### 🇩🇪 Montagelasche (unten)
Eine kleine Lasche direkt an der Frontplatte, mit Schraubenloch zur Befestigung.
- Unterstützt Zylinder- oder Senkkopfschrauben je nach `headType`.
- Kopf- und Schaftdurchmesser sind über `headD` und `screwD` einstellbar.

### 3. Side Panel
Vertical walls perpendicular to the front panel.
- Shaped like a “U” or “L” when viewed from the side.
- Provides lateral support.

### 🇩🇪 Seitenplatte
Vertikale Wände, rechtwinklig zur Frontplatte.
- Von der Seite wie ein "U" oder "L" geformt.
- Bietet seitliche Stabilität.

### 4. Hold-Down Tab (Top)
A top-side plate that prevents the device from slipping upward.
- Can be slightly flexible or mounted with screws for extra security.

### 🇩🇪 Fixierlasche (oben)
Eine obere Halteplatte, die verhindert, dass das Gerät nach oben herausrutscht.
- Kann leicht flexibel sein oder mit Schrauben fixiert werden.

---

## 🕳️ Holes
- **Screw Hole:** Located at the bottom mounting tab.
- **Center Hole:** Rectangular opening in the center for airflow or cable passthrough.

### 🇩🇪 Löcher
- **Schraubenloch:** Seitlich an der unteren Lasche.
- **Zentrales Loch:** Rechteckige Öffnung in der Mitte für Luftzirkulation oder Kabeldurchführung.

---

## 🔁 Symmetry
The bracket is mirrored with the command:
```scad
for(m=[0:1]) mirror([m,0,0])
```
This creates two identical side arms, forming the U-shape.

### 🇩🇪 Spiegelung
Die Halterung wird mit folgendem Befehl gespiegelt:
```scad
for(m=[0:1]) mirror([m,0,0])
```
Dies erzeugt zwei symmetrische Seitenarme und ergibt die U-Form.

---

## 📦 Example Dimensions
Values can be customized to fit various devices:
```scad
x = 63; // Width of the device (mm)
z = 60; // Height of the device (mm)
y = 15; // Depth of the bracket (mm)
```
Other key parameters:
```scad
THK = 4;     // Non-structural thickness
thk = 3;     // Structural part thickness
screwD = 4;  // Screw shaft diameter
headD = 10;  // Screw head diameter
headType = "BUTTON"; // or "TAPER"
```

### 🇩🇪 Beispielhafte Maße
Diese Werte können für verschiedene Geräte angepasst werden:
```scad
x = 63; // Breite des Geräts (mm)
z = 60; // Höhe des Geräts (mm)
y = 15; // Tiefe der Halterung (mm)
```
Weitere wichtige Parameter:
```scad
THK = 4;     // Dicke nicht-struktureller Teile
thk = 3;     // Dicke struktureller Teile
screwD = 4;  // Durchmesser der Schraubenachse
headD = 10;  // Durchmesser des Schraubenkopfs
headType = "BUTTON"; // oder "TAPER"
```

---

## 🖼️ Visual Overview
Imagine a U-shaped bracket made from plastic or metal:
- Two side arms with screw tabs
- A front face for device support
- Optional top clamp to hold the device in place

### 🇩🇪 Visuelle Vorstellung
Stellen Sie sich eine U-förmige Halterung aus Kunststoff oder Metall vor:
- Zwei Seitenarme mit Schraublaschen
- Eine Frontplatte zur Geräteauflage
- Eine optionale obere Fixierung zur Sicherung des Geräts

---

## 📁 File Information
```
echo(str("File name: Universal U Bracket ", x, "x", z));
```
This prints the configured size during rendering.

### 🇩🇪 Dateiinformation
```
echo(str("Dateiname: Universelle U-Halterung ", x, "x", z));
```
Gibt die eingestellte Größe während des Renderns aus.

---

## 🛠 Customization
Easily adaptable by changing the variables at the top of the SCAD file.
Perfect for 3D printing holders tailored to your hardware.

### 🇩🇪 Anpassung
Leicht anpassbar durch Ändern der Variablen am Anfang der SCAD-Datei.
Ideal für 3D-gedruckte Halterungen, die genau auf Ihre Hardware abgestimmt sind.

---

## 📄 License
Feel free to use, remix, and share under [MIT License] or similar open source terms.

