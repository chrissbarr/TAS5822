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
    /* This will set initial configuration and leave the TAS5822 ready to play data received over I2S. */
    tas5822.begin();

    /* Set Audio Format */
    tas5822.writeRegister(TAS5822::Register::SAP_CTRL1, 0b00000000); /* I2S 16-bits */
    // tas5822.writeRegister(TAS5822::Register::SAP_CTRL1, 0b00000001);  /* I2S 20-bits */
    // tas5822.writeRegister(TAS5822::Register::SAP_CTRL1, 0b00000010);  /* I2S 24-bits */
    // tas5822.writeRegister(TAS5822::Register::SAP_CTRL1, 0b00000011);  /* I2S 32-bits */

    /* Set Analog Gain */
    // by writing register manually
    tas5822.writeRegister(TAS5822::Register::AGAIN, 0b00011111); // Minimum (-15.5 dBFS)
    // tas5822.writeRegister(TAS5822::Register::AGAIN, 0b00000000); // Maximum (0 dBFS)
    // or via helper function
    tas5822.setAnalogGain(-4.0 /* dBFS */);
}

void loop() {
    /* Nothing to do */
    delay(100);
}