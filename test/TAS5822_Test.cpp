/* Project Scope */
#include "TAS5822.h"

/* Libraries */
#include <Arduino.h>
#include <gtest/gtest.h>

/* C++ Standard Library */
#include <bitset>
#include <map>
#include <utility>
#include <vector>

using namespace fakeit;

/* Register model is a simplistic model of the I2C register read/write behaviour of the TAS5822. */
class RegisterModel {
public:
    RegisterModel(uint8_t addr) : addr(addr) { reset(); }

    void reset() {
        registers.clear();
        active = false;
        writeIndex = 0;
        targetReg = 0;
    }

    // Arduino Wire-compatible signature

    void begin() {}

    void beginTransmission(uint8_t val) {
        if (val == addr) {
            active = true;
            writeIndex = 0;
        }
    }

    uint8_t endTransmission() {
        active = false;
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t qty) {
        if (address == addr) {
            active = true;
            return qty;
        }
        return 0;
    }

    bool write(uint8_t val) {
        if (!active) { return false; }

        switch (writeIndex) {
        case 0: {
            targetReg = val;
            // std::cout << "Selecting Register: " << int(targetReg) << "\n";
            writeIndex++;
            break;
        }
        case 1: {
            // std::cout << "Writing Register: " << int(targetReg) << " = " << int(val) << "\n";
            registers[targetReg] = val;
            writeIndex = 0;
            break;
        }
        }
        return true;
    }

    uint8_t read() {
        if (!active) { return false; }

        if (registers.count(targetReg) != 0) {
            // std::cout << "Reading Register: " << int(targetReg) << " = " << int(registers[targetReg]) << "\n";
            return registers[targetReg];
        } else {
            return 0;
        }
    }

    void registerExistsAndHasValue(TAS5822::Register reg, uint8_t value, uint8_t bitmask = 0xFF) {
        uint8_t regInt = static_cast<uint8_t>(reg);
        bool hasRegister = registers.count(regInt);
        EXPECT_EQ(true, hasRegister) << "Register " << std::hex << int(regInt) << " was not written.";
        if (hasRegister) {
            uint8_t actualMasked = (registers[regInt] & bitmask);
            uint8_t expectedMasked = (value & bitmask);
            EXPECT_EQ(expectedMasked, actualMasked)
                << "Register " << std::hex << int(regInt) << " Actual " << std::bitset<8>(actualMasked)
                << " != " << std::bitset<8>(expectedMasked) << " Expected";
        }
    }

    std::map<uint8_t, uint8_t> registers;

private:
    uint8_t addr;
    bool active = false;
    int writeIndex = 0;
    uint8_t targetReg = 0;
};

class RegisterModelTest : public testing::Test {
protected:
    const uint8_t defaultAddress{0x44};

    RegisterModelTest() : regmodel(defaultAddress) {}
    void SetUp() override {
        When(Method(ArduinoFake(), delay)).AlwaysReturn();
    }

    RegisterModel regmodel;
};

// Check that writeRegister sets value
TEST_F(RegisterModelTest, BasicRegisterWritePattern) {
    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    amp.writeRegister(TAS5822::Register::DEVICE_CTRL_2, 123);
    regmodel.registerExistsAndHasValue(TAS5822::Register::DEVICE_CTRL_2, 123);
}

// Check that readRegister gets value
TEST_F(RegisterModelTest, BasicRegisterReadPattern) {
    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    regmodel.registers[static_cast<uint8_t>(TAS5822::Register::DEVICE_CTRL_2)] = 121;
    EXPECT_EQ(static_cast<uint8_t>(121), amp.readRegister(TAS5822::Register::DEVICE_CTRL_2));
}

// Check state after initialisation is as expected
TEST_F(RegisterModelTest, DefaultInitialisedState) {

    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);

    // check that constructor writes to no registers
    EXPECT_EQ(static_cast<uint8_t>(0), regmodel.registers.size());

    amp.begin();

    regmodel.registerExistsAndHasValue(TAS5822::Register::DEVICE_CTRL_2, 0b00000011);
}

TEST_F(RegisterModelTest, MuteCommandWorks) {

    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    amp.begin();

    // Set register to known value
    amp.writeRegister(TAS5822::Register::DEVICE_CTRL_2, 0xFF);

    // verify setMuted(true) sets correct bit to correct value
    amp.setMuted(true);
    regmodel.registerExistsAndHasValue(
        TAS5822::Register::DEVICE_CTRL_2,
        0b00001000, /* Expect Mute bit is HIGH */
        0b00001000  /* Mute bit bit-mask */
    );

    // verify setMuted(false) sets correct bit to correct value
    amp.setMuted(false);
    regmodel.registerExistsAndHasValue(
        TAS5822::Register::DEVICE_CTRL_2,
        0b00000000, /* Expect Mute bit is LOW */
        0b00001000  /* Mute bit bit-mask */
    );

    // verify no other bits were changed by previous operations
    regmodel.registerExistsAndHasValue(
        TAS5822::Register::DEVICE_CTRL_2,
        0xFF,      /* Expect all other bits have original value */
        0b11110111 /* Inverted Mute bit bit-mask */
    );
}

TEST_F(RegisterModelTest, AnalogGainCalculation) {

    // Analog Gain (dBFS) to expected AGAIN register value
    std::vector<std::pair<float, uint8_t>> expectedResults = {
        {-16, 31},
        {-15.5, 31},
        {-15, 30},
        {-14.5, 29},
        {-0.5, 1},
        {-0.4, 1},
        {-0.1, 0},
        {0, 0},
        {1, 0},
        {100, 0},
    };

    for (const auto& result : expectedResults) {

        regmodel.reset();
        TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
        amp.begin();

        // Set Analog Gain from input float
        amp.setAnalogGain(result.first);

        // Check that AGAIN register matches expected value
        regmodel.registerExistsAndHasValue(TAS5822::Register::AGAIN, static_cast<uint8_t>(result.second));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS())
        ;
    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}