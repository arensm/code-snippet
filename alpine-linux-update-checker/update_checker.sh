#!/bin/sh

# Definiere Dateipfade für Server-Liste, Log-Datei und Konfigurationsdatei
# Define file paths for server list, log file, and configuration file
SERVER_FILE="server_list.txt"
LOG_FILE="update_check.log"
CONFIG_FILE="pushover_config.txt"

# Überprüfe, ob die Konfigurationsdatei existiert
# Check if the configuration file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Fehler: Konfigurationsdatei $CONFIG_FILE nicht gefunden!"
    exit 1
fi

# Lade die Konfigurationsdatei
# Load the configuration file
. "$CONFIG_FILE"

# Überprüfe, ob Pushover-Zugangsdaten in der Konfigurationsdatei definiert sind
# Check if Pushover credentials are defined in the configuration file
if [ -z "$PUSHOVER_USER" ] || [ -z "$PUSHOVER_TOKEN" ]; then
    echo "Fehler: PUSHOVER_USER oder PUSHOVER_TOKEN nicht in $CONFIG_FILE definiert!"
    exit 1
fi

# Funktion zum Loggen von Nachrichten mit Datum
# Function to log messages with date
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S'): $1" | tee -a "$LOG_FILE"
}

# Funktion zum Loggen von Nachrichten ohne Datum
# Function to log messages without date
log_no_date() {
    echo "$1" | tee -a "$LOG_FILE"
}

# Funktion zum Senden von Pushover-Benachrichtigungen
# Function to send Pushover notifications
send_pushover() {
    response=$(wget -qO- --post-data="token=${PUSHOVER_TOKEN}&user=${PUSHOVER_USER}&message=${1}&html=1" https://api.pushover.net/1/messages.json)
    if echo "$response" | grep -q '"status":1'; then
        log "Pushover-Nachricht erfolgreich gesendet"
    else
        log "Fehler beim Senden der Pushover-Nachricht: $response"
    fi
}

# Überprüfe, ob die Server-Liste existiert
# Check if the server list exists
if [ ! -f "$SERVER_FILE" ]; then
    log "Fehler: Datei $SERVER_FILE nicht gefunden!"
    exit 1
fi

# Starte die Server-Überprüfung
# Start the server check
log "Starte Server-Überprüfung..."
log_no_date "============================"

# Lese die Server-Liste
# Read the server list
SERVERS=$(cat "$SERVER_FILE")

# Iteriere über jeden Server in der Liste
# Iterate over each server in the list
for SERVER in $SERVERS; do
    [ -z "$SERVER" ] && continue

    log "Überprüfe Server: ${SERVER}"
    log_no_date "----------------------------"

    # Überprüfe, ob apk auf dem Server verfügbar ist
    # Check if apk is available on the server
    if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "${SERVER}" "command -v apk >/dev/null 2>&1"; then
        log "  Fehler: apk nicht gefunden auf ${SERVER}. Ist es ein Alpine Linux System?"
        continue
    fi

    # Führe apk update und upgrade aus und erfasse die Ergebnisse
    # Run apk update and upgrade and capture the results
    UPDATE_INFO=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "${SERVER}" "apk update >/dev/null 2>&1 && apk upgrade -s 2>/dev/null")
    UPDATES=$(echo "$UPDATE_INFO" | grep -E 'Upgrading' | wc -l)
    UPDATE_LIST=$(echo "$UPDATE_INFO" | grep -E 'Upgrading' | awk '{print "    - " $3 " (" $4 " -> " $6 ")"}')

    # Überprüfe, ob die SSH-Verbindung erfolgreich war
    # Check if the SSH connection was successful
    if [ $? -ne 0 ]; then
        log "  Fehler beim Abrufen von Updates für ${SERVER}!"
        continue
    fi

    # Wenn Updates verfügbar sind, protokolliere sie und sende eine Benachrichtigung
    # If updates are available, log them and send a notification
    if [ "$UPDATES" -gt 0 ]; then
        log "  ${UPDATES} Updates verfügbar:"
        echo "$UPDATE_LIST" | while IFS= read -r line; do
            log_no_date "$line"
        done
        MESSAGE="<b>Server ${SERVER}:</b> Es sind ${UPDATES} Updates verfügbar!<br><br><u>Verfügbare Updates:</u><br>${UPDATE_LIST//$'\n'/<br>}"
        send_pushover "${MESSAGE}"
    else
        log "  Keine Updates verfügbar für ${SERVER}."
    fi

    log_no_date "----------------------------"
    sleep 1

done

# Beende die Server-Überprüfung
# Finish the server check
log_no_date "============================"
log "Server-Überprüfung abgeschlossen."

