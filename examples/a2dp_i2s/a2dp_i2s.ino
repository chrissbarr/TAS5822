/*
This example is intended for the ESP32.

This is an example of a basic Bluetooth A2DP audio receiver, streaming audio to the TAS5822 I2S amplifier.

The ESP32-A2DP library (https://github.com/pschatzmann/ESP32-A2DP) is used to configure audio streaming.
*/

// Library Includes
#include "TAS5822.h"

// ESP32-A2DP Library
#include "BluetoothA2DPSink.h"

// Arduino Core
#include <Arduino.h>
#include <Wire.h>

/* Pin connected to TAS5822 PDN pin */
constexpr int16_t pdnPin = 13;

/* I2C Address of TAS5822 */
constexpr uint8_t i2cAddr = 44;

TAS5822::TAS5822<TwoWire> tas5822(Wire, i2cAddr, pdnPin);

/* Set ESP32 I2S Pinout */
i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = 27,
    .ws_io_num = 33,
    .data_out_num = 32,
    .data_in_num = I2S_PIN_NO_CHANGE};

/* Construct BT A2DP Sink */
BluetoothA2DPSink a2dp_sink;

void setup() {
    Serial.begin(115200);
    Serial.println("Start setup...");

    /* Create Audio Sink first, so that I2S interface is active. */
    a2dp_sink.set_pin_config(pin_config);
    a2dp_sink.start("Example-Music");

    /* TAS5822 should be started AFTER I2S interface (above). */
    if (!tas5822.begin()) {
        Serial.println("Error! TAS5822 initialisation failed!");
        while (true) {}
    }
    /* Set Volume */
    tas5822.setAnalogGain(-16);
    /* Un-Mute */
    tas5822.setMuted(false);

    Serial.println("Setup finished!");
}

void loop() {
    /* Nothing to do */
    delay(100);
}