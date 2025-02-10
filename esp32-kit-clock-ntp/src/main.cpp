//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Includes
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <arduino.h>                 // Arduino Core Library
#include <time.h>                    // Time functions
#include <WiFi.h>                    // WiFi functions for ESP32
#include <WiFiUdp.h>                 // UDP for NTP
#include <U8g2lib.h>                 // Library for OLED display
#include <ESPAsyncWebServer.h>       // Asynchronous Web Server library
#include "credentials.h"             // WiFi credentials

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define FONT_ONE_HEIGHT 8            // Height of smaller font on OLED
#define FONT_TWO_HEIGHT 20           // Height of larger font on OLED
#define NTP_DELAY_COUNT 20           // Delay between NTP requests
#define NTP_PACKET_LENGTH 48         // Length of NTP packet
#define UDP_PORT 4000                // UDP port for NTP communication

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int currentTimeZone = 1;             // Default time zone offset (CET)
String oledLanguage = "de";          // Default language for OLED ("de" = German)

// Network and display objects
char chBuffer[128];                  // General-purpose string buffer
bool bTimeReceived = false;          // Flag to track if NTP time is received
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 16, 15, 4); // OLED display object
WiFiUDP Udp;                         // UDP object for NTP
AsyncWebServer server(80);           // HTTP server object

// Day and month names for display
const char *wochentage_de[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char *wochentage_en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *monate_de[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
const char *monate_en[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Function Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setTimeZone(int timeZoneOffset);              // Updates the timezone offset
void updateDisplay(struct tm *tmPointer);          // Updates the OLED display
String generateHTMLPage();                         // Generates the HTML page for the web server
void handleTimezoneRequest(AsyncWebServerRequest *request); // Handles timezone change requests
void handleLanguageRequest(AsyncWebServerRequest *request); // Handles language change requests

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Setup Function
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    while (!Serial) {
        Serial.print('.');
    }

    // Initialize OLED display
    u8g2.begin();                                 // Start OLED
    u8g2.setFont(u8g2_font_6x10_tr);              // Set default font
    u8g2.setFontRefHeightExtendedText();          // Extend text height
    u8g2.setDrawColor(1);                         // Set drawing color to white
    u8g2.setFontPosTop();                         // Align text to the top
    u8g2.setFontDirection(0);                     // Set text direction to horizontal

    // Connect to WiFi
    Serial.print("NTP clock: connecting to wifi");
    WiFi.begin(chSSID, chPassword);
    
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);                               // Wait for connection
    }

    // Print WiFi connection details
    Serial.println();
    sprintf(chBuffer, "NTP clock: WiFi connected to %s.", chSSID);
    Serial.println(chBuffer);

    // Display WiFi stats on OLED
    u8g2.clearBuffer();                           // Clear the display buffer
    sprintf(chBuffer, "%s", "WiFi Stats:");
    u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 0, chBuffer); // Display title
    char chIp[81];
    WiFi.localIP().toString().toCharArray(chIp, sizeof(chIp) - 1);    // Get local IP
    sprintf(chBuffer, "IP  : %s", chIp);          // Show IP
    u8g2.drawStr(0, FONT_ONE_HEIGHT * 2, chBuffer);
    sprintf(chBuffer, "SSID: %s", chSSID);        // Show SSID
    u8g2.drawStr(0, FONT_ONE_HEIGHT * 3, chBuffer);
    sprintf(chBuffer, "RSSI: %d", WiFi.RSSI());   // Show RSSI
    u8g2.drawStr(0, FONT_ONE_HEIGHT * 4, chBuffer);
    u8g2.drawStr(0, FONT_ONE_HEIGHT * 6, "Awaiting NTP time...");
    u8g2.sendBuffer();                            // Update OLED

    // Start UDP for NTP communication
    Udp.begin(UDP_PORT);

    // Configure the web server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = generateHTMLPage();
        request->send(200, "text/html", html);
    });

    server.on("/setTimezone", HTTP_GET, [](AsyncWebServerRequest *request) {
        handleTimezoneRequest(request);
    });

    server.on("/setLanguage", HTTP_GET, [](AsyncWebServerRequest *request) {
        handleLanguageRequest(request);
    });

    server.begin();                               // Start the server
    Serial.println("Webserver gestartet.");

    // Initialize NTP time
    setTimeZone(currentTimeZone);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main Loop
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
    static byte chNtpPacket[NTP_PACKET_LENGTH];   // Buffer for NTP packet
    static int nNtpDelay = NTP_DELAY_COUNT;      // Counter for NTP request delay

    // Request and process NTP time if not received yet
    if (!bTimeReceived) {
        nNtpDelay++;
        if (nNtpDelay >= NTP_DELAY_COUNT) {
            nNtpDelay = 0;
            memset(chNtpPacket, 0, NTP_PACKET_LENGTH); // Clear packet
            chNtpPacket[0] = 0b11100011;          // NTP packet header
            IPAddress ipNtpServer(116, 203, 244, 102); // NTP server ubnt.pool.ntp.org
            Udp.beginPacket(ipNtpServer, 123);    // Send packet to NTP server
            Udp.write(chNtpPacket, NTP_PACKET_LENGTH);
            Udp.endPacket();
        }

        if (Udp.parsePacket()) {                  // Process incoming NTP response
            Udp.read(chNtpPacket, NTP_PACKET_LENGTH);
            unsigned long secsSince1900 = (unsigned long)chNtpPacket[40] << 24 |
                                          (unsigned long)chNtpPacket[41] << 16 |
                                          (unsigned long)chNtpPacket[42] << 8 |
                                          (unsigned long)chNtpPacket[43];
            struct timeval tv = {(secsSince1900 - 2208988800UL), 0}; // Convert NTP time
            settimeofday(&tv, NULL);             // Set system time
            bTimeReceived = true;
        }
    }

    // Update OLED display
    if (bTimeReceived) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm *tmPointer = localtime(&tv.tv_sec);
        updateDisplay(tmPointer);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper Functions
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setTimeZone(int timeZoneOffset) {
    currentTimeZone = timeZoneOffset;            // Update global timezone
    configTime(currentTimeZone * 3600, 0, "ubnt.pool.ntp.org"); // Set timezone and NTP server
    Serial.printf("Zeitzone aktualisiert auf: %d\n", currentTimeZone);
}

void updateDisplay(struct tm *tmPointer) {
    u8g2.clearBuffer();

    // Line 1: Day of the week
    const char **wochentage = (oledLanguage == "de") ? wochentage_de : wochentage_en;
    const char *wochentag = wochentage[tmPointer->tm_wday];
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(64 - (u8g2.getStrWidth(wochentag) / 2), 2, wochentag);

    // Line 2: Date
    const char **monate = (oledLanguage == "de") ? monate_de : monate_en; // Choose month names based on language.
    if (oledLanguage == "de") {
    // German format: Day. Month Year
    sprintf(chBuffer, "%d. %s %d", tmPointer->tm_mday, monate[tmPointer->tm_mon], tmPointer->tm_year + 1900);
    } else {
    // English format: Year, Month Day
    sprintf(chBuffer, "%d, %s %d.", tmPointer->tm_year + 1900, monate[tmPointer->tm_mon], tmPointer->tm_mday);
    }
    // Center-align the date on the display at y=15.
    u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 15, chBuffer);

    // Line 3: Time (hour:minute:second)
    // Determine the time format based on the language setting.
    if (oledLanguage == "de") {
        // For German: Use 24-hour format.
        u8g2.setFont(u8g2_font_logisoso18_tf); // Use a large font for the time.
        sprintf(chBuffer, "%02d:%02d:%02d", tmPointer->tm_hour, tmPointer->tm_min, tmPointer->tm_sec); // Format: HH:MM:SS.
    } else {
        // For English: Use 12-hour format with AM/PM.
        int hour = tmPointer->tm_hour; // Get the current hour.
        const char *am_pm = "AM"; // Default to AM.
        if (hour >= 12) {
            am_pm = "PM"; // Set to PM for hours 12 and above.
            if (hour > 12) hour -= 12; // Convert to 12-hour format.
        }
        if (hour == 0) hour = 12; // Handle midnight as 12 AM.
        u8g2.setFont(u8g2_font_logisoso18_tf); // Use a large font for the time.
        sprintf(chBuffer, "%02d:%02d:%02d%s", hour, tmPointer->tm_min, tmPointer->tm_sec, am_pm); // Format: HH:MM:SS AM/PM.
    }
    u8g2.drawStr(64 - (u8g2.getStrWidth(chBuffer) / 2), 63 - FONT_TWO_HEIGHT, chBuffer); // Center-align the time on the display

    u8g2.sendBuffer(); // Send the buffer to the OLED to update the display.
}

String generateHTMLPage() {
    String html = "<html><body>";
    
    if (oledLanguage == "de") {
        html += "<h1>ESP32 Einstellungen</h1>";
        html += "<form action='/setTimezone' method='get'>";
        html += "Zeitzone (z.B. CET = 1): <input type='number' name='tz' required>";
        html += "<button type='submit'>Zeitzone setzen</button>";
        html += "</form>";
        html += "<form action='/setLanguage' method='get'>";
        html += "Sprache: ";
        html += "<input type='radio' name='lang' value='de'";
        if (oledLanguage == "de") html += " checked";
        html += "> Deutsch";
        html += "<input type='radio' name='lang' value='en'";
        if (oledLanguage == "en") html += " checked";
        html += "> Englisch";
        html += "<button type='submit'>Sprache aendern</button>";
        html += "</form>";
    } else if (oledLanguage == "en") {
        html += "<h1>ESP32 Settings</h1>";
        html += "<form action='/setTimezone' method='get'>";
        html += "Timezone (e.g., CET = 1): <input type='number' name='tz' required>";
        html += "<button type='submit'>Set Timezone</button>";
        html += "</form>";
        html += "<form action='/setLanguage' method='get'>";
        html += "Language: ";
        html += "<input type='radio' name='lang' value='de'";
        if (oledLanguage == "de") html += " checked";
        html += "> German";
        html += "<input type='radio' name='lang' value='en'";
        if (oledLanguage == "en") html += " checked";
        html += "> English";
        html += "<button type='submit'>Change Language</button>";
        html += "</form>";
    }

    html += "</body></html>";
    return html;
}


void handleTimezoneRequest(AsyncWebServerRequest *request) {
    if (request->hasParam("tz")) {
        String tzParam = request->getParam("tz")->value();
        int newTimeZone = tzParam.toInt();
        setTimeZone(newTimeZone);
        request->send(200, "text/plain", "Zeitzone auf " + String(newTimeZone) + " gesetzt.");
    } else {
        request->send(400, "text/plain", "Fehler: Keine Zeitzone angegeben.");
    }
}

void handleLanguageRequest(AsyncWebServerRequest *request) {
    if (request->hasParam("lang")) {
        oledLanguage = request->getParam("lang")->value();
        request->send(200, "text/plain", "Sprache auf " + oledLanguage + " gesetzt.");
    } else {
        request->send(400, "text/plain", "Fehler: Keine Sprache angegeben.");
    }
}
