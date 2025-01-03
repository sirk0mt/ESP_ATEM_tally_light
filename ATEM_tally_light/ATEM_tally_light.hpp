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

#include "Arduino.h"
#include "ImprovWiFiLibrary.h"

/* v0.0.0 - v(OR).(dev test ver).(dev ver) */
#define VER "v0.0.2"

//Blink LED to show saved tally number
void blinkTallyNo(uint8_t tallyNo);

//Perform initial setup on power on
//Handle the change of states in the program
void changeState(uint8_t stateToChangeTo);

//Set the color of both LEDs
void setBothLEDs(uint8_t color);

//Set the color of LED
void setLED(uint8_t color);

void analogWriteWrapper(uint8_t pin, uint8_t value);

int getTallyState(uint16_t tallyNo);

int getLedColor(int tallyMode, int tallyNo);

//Serve setup web page to client, by sending HTML with the correct variables
void handleRoot();

//Save new settings from client in EEPROM and restart the ESP8266 module
void handleSave();

void handleFirmwareUpload();

void handleFirmwareUpdate();

void handleRestart();

void handleNetworks();

String getAvailableNetworksHtml();

//Send 404 to client in case of invalid webpage being requested.
void handleNotFound();

String getSSID();

void setWiFi(String ssid, String pwd);

// void improvCallback(improv::ImprovCommand d);
//Commented out for users without batteries - Also timer is not done properly
//Main loop for things that should work every second
// void batteryLoop();
