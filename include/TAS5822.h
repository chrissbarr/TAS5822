/***************************************************
 This is a library for the TAS5822 I2S DAC/AMP.

 Written by Chris Barr, 2024.
 ****************************************************/

#ifndef _TAS5822_H_
#define _TAS5822_H_

#include <Arduino.h>
#include <stdint.h>

namespace TAS5822 {

/* Register names and offsets as per datasheet */
enum class Register : uint8_t {
    RESET_CTRL = 0x01,
    DEVICE_CTRL_1 = 0x02,
    DEVICE_CTRL_2 = 0x03,
    I2C_PAGE_AUTO_INC = 0x0F,
    SIG_CH_CTRL = 0x28,
    CLOCK_DET_CTRL = 0x29,
    SDOUT_SEL = 0x30,
    I2S_CTRL = 0x31,
    SAP_CTRL1 = 0x33,
    SAP_CTRL2 = 0x34,
    SAP_CTRL3 = 0x35,
    FS_MON = 0x37,
    BCK_MON = 0x38,
    CLKDET_STATUS = 0x39,
    DIG_VOL = 0x4C,
    DIG_VOL_CTRL1 = 0x4E,
    DIG_VOL_CTRL2 = 0x4F,
    AUTO_MUTE_CTRL = 0x50,
    AUTO_MUTE_TIME = 0x51,
    AMUTE_DELAY = 0x52,
    ANA_CTRL = 0x53,
    AGAIN = 0x54,
    BQ_WR_CTRL1 = 0x5C,
    DAC_CTRL = 0x5D,
    ADR_PIN_CTRL = 0x60,
    ADR_PIN_CONFIG = 0x61,
    DSP_MISC = 0x66,
    DIE_ID = 0x67,
    POWER_STATE = 0x68,
    AUTOMUTE_STATE = 0x69,
    PHASE_CTRL = 0x6A,
    SS_CTRL0 = 0x6B,
    SS_CTRL1 = 0x6C,
    SS_CTRL2 = 0x6D,
    SS_CTRL3 = 0x6E,
    SS_CTRL4 = 0x6F,
    CHAN_FAULT = 0x70,
    GLOBAL_FAULT1 = 0x71,
    GLOBAL_FAULT2 = 0x72,
    OT_WARNING = 0x73,
    PIN_CONTROL1 = 0x74,
    PIN_CONTROL2 = 0x75,
    FAULT_CLEAR = 0x78,
};

enum class CTRL_STATE : uint8_t { DEEP_SLEEP = 0x00, SLEEP = 0x01, HIGH_Z = 0x02, PLAY = 0x03 };

template <typename WIRE> class TAS5822 {
public:
    explicit TAS5822(WIRE& wire, uint8_t address, int16_t pdnPin = -1) : mWire(wire), pdnPin(pdnPin) {
        _i2caddr = address;
    }

    /**
     * Initialises the TAS5822 and leaves it in a playing state.
     * Note that output is MUTED by default. Call setMuted(false) to unmute.
     * \return True if initialisation completed successfully.
     */
    bool begin() {

        mWire.begin();

        if (pdnPin != -1) {
            pinMode(pdnPin, OUTPUT);
            digitalWrite(pdnPin, LOW);
            delay(10);
            digitalWrite(pdnPin, HIGH);
            delay(10);
        }

        // DSP Reset + HighZ + Mute
        if (!writeRegister(Register::DEVICE_CTRL_2, 0b00011010)) {
            logMessage("Failed to set: DSP Reset + HighZ + Mute");
            return false;
        }
        delay(5);

        // Reset Digital Core + Reset Registers
        if (!writeRegister(Register::RESET_CTRL, 0b00010001)) {
            logMessage("Failed to set: Reset Digital Core + Reset Registers");
            return false;
        }
        delay(5);

        // DSP Normal + HighZ + Mute
        if (!writeRegister(Register::DEVICE_CTRL_2, 0b00001010)) {
            logMessage("Failed to set: DSP Normal + HighZ + Mute");
            return false;
        }

        // Set Audio format
        if (!writeRegister(Register::SAP_CTRL1, 0x00)) {
            logMessage("Failed to set: Audio Format");
            return false;
        }

        // Set Muted
        if (!setMuted(true)) {
            logMessage("Failed to set: Muted");
            return false;
        }

        // Set Playing
        if (!setControlState(CTRL_STATE::PLAY)) {
            logMessage("Failed to set: Playing");
            return false;
        }

        // Set Analog Gain to lowest level
        if (!setAnalogGain(-15.5f)) {
            logMessage("Failed to set: Analog Gain");
            return false;
        }

        return true;
    }

    /**
     * Writes 8-bit value to given register.
     * \param reg Register to write to
     * \param value Value to write
     * \return True if I2C transmission completed successfully.
     */
    bool writeRegister(Register reg, uint8_t value) {
        mWire.beginTransmission(_i2caddr);
        mWire.write(static_cast<uint8_t>(reg));
        mWire.write(value);
        uint8_t ret = mWire.endTransmission();
        return (ret == 0);
    }

    /**
     * Reads 8-bit value from a given register.
     * \param reg Register to read from
     * \return Value read from given register.
     */
    uint8_t readRegister(Register reg) {
        mWire.beginTransmission(_i2caddr);
        mWire.write(static_cast<uint8_t>(reg));
        mWire.endTransmission();
        mWire.requestFrom(_i2caddr, uint8_t(1));
        return mWire.read();
    }

    /**
     * Set Analog Gain
     * \param gain Gain to be applied (-15.5 to 0 dBFS)
     * \return True if I2C transmission completed successfully.
     */
    bool setAnalogGain(float gain) {
        if (gain < -15.5) { gain = -15.5; }
        if (gain > 0) { gain = 0; }
        gain = -2.0 * gain;
        uint8_t gain_int = static_cast<uint8_t>(round(gain));
        return writeRegister(Register::AGAIN, gain_int);
    }

    /**
     * Enable/disable soft-mute.
     * \param muted Muted if true, unmuted if false.
     * \return True if I2C transmission completed successfully.
     */
    bool setMuted(bool muted) {
        // get the current state so we don't overwrite other parameters
        uint8_t regVal = readRegister(Register::DEVICE_CTRL_2);

        // clear mute bit and set updated value
        const uint8_t muteBitPos = 3;
        regVal = (regVal & (~(1 << muteBitPos))) | (static_cast<uint8_t>(muted) << muteBitPos);

        // write new value
        return writeRegister(Register::DEVICE_CTRL_2, regVal);
    }

    /**
     * Set the current control state.
     * \param state State, such as CTRL_STATE::PLAY, CTRL_STATE::SLEEP, etc.
     * \return True if I2C transmission completed successfully.
     */
    bool setControlState(CTRL_STATE state) {
        // get the current state so we don't overwrite other parameters
        uint8_t regVal = readRegister(Register::DEVICE_CTRL_2);

        // clear control state and set updated value
        const uint8_t CTRL_STATE_MASK = 0x03;
        regVal = (regVal & ~CTRL_STATE_MASK) | (static_cast<uint8_t>(state));

        // write new value
        return writeRegister(Register::DEVICE_CTRL_2, regVal);
    }

    /**
     * Get the current control state.
     * \return Current control state, such as CTRL_STATE::PLAY, CTRL_STATE::SLEEP, etc.
     */
    CTRL_STATE getControlState() {
        uint8_t regVal = readRegister(Register::DEVICE_CTRL_2);
        const uint8_t CTRL_STATE_MASK = 0x03;
        return static_cast<CTRL_STATE>(regVal & CTRL_STATE_MASK);
    }

    /**
     * Set a target for debug log messages.
     * If not set, no log messages will be written.
     * Any class deriving from Arduino Print (such as Serial)
     * should be usable as a target.
     * e.g. setLoggingOutput(&Serial)
     * \param print Pointer to target Print output.
     */
    void setLoggingOutput(Print* print) { logPrint = print; }

private:
    uint8_t _i2caddr;
    WIRE& mWire;
    int16_t pdnPin;
    Print* logPrint = nullptr;

    void logMessage(const char* msg) {
        if (logPrint) {
            logPrint->print("TAS5822: ");
            logPrint->println(msg);
        }
    }
};

} // namespace TAS5822

#endif