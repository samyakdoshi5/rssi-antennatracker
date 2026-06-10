/*
 * SPI driver based on fs_skyrf_58g-main.c Written by Simon Chambers
 * TVOUT by Myles Metzel
 * Scanner by Johan Hermen
 * Inital 2 Button version by Peter (pete1990)
 * Refactored and GUI reworked by Marko Hoepken
 * Universal version my Marko Hoepken
 * Diversity Receiver Mode and GUI improvements by Shea Ivey
 * OLED Version by Shea Ivey
 * Seperating display concerns by Shea Ivey

The MIT License (MIT)

Copyright (c) 2015 Marko Hoepken

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "settings.h"
#include "settings_internal.h"
#include "settings_eeprom.h"

#include "channels.h"
#include "receiver.h"
#include "receiver_spi.h"
#include "buttons.h"
#include "state.h"

#include "ui.h"


static void globalMenuButtonHandler(
    Button button,
    Buttons::PressType pressType
);

#ifdef RSSI_SERIAL_DIAGNOSTIC
#define RSSI_SCAN_START_CHANNEL 32
#define RSSI_SCAN_CHANNEL_COUNT 8

static uint8_t scanChannel = RSSI_SCAN_START_CHANNEL;
static uint32_t scanTuneTime = 0;
static bool scanReadingPending = false;
static uint8_t scanRssi[RSSI_SCAN_CHANNEL_COUNT] = { 0 };
static uint16_t scanRawA[RSSI_SCAN_CHANNEL_COUNT] = { 0 };
static uint16_t scanRawB[RSSI_SCAN_CHANNEL_COUNT] = { 0 };

static void tuneScanChannel() {
    Receiver::setChannel(scanChannel);
    scanTuneTime = millis();
    scanReadingPending = true;
}

static void printScanHeader() {
    Serial.println(F("band\tch\tfreq\trssi\trawA\trawB"));
}

static void drawOledScanSummary(uint8_t maxIndex) {
    uint8_t maxChannel = RSSI_SCAN_START_CHANNEL + maxIndex;
    const char *maxName = Channels::getName(maxChannel);

    Ui::clear();
    Ui::display.setTextColor(WHITE);
    Ui::display.setTextSize(1);
    Ui::display.setCursor(0, 0);
    Ui::display.print(F("RX5808 R scan"));

    Ui::display.setCursor(0, 12);
    Ui::display.print(F("Best "));
    Ui::display.print(maxName);
    Ui::display.print(F(" "));
    Ui::display.print(Channels::getFrequency(maxChannel));

    Ui::display.setCursor(0, 24);
    Ui::display.print(F("RSSI "));
    Ui::display.print(scanRssi[maxIndex]);
    Ui::display.print(F(" Raw "));
    Ui::display.print(scanRawA[maxIndex]);

    Ui::display.setCursor(0, 36);
    Ui::display.print(F("A "));
    Ui::display.print(scanRawA[maxIndex]);
    Ui::display.print(F(" B "));
    Ui::display.print(scanRawB[maxIndex]);

    Ui::display.setCursor(0, 52);
    for (uint8_t i = 0; i < RSSI_SCAN_CHANNEL_COUNT; i++) {
        uint8_t barHeight = map(scanRssi[i], 0, 100, 0, 10);
        uint8_t x = i * 15;
        Ui::display.drawRect(x, 53, 10, 10, WHITE);
        Ui::display.fillRect(x, 63 - barHeight, 10, barHeight, WHITE);
    }

    Ui::needDisplay();
}

static void printScanRow(uint8_t channel, bool isMaxRssi) {
    const char *name = Channels::getName(channel);

    Serial.print(name[0]);
    Serial.print(F("\t"));
    Serial.print(name[1]);
    Serial.print(F("\t"));
    Serial.print(Channels::getFrequency(channel));
    Serial.print(F("\t"));
    Serial.print(Receiver::rssiA);
    Serial.print(F("\t"));
    Serial.print(Receiver::rssiARaw);
    Serial.print(F("\t"));
    Serial.print(Receiver::rssiBRaw);
    if (isMaxRssi) {
        Serial.print(F("\t***"));
    } else if (Receiver::rssiA > 60) {
        Serial.print(F("\t*"));
    }
    Serial.println();
}

static void printCompletedScan() {
    uint8_t maxRssi = 0;
    uint8_t maxIndex = 0;

    for (uint8_t i = 0; i < RSSI_SCAN_CHANNEL_COUNT; i++) {
        if (scanRssi[i] > maxRssi) {
            maxRssi = scanRssi[i];
            maxIndex = i;
        }
    }

    drawOledScanSummary(maxIndex);

    printScanHeader();
    for (uint8_t i = 0; i < RSSI_SCAN_CHANNEL_COUNT; i++) {
        Receiver::rssiA = scanRssi[i];
        Receiver::rssiARaw = scanRawA[i];
        Receiver::rssiBRaw = scanRawB[i];
        printScanRow(RSSI_SCAN_START_CHANNEL + i, scanRssi[i] == maxRssi);
    }
}
#endif


void setup()
{
    setupPins();

    // Enable buzzer and LED for duration of setup process.
    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_BUZZER, LOW);

    setupSettings();

#ifdef RSSI_SERIAL_DIAGNOSTIC
    Serial.begin(250000);
    while (!Serial) {}

    Receiver::setup();
    Receiver::setActiveReceiver(Receiver::ReceiverId::A);
    Ui::setup();

    Serial.println(F("RX5808 RSSI scan"));
    tuneScanChannel();

    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, HIGH);
    return;
#else
    StateMachine::setup();
    Receiver::setup();
    Ui::setup();

    Receiver::setActiveReceiver(Receiver::ReceiverId::A);

    #ifdef USE_IR_EMITTER
        Serial.begin(9600);
    #endif
    #ifdef USE_SERIAL_OUT
        Serial.begin(250000);
    #endif

    // Setup complete.
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, HIGH);

    Buttons::registerChangeFunc(globalMenuButtonHandler);

    // Switch to initial state.
    StateMachine::switchState(StateMachine::State::SEARCH);
#endif
}

void setupPins() {
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
    pinMode(PIN_BUTTON_MODE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
    pinMode(PIN_BUTTON_SAVE, INPUT_PULLUP);

    pinMode(PIN_LED_A,OUTPUT);
    #ifdef USE_DIVERSITY
        pinMode(PIN_LED_B,OUTPUT);
    #endif

    pinMode(PIN_RSSI_A, INPUT);
    pinMode(PIN_RSSI_B, INPUT);
    #ifdef USE_DIVERSITY
        pinMode(PIN_RSSI_B, INPUT_PULLUP);
    #endif

    pinMode(PIN_SPI_SLAVE_SELECT, OUTPUT);
    pinMode(PIN_SPI_DATA, OUTPUT);
	pinMode(PIN_SPI_CLOCK, OUTPUT);

    digitalWrite(PIN_SPI_SLAVE_SELECT, HIGH);
    digitalWrite(PIN_SPI_CLOCK, LOW);
    digitalWrite(PIN_SPI_DATA, LOW);
}

void setupSettings() {
    EepromSettings.load();
    Receiver::setChannel(EepromSettings.startChannel);
}


void loop() {
#ifdef RSSI_SERIAL_DIAGNOSTIC
    Ui::update();

    if (
        scanReadingPending
        && millis() - scanTuneTime >= RSSI_SCAN_SETTLE_MS
    ) {
        Receiver::updateRssi();
        uint8_t scanIndex = scanChannel - RSSI_SCAN_START_CHANNEL;
        scanRssi[scanIndex] = Receiver::rssiA;
        scanRawA[scanIndex] = Receiver::rssiARaw;
        scanRawB[scanIndex] = Receiver::rssiBRaw;

        scanChannel++;
        if (scanChannel >= RSSI_SCAN_START_CHANNEL + RSSI_SCAN_CHANNEL_COUNT) {
            scanChannel = RSSI_SCAN_START_CHANNEL;
            Serial.println();
            printCompletedScan();
        }

        scanTuneTime = millis();
        scanReadingPending = false;
    } else if (
        !scanReadingPending
        && millis() - scanTuneTime >= RSSI_SCAN_BETWEEN_MS
    ) {
        tuneScanChannel();
    }

    return;
#else
    Receiver::update();
    Buttons::update();
    StateMachine::update();
    Ui::update();
    EepromSettings.update();

    if (
        StateMachine::currentState != StateMachine::State::SCREENSAVER
        && StateMachine::currentState != StateMachine::State::BANDSCAN
        && (millis() - Buttons::lastChangeTime) >
            (SCREENSAVER_TIMEOUT * 1000)
    ) {
        StateMachine::switchState(StateMachine::State::SCREENSAVER);
    }
#endif
}


static void globalMenuButtonHandler(
    Button button,
    Buttons::PressType pressType
) {
    if (
        StateMachine::currentState != StateMachine::State::MENU &&
        button == Button::MODE &&
        pressType == Buttons::PressType::HOLDING
    ) {
        StateMachine::switchState(StateMachine::State::MENU);
    }
}
