/*
This is a minimum example demonstrating configuration of the TAS5822.

No audio playback is performed in this example.
*/

// Library Includes
#include "TAS5822.h"

// Arduino Core
#include <Arduino.h>
#include <Wire.h>

/* Pin connected to TAS5822 PDN pin */
constexpr int16_t pdnPin = 13;

/* I2C Address of TAS5822 */
constexpr uint8_t i2cAddr = 44;

TAS5822::TAS5822<TwoWire> tas5822(Wire, i2cAddr, pdnPin);

void setup() {
    Serial.begin(115200);
    Serial.println("Start setup...");

    /* Uncomment to enable debug logging to Serial */
    // tas5822.setLoggingOutput(&Serial);

    /* This will set initial configuration and leave the TAS5822 ready to play data received over I2S. */
    if (!tas5822.begin()) {
        Serial.println("Error! TAS5822 initialisation failed!");
        while (true) {}
    }

    /* Set Audio Format */
    tas5822.setAudioFormat(TAS5822::DATA_FORMAT::I2S);
    // tas5822.setAudioFormat(TAS5822::DATA_FORMAT::LTJ);
    // tas5822.setAudioFormat(TAS5822::DATA_FORMAT::RTJ);
    // tas5822.setAudioFormat(TAS5822::DATA_FORMAT::TDM);
    tas5822.setAudioWordLength(TAS5822::DATA_WORD_LENGTH::b16);
    // tas5822.setAudioWordLength(TAS5822::DATA_WORD_LENGTH::b20);
    // tas5822.setAudioWordLength(TAS5822::DATA_WORD_LENGTH::b24);
    // tas5822.setAudioWordLength(TAS5822::DATA_WORD_LENGTH::b32);

    /* Set Analog Gain */
    tas5822.setAnalogGain(-4.0 /* dBFS */);

    /* Un-Mute */
    tas5822.setMuted(false);

    Serial.println("Setup finished!");
}

void loop() {
    /* Nothing to do */
    delay(100);
}