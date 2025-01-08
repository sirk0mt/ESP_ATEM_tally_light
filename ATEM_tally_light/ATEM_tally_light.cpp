/*
    Copyright (C) 2023 Aron N. Het Lam, aronhetlam@gmail.com

    This program makes an ESP8266 into a wireless tally light system for ATEM switchers,
    by using Kasper Skårhøj's (<https://skaarhoj.com>) ATEM client libraries for Arduino.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ATEM_tally_light.hpp"
//#include "logo.h"

#ifndef VERSION
#define VERSION "dev"
#endif

// #define DEBUG_LED_STRIP
#define FASTLED_ALLOW_INTERRUPTS 0

#ifndef CHIP_FAMILY
#define CHIP_FAMILY "Unknown"
#endif

#ifndef VERSION
#define VERSION "Unknown"
#endif

#ifdef TALLY_TEST_SERVER
#define DISPLAY_NAME "Tally Test server"
#else
#define DISPLAY_NAME "Tally Light"
#endif

//Include libraries:
#if ESP32
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Updater.h>
#ifndef UPDATE_SIZE_UNKNOWN
    #define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif
#endif

#include <EEPROM.h>
#include <ATEMmin.h>
#include <TallyServer.h>

#if ESP32
//Define LED1 color pins
#ifndef PIN_RED
#define PIN_RED   32
#endif
#ifndef PIN_GREEN
#define PIN_GREEN 33
#endif
#ifndef PIN_BLUE
#define PIN_BLUE  25
#endif

#else // ESP8266
//Define LED1 color pins
#ifndef PIN_RED
#define PIN_RED    2  // D4
#endif
#ifndef PIN_GREEN
#define PIN_GREEN  4  // D2
#endif
#ifndef PIN_BLUE
#define PIN_BLUE   5  // D1
#endif

#endif

//Define LED colors
#define LED_OFF     0
#define LED_RED     1
#define LED_GREEN   2
#define LED_BLUE    3
#define LED_YELLOW  4
#define LED_PINK    5
#define LED_WHITE   6
#define LED_ORANGE  7

//Define states
#define STATE_STARTING                  0
#define STATE_CONNECTING_TO_WIFI        1
#define STATE_CONNECTING_TO_SWITCHER    2
#define STATE_RUNNING                   3

//Define modes of operation
#define MODE_NORMAL                     1
#define MODE_PREVIEW_STAY_ON            2
#define MODE_PROGRAM_ONLY               3
#define MODE_ON_AIR                     4

#define TALLY_FLAG_OFF                  0
#define TALLY_FLAG_PROGRAM              1
#define TALLY_FLAG_PREVIEW              2

//Initialize global variables
#if ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

#ifndef TALLY_TEST_SERVER
ATEMmin atemSwitcher;
#else
int tallyFlag = TALLY_FLAG_OFF;
#endif

TallyServer tallyServer;

ImprovWiFi improv(&Serial);

uint8_t state = STATE_STARTING;

//Define struct for holding tally settings (mostly to simplify EEPROM read and write, in order to persist settings)
struct Settings {
    char tallyName[32] = "";
    uint8_t tallyNo;
    uint8_t tallyModeLED;
    bool staticIP;
    IPAddress tallyIP;
    IPAddress tallySubnetMask;
    IPAddress tallyGateway;
    IPAddress switcherIP;
    uint8_t ledBrightness;
};

Settings settings;

bool firstRun = true;

int bytesAvailable = false;
uint8_t readByte;

//Commented out for users without batteries
// long secLoop = 0;
// int lowLedCount = 0;
// bool lowLedOn = false;
// double uBatt = 0;
// char buffer[3];

void onImprovWiFiErrorCb(ImprovTypes::Error err)
{

}

void onImprovWiFiConnectedCb(const char *ssid, const char *password)
{

}

String htmlHead(bool redirectToMain) {
    String head ="<head>"
    "<meta charset=\"ASCII\">"
    "<meta name=\"viewport\"content=\"width=device-width,initial-scale=1.0\">"
    "<title>Tally Light setup</title>"
    "<style>"
        "a{color:#0F79E0}"
        "body {"
            "font-family: sans-serif;"
            "background-color: #f4f4f4;"
            "margin: 60px;"
            "display: flex;"
            "flex-direction: column;"
            "align-items: center;"
        "}"
        ".container {"
            "background-color: white;"
            "border-radius: 5px;"
            "box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);"
            "padding: 20px;"
            "margin-bottom: 20px;"
            "width: 600px;"
            "max-width: 90%;"
        "}"
        ".logocontainer {"
            "background-color: white;"
            "border-radius: 5px;"
            "box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);"
            "padding: 10px;"
            "margin-bottom: 20px;"
            "max-width: 600px;"
        "}"
        ".logoimg {"
            "max-width: 600px;"
        "}"
        "h1, h2 {"
            "color: #333;"
            "margin-bottom: 10px;"
        "}"
        "label {"
            "display: inline;"
            "margin-bottom: 5px;"
            "font-weight: bold;"
        "}"
        "input[type=\"text\"],"
        "input[type=\"password\"],"
        "input[type=\"checkbox\"],"
        "input[type=\"file\"],"
        "input[type=\"number\"],"
        "select {"
            "width: 100%;"
            "padding: 8px;"
            "margin-bottom: 15px;"
            "box-sizing: border-box;"
        "}"
        "input[type=\"checkbox\"] {"
            "width: auto;"
            "margin-right: 10px;"
        "}"
        ".ip-fields {"
            "display: flex;"
            "justify-content: space-between;"
        "}"
        ".ip-fields input {"
            "width: calc(23% - 10px);"
            "margin-right: 5px;"
        "}"
        "input[type=\"submit\"],"
        "button {"
            "padding: 10px 20px;"
            "background-color: #28a745;"
            "color: white;"
            "border: none;"
            "border-radius: 5px;"
            "cursor: pointer;"
            "transition: background-color 0.3s ease;"
            "margin: 5px 5px 5px"
        "}"
        "input[type=\"submit\"]:hover,"
        "button:hover {"
            "background-color: #218838;"
        "}"
        ".status p {"
            "margin-bottom: 5px;"
        "}"
        "footer {"
            "position: fixed;"
            "bottom: 0;"
            "left: 0;"
            "width: 100%;"
            "background-color: #333;"
            "color: white;"
            "text-align: center;"
        "}"
    "</style>";

    if (redirectToMain) {
        head += "<meta http-equiv=\"refresh\" content=\"5;url=/\" /></head>";
    } else {
        head += "</head>";
    }

    return head;
}
    

String htmlFooter = "<footer>"
    "&nbsp;&copy; 2025 <a href=\"https://github.com/sirk0mt\">Mateusz Sirko</a> for <a href=\"https://deltapix.pl/\">DELTA-PIX</a><br>"
    "&nbsp;Based on <a href=\"https://aronhetlam.github.io/\">Aron N. Het Lam</a> project and ATEM libraries for Arduino by <a href=\"https://www.skaarhoj.com/\">SKAARHOJ</a><br>"
    "Ver: "+ String(VER) + " - Compilation " + String(__DATE__) + " " + String(__TIME__) +"<br>"
"</footer>";

String getConnectionStatusString() {
    switch (WiFi.status()) {
        case WL_CONNECTED:
            return "Connected to network";
        case WL_NO_SSID_AVAIL:
            return  "Network not found";
        case WL_CONNECT_FAILED:
            return  "Invalid password";
        case WL_IDLE_STATUS:
            return "Changing state...";
        case WL_DISCONNECTED:
            return  "Station mode disabled";
#if ESP32
        default:
#else
        case -1:
#endif
            return  "Timeout";
    }
}

String getAtemStatusString() {
#ifndef TALLY_TEST_SERVER
    if (atemSwitcher.isRejected())
        return "Connection rejected - No empty spot";
    else if (atemSwitcher.isConnected())
        return "Connected"; // - Wating for initialization";
    else if (WiFi.status() == WL_CONNECTED)
        return "Disconnected - No response from switcher";
    else
        return "Disconnected - Waiting for WiFi";
#endif
    return "THIS IS TEST SERVER";
}

String getAtemIpString() {
#ifndef TALLY_TEST_SERVER
    return (String)settings.switcherIP[0] + '.' + settings.switcherIP[1] + '.' + settings.switcherIP[2] + '.' + settings.switcherIP[3];
#endif
    return "THIS IS TEST SERVER";
}

String getHostMdnsName() {
#if ESP32
    return (String)WiFi.getHostname();
#else
    return (String)WiFi.hostname();
#endif
}

String ifOptionSellected(uint8_t option) {
    if(settings.tallyModeLED == option){
        return "selected";
    } else {
        return "";
    }
}

void blinkTallyNo(uint8_t tallyNo) {
    setLED(LED_OFF);
    if (tallyNo > 10) {
        return;
    }
    for (int i = 0; i < tallyNo + 1; i++) {
        delay(250);
        setLED(LED_BLUE);
        delay(250);
        setLED(LED_OFF);
    }
    delay(500);
}

//Perform initial setup on power on
void setup() {
    //Init pins for LED
    pinMode(PIN_RED, OUTPUT);
    pinMode(PIN_GREEN, OUTPUT);
    pinMode(PIN_BLUE, OUTPUT);

    //Setup current-measuring pin - Commented out for users without batteries
    // pinMode(A0, INPUT);

    //Start Serial
    Serial.begin(115200);
    Serial.println("########################");
    Serial.println("Serial started");

    //Read settings from EEPROM. WIFI settings are stored separately by the ESP
    EEPROM.begin(sizeof(settings)); //Needed on ESP8266 module, as EEPROM lib works a bit differently than on a regular Arduino
    EEPROM.get(0, settings);

    blinkTallyNo(settings.tallyNo);

    setLED(LED_YELLOW);


    if (settings.staticIP && settings.tallyIP != IPADDR_NONE) {
        WiFi.config(settings.tallyIP, settings.tallyGateway, settings.tallySubnetMask);
    } else {
        settings.staticIP = false;
    }

    //Put WiFi into station mode and make it connect to saved network
    WiFi.mode(WIFI_STA);
#if ESP32
    WiFi.setHostname(settings.tallyName);
#else
    WiFi.hostname(settings.tallyName);
#endif
    WiFi.setAutoReconnect(true);
    WiFi.begin();

#if ESP32
    if (!MDNS.begin(settings.tallyName)) {
        Serial.println("Error setting up mDNS responder!");
    } else {
       Serial.println("mDNS responder started");
    }
#endif

    Serial.println("------------------------");
    Serial.println("Connecting to WiFi...");
    Serial.println("Network name (SSID): " + getSSID());

    // Initialize and begin HTTP server for handeling the web interface
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.on("/upload", HTTP_POST, handleFirmwareUpdate, handleFirmwareUpload);
    server.on("/restart", handleRestart);
    server.on("/networks", handleNetworks);
    server.on("/save_network", HTTP_POST, [](){
        if (server.hasArg("ssid") && server.hasArg("password")) { // check if value parameter is present
            // Save new credentials
            String ssid = String(server.arg("ssid"));
            String pwd = String(server.arg("password"));

            server.send(200, "text/html", "<!DOCTYPE html><html>" + htmlHead(false) + 
            "<body><div class=\"container\">"
                "<h2>Network changed</h2>"
                "<hr>"
                "<p>Please manually go to new IP address, or try to go <a href=\"http://" + String(settings.tallyName) + ".local\">http://" + String(settings.tallyName) + ".local</a> after connect to new saved network</p>"
            "</div></body></html>");
            
            // Change into STA mode to disable softAP
            WiFi.mode(WIFI_STA);
            delay(100); // Give it time to switch over to STA mode (this is important on the ESP32 at least)

            if (ssid && pwd) {
                WiFi.persistent(true); // Needed by ESP8266
                // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
                // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
                WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
            }

            //Delay to apply settings before restart
            delay(100);
            ESP.restart();
        } else {
            server.send(200, "text/html", "Wrong parameters");
        }
    });
    server.onNotFound(handleNotFound);
    server.begin();

    tallyServer.begin();

    improv.setDeviceInfo(CHIP_FAMILY, DISPLAY_NAME, VERSION, "Tally Light", "");
    improv.onImprovError(onImprovWiFiErrorCb);
    improv.onImprovConnected(onImprovWiFiConnectedCb);

    //Wait for result from first attempt to connect - This makes sure it only activates the softAP if it was unable to connect,
    //and not just because it hasn't had the time to do so yet. It's blocking, so don't use it inside loop()
    unsigned long start = millis();
    while((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && (millis() - start) < 10000LU) {
        bytesAvailable = Serial.available();
            if(bytesAvailable > 0) {
                readByte = Serial.read();
                improv.handleByte(readByte);
            }
    }

    //Set state to connecting before entering loop
    changeState(STATE_CONNECTING_TO_WIFI);

#ifdef TALLY_TEST_SERVER
    tallyServer.setTallySources(40);
#endif
}

void loop() {
    bytesAvailable = Serial.available();
    if(bytesAvailable > 0) {
        readByte = Serial.read();
        improv.handleByte(readByte);
    }

    switch (state) {
        case STATE_CONNECTING_TO_WIFI:
            if (WiFi.status() == WL_CONNECTED) {
                WiFi.mode(WIFI_STA); // Disable softAP if connection is successful

#if ESP8266
                    if (!MDNS.begin(settings.tallyName)) {
                        Serial.println("Error setting up mDNS responder!");
                    } else {
                        Serial.println("mDNS responder started");
                    }
                    
                    MDNS.addService("http", "tcp", 80);
#endif

                Serial.println("------------------------");
                Serial.println("Connected to WiFi:   " + getSSID());
                Serial.println("IP:                  " + WiFi.localIP().toString());
                Serial.println("mDNS address:        http://" + String(settings.tallyName) + ".local");
                Serial.println("Subnet Mask:         " + WiFi.subnetMask().toString());
                Serial.println("Gateway IP:          " + WiFi.gatewayIP().toString());
#ifdef TALLY_TEST_SERVER
                Serial.println("Press enter (\\r) to loop through tally states.");
                changeState(STATE_RUNNING);
#else
                changeState(STATE_CONNECTING_TO_SWITCHER);
#endif
            } else if (firstRun) {
                firstRun = false;
                Serial.println("Unable to connect. Serving \"Tally Light setup\" WiFi for configuration, while still trying to connect...");
                Serial.println("IP for that device in \"Tally Light setup\" WiFi network is 192.168.4.1 or mDNS address http://" + String(settings.tallyName) + ".local");
                WiFi.softAP((String)DISPLAY_NAME + " setup");
                WiFi.mode(WIFI_AP_STA); // Enable softAP to access web interface in case of no WiFi
                setLED(LED_WHITE);
            }
            break;
#ifndef TALLY_TEST_SERVER
        case STATE_CONNECTING_TO_SWITCHER:
            // Initialize a connection to the switcher:
            if (firstRun) {
                atemSwitcher.begin(settings.switcherIP);
                //atemSwitcher.serialOutput(0xff); //Makes Atem library print debug info
                Serial.println("------------------------");
                Serial.println("Connecting to switcher...");
                Serial.println((String)"Switcher IP:         " + settings.switcherIP[0] + "." + settings.switcherIP[1] + "." + settings.switcherIP[2] + "." + settings.switcherIP[3]);
                firstRun = false;
            }
            atemSwitcher.runLoop();
            if (atemSwitcher.isConnected()) {
                changeState(STATE_RUNNING);
                Serial.println("Connected to switcher");
            }
            break;
#endif        

        case STATE_RUNNING:
#ifdef TALLY_TEST_SERVER
            if(bytesAvailable && readByte == '\r') {
                tallyFlag++;
                tallyFlag %= 4;
                
                switch (tallyFlag) {
                    case TALLY_FLAG_OFF:
                        Serial.println("Off");
                        break;
                    case TALLY_FLAG_PROGRAM:
                        Serial.println("Program");
                        break;
                    case TALLY_FLAG_PREVIEW:
                        Serial.println("Preview");
                        break;
                    case TALLY_FLAG_PROGRAM | TALLY_FLAG_PREVIEW:
                        Serial.println("Program and preview");
                        break;
                    default:
                        Serial.println("Invalid tally state...");
                        break;
                }

                for(int i = 0; i < 41; i++) {
                    tallyServer.setTallyFlag(i, tallyFlag);
                }
            }
#else
            //Handle data exchange and connection to swithcher
            atemSwitcher.runLoop();

            int tallySources = atemSwitcher.getTallyByIndexSources();
            tallyServer.setTallySources(tallySources);
            for (int i = 0; i < tallySources; i++) {
                tallyServer.setTallyFlag(i, atemSwitcher.getTallyByIndexTallyFlags(i));
            }
#endif

            //Handle Tally Server
            tallyServer.runLoop();

            //Set LED colors accordingly
            int color = getLedColor(settings.tallyModeLED, settings.tallyNo);
            setLED(color);

#ifndef TALLY_TEST_SERVER
            //Switch state if ATEM connection is lost...
            if (!atemSwitcher.isConnected()) { // will return false if the connection was lost
                Serial.println("------------------------");
                Serial.println("Connection to Switcher lost...");
                changeState(STATE_CONNECTING_TO_SWITCHER);

                //Reset tally server's tally flags, so clients turn off their lights.
                tallyServer.resetTallyFlags();
            }
#endif

            //Commented out for userst without batteries - Also timer is not done properly
            // batteryLoop();
            break;
    }

    //Switch state if WiFi connection is lost...
    if (WiFi.status() != WL_CONNECTED && state != STATE_CONNECTING_TO_WIFI) {
        Serial.println("------------------------");
        Serial.println("WiFi connection lost...");
        changeState(STATE_CONNECTING_TO_WIFI);

#ifndef TALLY_TEST_SERVER
        //Force atem library to reset connection, in order for status to read correctly on website.
        atemSwitcher.begin(settings.switcherIP);
        atemSwitcher.connect();
#endif

        //Reset tally server's tally flags, They won't get the message, but it'll be reset for when the connectoin is back.
        tallyServer.resetTallyFlags();
    }

    //Handle web interface
    server.handleClient();
#if ESP8266
    MDNS.update();  // Keep the mDNS responder alive (optional for ESP8266, not required for ESP32)
#endif
}

//Handle the change of states in the program
void changeState(uint8_t stateToChangeTo) {
    firstRun = true;
    switch (stateToChangeTo) {
        case STATE_CONNECTING_TO_WIFI:
            state = STATE_CONNECTING_TO_WIFI;
            setLED(LED_BLUE);
            break;
        case STATE_CONNECTING_TO_SWITCHER:
            state = STATE_CONNECTING_TO_SWITCHER;
            setLED(LED_PINK);
            break;
        case STATE_RUNNING:
            state = STATE_RUNNING;
            setLED(LED_GREEN);
            break;
    }
}

//Set the color of a LED using the given pins
void setLED(uint8_t color) {
#if ESP32
    switch (color) {
        case LED_OFF:
            digitalWrite(PIN_RED, 0);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_RED:
            digitalWrite(PIN_RED, 1);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_GREEN:
            digitalWrite(PIN_RED, 0);
            digitalWrite(PIN_GREEN, 1);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_BLUE:
            digitalWrite(PIN_RED, 0);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 1);
            break;
        case LED_YELLOW:
            digitalWrite(PIN_RED, 1);
            digitalWrite(PIN_GREEN, 1);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_PINK:
            digitalWrite(PIN_RED, 1);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 1);
            break;
        case LED_WHITE:
            digitalWrite(PIN_RED, 1);
            digitalWrite(PIN_GREEN, 1);
            digitalWrite(PIN_BLUE, 1);
            break;
    }
#else
    uint8_t ledBrightness = settings.ledBrightness;
    void (*writeFunc)(uint8_t, uint8_t);
    if(ledBrightness >= 0xff) {
        writeFunc = &digitalWrite;
        ledBrightness = 1;
    } else {
        writeFunc = &analogWriteWrapper;
    }

    switch (color) {
        case LED_OFF:
            digitalWrite(PIN_RED, 0);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_RED:
            writeFunc(PIN_RED, ledBrightness);
            digitalWrite(PIN_GREEN, 0);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_GREEN:
            digitalWrite(PIN_RED, 0);
            writeFunc(PIN_GREEN, ledBrightness);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_BLUE:
            digitalWrite(PIN_RED, 0);
            digitalWrite(PIN_GREEN, 0);
            writeFunc(PIN_BLUE, ledBrightness);
            break;
        case LED_YELLOW:
            writeFunc(PIN_RED, ledBrightness);
            writeFunc(PIN_GREEN, ledBrightness);
            digitalWrite(PIN_BLUE, 0);
            break;
        case LED_PINK:
            writeFunc(PIN_RED, ledBrightness);
            digitalWrite(PIN_GREEN, 0);
            writeFunc(PIN_BLUE, ledBrightness);
            break;
        case LED_WHITE:
            writeFunc(PIN_RED, ledBrightness);
            writeFunc(PIN_GREEN, ledBrightness);
            writeFunc(PIN_BLUE, ledBrightness);
            break;
    }
#endif
}

void analogWriteWrapper(uint8_t pin, uint8_t value) {
    analogWrite(pin, value);
}

int getTallyState(uint16_t tallyNo) {
#ifndef TALLY_TEST_SERVER
    if(tallyNo >= atemSwitcher.getTallyByIndexSources()) { //out of range
        return TALLY_FLAG_OFF;
    }

    uint8_t tallyFlag = atemSwitcher.getTallyByIndexTallyFlags(tallyNo);
#endif
    if (tallyFlag & TALLY_FLAG_PROGRAM) {
        return TALLY_FLAG_PROGRAM;
    } else if (tallyFlag & TALLY_FLAG_PREVIEW) {
        return TALLY_FLAG_PREVIEW;
    } else {
        return TALLY_FLAG_OFF;
    }
}

int getLedColor(int tallyMode, int tallyNo) {
    if(tallyMode == MODE_ON_AIR) {
#ifndef TALLY_TEST_SERVER
        if(atemSwitcher.getStreamStreaming()) {
            return LED_RED;
        }
#endif
        return LED_OFF;
    }

    int tallyState = getTallyState(tallyNo);

    if (tallyState == TALLY_FLAG_PROGRAM) {             //if tally live
        return LED_RED;
    } else if ((tallyState == TALLY_FLAG_PREVIEW        //if tally preview
                || tallyMode == MODE_PREVIEW_STAY_ON)   //or preview stay on
               && tallyMode != MODE_PROGRAM_ONLY) {     //and not program only
        return LED_GREEN;
    } else {                                            //if tally is neither
        return LED_OFF;
    }
}

//Serve setup web page to client, by sending HTML with the correct variables
void handleRoot() {
    server.send(200, "text/html", "<!DOCTYPE html><html>"
        + htmlHead(false) +
        "<script>"
            "function switchIpField(e){"
                "console.log(\"switch\");"
                "console.log(e);"
                "var target=e.srcElement||e.target;"
                "var maxLength=parseInt(target.attributes[\"maxlength\"].value,10);"
                "var myLength=target.value.length;"
                "if(myLength>=maxLength){"
                    "var next=target.nextElementSibling;"
                    "if(next!=null){"
                        "if(next.className.includes(\"IP\")){"
                            "next.focus();"
                        "}"
                    "}"
                "}else if(myLength==0){"
                    "var previous=target.previousElementSibling;"
                    "if(previous!=null){"
                        "if(previous.className.includes(\"IP\")){"
                            "previous.focus();"
                        "}"
                    "}"
                "}"
            "}"
            "function ipFieldFocus(e){"
                "console.log(\"focus\");"
                "console.log(e);"
                "var target=e.srcElement||e.target;"
                "target.select();"
            "}"
            "function load(){"
                "var containers=document.getElementsByClassName(\"IP\");"
                "for(var i=0;i<containers.length;i++){"
                    "var container=containers[i];"
                    "container.oninput=switchIpField;"
                    "container.onfocus=ipFieldFocus;"
                "}"
                "containers=document.getElementsByClassName(\"tIP\");"
                "for(var i=0;i<containers.length;i++){"
                    "var container=containers[i];"
                    "container.oninput=switchIpField;"
                    "container.onfocus=ipFieldFocus;"
                "}"
                "toggleStaticIPFields();"
            "}"
            "function toggleStaticIPFields(){"
                "var enabled=document.getElementById(\"staticIP\").checked;"
                "document.getElementById(\"staticIPHidden\").disabled=enabled;"
                "var staticIpFields=document.getElementsByClassName('tIP');"
                "for(var i=0;i<staticIpFields.length;i++){"
                    "staticIpFields[i].disabled=!enabled;"
                "}"
            "}"
            "function restart() {"
                "window.location.href = '/restart';"
            "}"
            "function networkClick() {"
                "window.location.href = '/networks';"
            "}"
        "</script>"
        "<body onload=\"load()\">"
            "<h1>" + (String)DISPLAY_NAME + " setup</h1>"
            "<div class=\"container\">"
                "<h2>Status</h2>"
                "<hr>"
                "<p><strong>Connection Status:</strong>" + getConnectionStatusString() + "</p>"
                "<p><strong>Network name (SSID):</strong>" + getSSID() + "</p>"
                "<p><strong>Signal strength:</strong>" + WiFi.RSSI() + " dBm</p>"
                "<p><strong>Static IP:</strong>" + (settings.staticIP == true ? "True" : "False") + "</p>"
                "<p><strong>This device IP:</strong><a href=\"http://" + WiFi.localIP().toString() + "\">" + WiFi.localIP().toString() + "</a></p>"
                "<p><strong>mDNS address:</strong> <a href=\"http://" + String(settings.tallyName) + ".local\">http://" + String(settings.tallyName) + ".local</a></p>"
                "<p><strong>Subnet mask:</strong>" + WiFi.subnetMask().toString() + "</p>"
                "<p><strong>Gateway:</strong>" + WiFi.gatewayIP().toString() + "</p>"
                "<br>"
                "<p><strong>ATEM switcher status:</strong>" + getAtemStatusString() + "</p>"
                "<p><strong>ATEM switcher IP:</strong>" + getAtemIpString() + "</p>"
                "<button type=\"button\" onclick=\"restart()\">Restart</button>"
            "</div>"
            "<div class=\"container\">"
                "<h2>Settings</h2>"
                "<hr>"
                "<form action=\"/save\"method=\"post\">"
                    "<label>Tally Light name:</label>"
                    "<input type=\"text\"maxlength=\"30\"name=\"tName\"value=\"" + getHostMdnsName() + "\"required/>"
                    "<label>Tally Light number:</label>"
                    "<input type=\"number\"min=\"1\"max=\"41\"name=\"tNo\"value=\"" + String(settings.tallyNo + 1) + "\"required/>"
                    "<label>Tally Light mode:</label>"
                    "<select name=\"tModeLED1\">"
                        "<option value=\"" + (String) MODE_NORMAL + "\"" + ifOptionSellected(MODE_NORMAL) + ">Normal</option>"
                        "<option value=\"" + (String) MODE_PREVIEW_STAY_ON + "\"" + ifOptionSellected(MODE_PREVIEW_STAY_ON) + ">Preview stay on</option>"
                        "<option value=\"" + (String) MODE_PROGRAM_ONLY + "\"" + ifOptionSellected(MODE_PROGRAM_ONLY) + ">Program only</option>"
                        "<option value=\"" + (String) MODE_ON_AIR + "\"" + ifOptionSellected(MODE_ON_AIR) + ">On Air</option>"
                    "</select>"
                    "<label>Led brightness:</label>"
                    "<input type=\"number\"min=\"0\"max=\"255\"name=\"ledBright\"value=\"" + settings.ledBrightness + "\"required/>"
                    "<hr>"
                    "<label>Network name(SSID):</label>"
                    "<input type =\"text\"maxlength=\"30\"name=\"ssid\"value=\"" +  getSSID() + "\"required/>"
                    "<label>Network password:</label>"
                    "<input type=\"password\"maxlength=\"30\"name=\"pwd\"pattern=\"^$|.{8,32}\"value=\"" +  WiFi.psk() + "\"/>"
                    "<br>"
                    "<button type=\"button\" onclick=\"networkClick()\">See available networks</button>"
                    "<hr>"
                    "<label>Use static IP:</label>"
                    "<input type=\"hidden\"id=\"staticIPHidden\"name=\"staticIP\"value=\"false\"/>"
                    "<input id=\"staticIP\"type=\"checkbox\"name=\"staticIP\"value=\"true\"onchange=\"toggleStaticIPFields()\"" + (settings.staticIP == true ? "checked" : "") + "/>"
                    "<label>This device IP:</label>"
                    "<div class=\"ip-fields\">"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"tIP1\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyIP[0] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"tIP2\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyIP[1] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"tIP3\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyIP[2] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"tIP4\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyIP[3] + "\"required/>"
                    "</div>"
                    "<label>Subnet mask:</label>"
                    "<div class=\"ip-fields\">"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"mask1\"pattern=\"\\d{0,3}\"value=\"" + settings.tallySubnetMask[0] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"mask2\"pattern=\"\\d{0,3}\"value=\"" + settings.tallySubnetMask[1] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"mask3\"pattern=\"\\d{0,3}\"value=\"" + settings.tallySubnetMask[2] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"mask4\"pattern=\"\\d{0,3}\"value=\"" + settings.tallySubnetMask[3] + "\"required/>"
                    "</div>"
                    "<label>Gateway:</label>"
                    "<div class=\"ip-fields\">"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"gate1\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyGateway[0] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"gate2\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyGateway[1] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"gate3\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyGateway[2] + "\"required/>"
                        "<input class=\"tIP\"type=\"text\"maxlength=\"3\"name=\"gate4\"pattern=\"\\d{0,3}\"value=\"" + settings.tallyGateway[3] + "\"required/>"
                    "</div>"
                    "<hr>"
                    "<label>ATEM switcher IP:</label>"
                    "<div class=\"ip-fields\">"
                        "<input class=\"IP\"type=\"text\"maxlength=\"3\"name=\"aIP1\"pattern=\"\\d{0,3}\"value=\"" + settings.switcherIP[0] + "\"required/>"
                        "<input class=\"IP\"type=\"text\"maxlength=\"3\"name=\"aIP2\"pattern=\"\\d{0,3}\"value=\"" + settings.switcherIP[1] + "\"required/>"
                        "<input class=\"IP\"type=\"text\"maxlength=\"3\"name=\"aIP3\"pattern=\"\\d{0,3}\"value=\"" + settings.switcherIP[2] + "\"required/>"
                        "<input class=\"IP\"type=\"text\"maxlength=\"3\"name=\"aIP4\"pattern=\"\\d{0,3}\"value=\"" + settings.switcherIP[3] + "\"required/>"
                    "</div>"
                    "<input type=\"submit\"value=\"Save Changes\"/>"
                "</form>"
            "</div>"
            "<div class=\"container\">"
                "<h2>Firmware update</h2>"
                "<hr>"
                "<form method='POST' action='/upload' enctype='multipart/form-data'>"
                    "<input type='file' name='firmware'>"
                    "<input type='submit' value='Update Firmware'>"
                "</form>"
            "</div>"
            + htmlFooter +
        "</body>"
    "</html>");
}

//Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/html", "<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>Error</h2>"
                "<hr>"
                "<p>Request without posting settings not allowed<br><br>Redirecting to main page...</p>"
            "</div></body></html>");
    } else {
        String ssid;
        String pwd;
        bool change = false;
        for (uint8_t i = 0; i < server.args(); i++) {
            change = true;
            String var = server.argName(i);
            String val = server.arg(i);

            if (var == "tName") {
                val.toCharArray(settings.tallyName, (uint8_t)32);
            } else if (var == "tModeLED1") {
                settings.tallyModeLED = val.toInt();
            } else if (var == "ledBright") {
                settings.ledBrightness = val.toInt();
            } else if (var == "tNo") {
                settings.tallyNo = val.toInt() - 1;
            } else if (var == "ssid") {
                ssid = String(val);
            } else if (var == "pwd") {
                pwd = String(val);
            } else if (var == "staticIP") {
                settings.staticIP = (val == "true");
            } else if (var == "tIP1") {
                settings.tallyIP[0] = val.toInt();
            } else if (var == "tIP2") {
                settings.tallyIP[1] = val.toInt();
            } else if (var == "tIP3") {
                settings.tallyIP[2] = val.toInt();
            } else if (var == "tIP4") {
                settings.tallyIP[3] = val.toInt();
            } else if (var == "mask1") {
                settings.tallySubnetMask[0] = val.toInt();
            } else if (var == "mask2") {
                settings.tallySubnetMask[1] = val.toInt();
            } else if (var == "mask3") {
                settings.tallySubnetMask[2] = val.toInt();
            } else if (var == "mask4") {
                settings.tallySubnetMask[3] = val.toInt();
            } else if (var == "gate1") {
                settings.tallyGateway[0] = val.toInt();
            } else if (var == "gate2") {
                settings.tallyGateway[1] = val.toInt();
            } else if (var == "gate3") {
                settings.tallyGateway[2] = val.toInt();
            } else if (var == "gate4") {
                settings.tallyGateway[3] = val.toInt();
            } else if (var == "aIP1") {
                settings.switcherIP[0] = val.toInt();
            } else if (var == "aIP2") {
                settings.switcherIP[1] = val.toInt();
            } else if (var == "aIP3") {
                settings.switcherIP[2] = val.toInt();
            } else if (var == "aIP4") {
                settings.switcherIP[3] = val.toInt();
            }
        }

        if (change) {
            EEPROM.put(0, settings);
            EEPROM.commit();

            server.send(200, "text/html", (String)"<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>Settings saved successfully</h2>"
                "<hr>"
                "<p>Redirecting to main page...</p>"
            "</div></body></html>");

            // Delay to let data be saved, and the response to be sent properly to the client
            server.close(); // Close server to flush and ensure the response gets to the client
            delay(100);

            // Change into STA mode to disable softAP
            WiFi.mode(WIFI_STA);
            delay(100); // Give it time to switch over to STA mode (this is important on the ESP32 at least)

            if (ssid && pwd) {
                WiFi.persistent(true); // Needed by ESP8266
                // Pass in 'false' as 5th (connect) argument so we don't waste time trying to connect, just save the new SSID/PSK
                // 3rd argument is channel - '0' is default. 4th argument is BSSID - 'NULL' is default.
                WiFi.begin(ssid.c_str(), pwd.c_str(), 0, NULL, false);
            }

            //Delay to apply settings before restart
            delay(100);
            ESP.restart();
        }
    }
}

void handleFirmwareUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Updating Firmware: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

void handleFirmwareUpdate() {
    if (!Update.hasError()) {
        Serial.println("Restarting...");
        server.send(200, "text/html", "<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>Successfully updated</h2>"
                "<hr>"
                "<p>Redirecting to main page...</p>"
            "</div></body></html>");
        delay(100);
        ESP.restart();
    } else {
        server.send(500, "text/html", "<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>Firmware Update Failed!</h2>"
                "<hr>"
                "<p>Redirecting to main page...</p>"
            "</div></body></html>");
    }
}

void handleRestart() {
    server.send(200, "text/html", "<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>Rebooting</h2>"
                "<hr>"
                "<p>Redirecting to main page...</p>"
            "</div></body></html>");
    Serial.println("Restarting from webpage...");
    delay(100);
    ESP.restart();
}

String networkChoiseSiteHead    = 
    "<!DOCTYPE html><html>" + htmlHead(false) + 
    "<script>"
        "function backClick() {"
                "window.location.href = '/';"
            "}"
        "</script>"
    "<body>"
        "<div class=\"container\">"
            "<h2>Set new network credentials</h2>"
            "<hr>"
            "<form method='post' action='/save_network'>"
                "<div>"
                "<label for='ssid'>SSID</label>"
                "<input type='text' id='ssid' name='ssid'>"
                "</div>"
                "<div>"
                "<label for='passVal'>Password</label>"
                "<input type='password' id='passVal' name='password'>"
                "</div>"
                "<button type='submit'>Connect</button>"
                "<button type=\"button\" onclick=\"backClick()\">Cancel and get back</button>"
            "</form>"
        "</div>";

String networkChoiseSiteFooter  = htmlFooter +
      "<script>"
        "function copyText(element) {"
          "var textToCopy = element.textContent || element.innerText;"
          "document.getElementById('ssid').value = textToCopy;"
        "}"
      "</script></body></html>";

String listVisibleNetworks() {
  String networks = "<div class=\"container\">"
            "<h2>Available Networks</h2>"
            "<hr>";
  int numNetworks = WiFi.scanNetworks();
  for (int i = 0; i < numNetworks; ++i) {
    networks += "<button type='button' onclick='copyText(this)'>" + WiFi.SSID(i) + "</button>";
  }
  networks += "</div>";
  return networks;
}

String getAvailableNetworksHtml() {
      return networkChoiseSiteHead + String(listVisibleNetworks()) + networkChoiseSiteFooter;
}

void handleNetworks() {
    server.send(200, "text/html", getAvailableNetworksHtml());
}

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound() {
    server.send(404, "text/html", "<!DOCTYPE html><html>" + htmlHead(true) + 
            "<body><div class=\"container\">"
                "<h2>404 - Page not found</h2>"
                "<hr>"
                "<p>Redirecting to main page...</p>"
            "</div></body></html>");
}

String getSSID() {
#if ESP32
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<const char *>(conf.sta.ssid));
#else
    return WiFi.SSID();
#endif
}

//Commented out for users without batteries - Also timer is not done properly
//Main loop for things that should work every second
// void batteryLoop() {
//     if (secLoop >= 400) {
//         //Get and calculate battery current
//         int raw = analogRead(A0);
//         uBatt = (double)raw / 1023 * 4.2;

//         //Set back status LED after one second to working LED_BLUE if it was changed by anything
//         if (lowLedOn) {
//             setStatusLED(LED_ORANGE);
//             lowLedOn = false;
//         }

//         //Blink every 5 seconds for one second if battery current is under 3.6V
//         if (lowLedCount >= 5 && uBatt <= 3.600) {
//             setStatusLED(LED_YELLOW);
//             lowLedOn = true;
//             lowLedCount = 0;
//         }
//         lowLedCount++;

//        //Turn stripes of and put ESP to deepsleep if battery is too low
//        if(uBatt <= 3.499) {
//            setSTRIP(LED_OFF);
//            setStatusLED(LED_OFF);
//            ESP.deepSleep(0, WAKE_NO_RFCAL);
//        }

//         secLoop = 0;
//     }
//     secLoop++;
// }
