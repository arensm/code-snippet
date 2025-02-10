#!/bin/bash
# EN: Specifies that this script should be executed by the bash shell
# DE: Legt fest, dass dieses Skript von der Bash-Shell ausgeführt werden soll

set -e  
# EN: Stops script execution if any command returns an error
# DE: Bricht das Skript ab, wenn ein Befehl einen Fehler zurückgibt

set -x  
# EN: Enables debug mode - prints each command before execution
# DE: Aktiviert den Debug-Modus - gibt jeden Befehl vor der Ausführung aus

# EN: Define log file
# DE: Log-Datei definieren
LOG_DATEI="kopier_log.txt"
# EN: Create/overwrite log file with start timestamp
# DE: Erstellt/überschreibt Log-Datei mit Startzeitstempel
echo "Starte Kopiervorgang: $(date)" > "$LOG_DATEI"

# EN: Define configuration file name
# DE: Konfigurationsdateinamen definieren
CONFIG_DATEI="config.cfg"

# EN: Check if configuration file exists
# DE: Prüft, ob die Konfigurationsdatei existiert
if [[ ! -f "$CONFIG_DATEI" ]]; then
    # EN: Log error and exit if config file not found
    # DE: Protokolliert Fehler und beendet, wenn Konfigurationsdatei nicht gefunden
    echo "Fehler: Konfigurationsdatei $CONFIG_DATEI nicht gefunden!" >> "$LOG_DATEI"
    exit 1
fi

# EN: Source (load) the configuration file
# DE: Lädt die Konfigurationsdatei
source "$CONFIG_DATEI"

# EN: Check if required directories are set in config
# DE: Prüft, ob erforderliche Verzeichnisse in der Konfiguration gesetzt sind
if [[ -z "$HAUPTVERZEICHNIS" || -z "$ZIELVERZEICHNIS" ]]; then
    # EN: Log error and exit if directories not set
    # DE: Protokolliert Fehler und beendet, wenn Verzeichnisse nicht gesetzt sind
    echo "Fehler: HAUPTVERZEICHNIS oder ZIELVERZEICHNIS nicht in $CONFIG_DATEI gesetzt!" >> "$LOG_DATEI"
    exit 1
fi

# EN: Log the paths for debugging
# DE: Protokolliert die Pfade für Debugging-Zwecke
echo "HAUPTVERZEICHNIS: $HAUPTVERZEICHNIS" >> "$LOG_DATEI"
echo "ZIELVERZEICHNIS: $ZIELVERZEICHNIS" >> "$LOG_DATEI"

# EN: Create target directory if it doesn't exist
# DE: Erstellt das Zielverzeichnis, falls es nicht existiert
mkdir -p "$ZIELVERZEICHNIS"

# EN: Create verification directory
# DE: Erstellt das Prüfverzeichnis
PRUEF_VERZEICHNIS="$ZIELVERZEICHNIS/00_zupruefen"
mkdir -p "$PRUEF_VERZEICHNIS"

# EN: Function to extract author and book title
# DE: Funktion zum Extrahieren von Autor und Buchtitel
extrahiere_autoren() {
    # EN: Get filename as parameter
    # DE: Erhält Dateinamen als Parameter
    local dateiname="$1"
    
    # EN: Extract author part (everything before " - ", but keep hyphens in names)
    # DE: Extrahiert den Autor-Teil (alles vor " - ", aber behält Bindestriche in Namen)
    local name_teil
    local rest
    
    # EN: Split filename at " - " and get first part (author)
    # DE: Teilt Dateinamen bei " - " und nimmt ersten Teil (Autor)
    name_teil=$(echo "$dateiname" | sed 's/ - /\n/' | head -n1 | sed 's/[[:space:]]*$//')
    # EN: Get everything after " - " and remove .epub extension
    # DE: Nimmt alles nach " - " und entfernt .epub-Erweiterung
    rest=$(echo "$dateiname" | sed 's/ - /\n/' | tail -n +2 | sed 's/^[[:space:]]*//;s/\.epub$//')
    
    # EN: If multiple authors (separated by &), take only the first
    # DE: Wenn mehrere Autoren (getrennt durch &), nimm nur den ersten
    if echo "$name_teil" | grep -q "&"; then
        name_teil=$(echo "$name_teil" | awk -F'&' '{print $1}' | sed 's/[[:space:]]*$//')
    fi
    
    # EN: Split name into words
    # DE: Teilt den Namen in Wörter
    read -r -a worte <<< "$name_teil"
    local wort_anzahl=${#worte[@]}
    
    # EN: Last word is the surname
    # DE: Letztes Wort ist der Nachname
    local nachname="${worte[$wort_anzahl-1]}"
    
    # EN: All other words form the first name
    # DE: Alle anderen Wörter bilden den Vornamen
    local vorname=""
    for ((i=0; i<wort_anzahl-1; i++)); do
        if [ -z "$vorname" ]; then
            vorname="${worte[$i]}"
        else
            vorname="$vorname ${worte[$i]}"
        fi
    done
    
    # EN: Ensure umlauts are correct
    # DE: Stellt sicher, dass Umlaute korrekt sind
    nachname=$(echo "$nachname" | sed -e 's/ae/ä/g' -e 's/oe/ö/g' -e 's/ue/ü/g' -e 's/Ae/Ä/g' -e 's/Oe/Ö/g' -e 's/Ue/Ü/g')
    vorname=$(echo "$vorname" | sed -e 's/ae/ä/g' -e 's/oe/ö/g' -e 's/ue/ü/g' -e 's/Ae/Ä/g' -e 's/Oe/Ö/g' -e 's/Ue/Ü/g')
    
    # EN: Return surname, first name, and rest if both names exist
    # DE: Gibt Nachname, Vorname und Rest zurück, wenn beide Namen existieren
    if [[ -n "$vorname" && -n "$nachname" ]]; then
        echo "$nachname, $vorname, $rest"
    else
        echo "Fehler: Konnte Namen nicht korrekt extrahieren aus $dateiname"
    fi
}

# EN: Find and process all .epub files in source directory
# DE: Findet und verarbeitet alle .epub-Dateien im Quellverzeichnis
find "$HAUPTVERZEICHNIS" -type f -name "*.epub" -print0 | while IFS= read -r -d '' datei; do
    # EN: Get filename without path
    # DE: Holt Dateinamen ohne Pfad
    dateiname=$(basename "$datei")

    # EN: Replace umlauts manually
    # DE: Ersetzt Umlaute manuell
    dateiname=$(echo "$dateiname" | sed -e 's/ä/ae/g' -e 's/ö/oe/g' -e 's/ü/ue/g' -e 's/Ä/Ae/g' -e 's/Ö/Oe/g' -e 's/Ü/Ue/g' -e 's/ß/ss/g')

    # EN: Log current file processing
    # DE: Protokolliert aktuelle Dateiverarbeitung
    echo "Verarbeite Datei: $dateiname" >> "$LOG_DATEI"

    # EN: Extract author and title
    # DE: Extrahiert Autor und Titel
    extrahierte_daten=$(extrahiere_autoren "$dateiname")
    
    # EN: Handle extraction errors
    # DE: Behandelt Extraktionsfehler
    if [[ "$extrahierte_daten" == Fehler* ]]; then
        echo "Fehler beim Extrahieren von Autoren und Titel aus $dateiname" >> "$LOG_DATEI"
        echo "Kopiere $dateiname in Prüfverzeichnis" >> "$LOG_DATEI"
        cp "$datei" "$PRUEF_VERZEICHNIS/"
        continue
    fi

    # EN: Read extracted data in correct order
    # DE: Liest extrahierte Daten in korrekter Reihenfolge
    if ! IFS=', ' read -r nachname vorname buchtitel <<< "$extrahierte_daten"; then
        echo "Fehler beim Verarbeiten der extrahierten Daten für $dateiname" >> "$LOG_DATEI"
        echo "Kopiere $dateiname in Prüfverzeichnis" >> "$LOG_DATEI"
        cp "$datei" "$PRUEF_VERZEICHNIS/"
        continue
    fi

    # EN: Check if all required data exists
    # DE: Prüft ob alle erforderlichen Daten vorhanden sind
    if [[ -z "$nachname" || -z "$vorname" || -z "$buchtitel" ]]; then
        echo "Unvollständige Daten für $dateiname" >> "$LOG_DATEI"
        echo "Kopiere $dateiname in Prüfverzeichnis" >> "$LOG_DATEI"
        cp "$datei" "$PRUEF_VERZEICHNIS/"
        continue
    fi

    # EN: Get first letter of surname (uppercase)
    # DE: Holt ersten Buchstaben des Nachnamens (Großbuchstabe)
    erster_buchstabe=$(echo "$nachname" | cut -c1 | tr '[:lower:]' '[:upper:]')

    # EN: Create target directories
    # DE: Erstellt Zielverzeichnisse
    buchstaben_verzeichnis="$ZIELVERZEICHNIS/$erster_buchstabe"
    autor_verzeichnis="$buchstaben_verzeichnis/$nachname, $vorname"

    # EN: Log directory creation
    # DE: Protokolliert Verzeichniserstellung
    echo "Erstelle Verzeichnis für $nachname, $vorname unter: $autor_verzeichnis" >> "$LOG_DATEI"

    # EN: Create author directory
    # DE: Erstellt Autorverzeichnis
    mkdir -p "$autor_verzeichnis"

    # EN: Copy file to correct directory
    # DE: Kopiert Datei in das richtige Verzeichnis
    cp "$datei" "$autor_verzeichnis/"

    # EN: Log successful copy operation
    # DE: Protokolliert erfolgreichen Kopiervorgang
    echo "Kopiert: $dateiname → $autor_verzeichnis/" >> "$LOG_DATEI"
done

# EN: Log completion message
# DE: Protokolliert Abschlussmeldung
echo "Fertig! Alle .epub-Dateien wurden sortiert und kopiert." >> "$LOG_DATEI"

