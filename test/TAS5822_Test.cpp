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

TEST(TAS5822Test, BasicRegisterWritePattern) {

    TAS5822::TAS5822<TwoWire> amp(Wire, 123, 0);

    When(OverloadedMethod(ArduinoFake(Wire), begin, void(void))).AlwaysReturn();
    When(OverloadedMethod(ArduinoFake(Wire), beginTransmission, void(uint8_t))).AlwaysReturn();
    When(OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))).AlwaysReturn(true);
    When(OverloadedMethod(ArduinoFake(Wire), endTransmission, uint8_t(void))).AlwaysReturn(0);

    amp.writeRegister(TAS5822::Register::DEVICE_CTRL_2, 124);

    Verify(
        OverloadedMethod(ArduinoFake(Wire), beginTransmission, void(uint8_t)).Using(123),
        OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))
            .Using(static_cast<uint8_t>(TAS5822::Register::DEVICE_CTRL_2)),
        OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t)).Using(124))
        .Exactly(1);
}

TEST(TAS5822Test, AnalogGainCalculation) {

    TAS5822::TAS5822<TwoWire> amp(Wire, 0, 0);

    // Analog Gain (dBFS) to expected byte value
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

        ArduinoFakeReset();

        When(OverloadedMethod(ArduinoFake(Wire), begin, void(void))).AlwaysReturn();
        When(OverloadedMethod(ArduinoFake(Wire), beginTransmission, void(uint8_t))).AlwaysReturn();
        When(OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))).AlwaysReturn(true);
        When(OverloadedMethod(ArduinoFake(Wire), endTransmission, uint8_t(void))).AlwaysReturn(0);

        amp.setAnalogGain(result.first);

        Verify(
            OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))
                .Using(static_cast<uint8_t>(TAS5822::Register::AGAIN)),
            OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t)).Using(static_cast<uint8_t>(result.second)))
            .Exactly(Once);
    }
}

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

    void beginTransmission(uint8_t val) {
        if (val == addr) {
            active = true;
            writeIndex = 0;
        }
    }

    void endTransmission() { active = false; }

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
        When(OverloadedMethod(ArduinoFake(Wire), begin, void(void))).AlwaysReturn();
        When(OverloadedMethod(ArduinoFake(Wire), requestFrom, uint8_t(uint8_t, uint8_t)))
            .AlwaysDo([&](uint8_t addr, uint8_t num) { return regmodel.requestFrom(addr, num); });
        When(OverloadedMethod(ArduinoFake(Wire), beginTransmission, void(uint8_t))).AlwaysDo([&](uint8_t v) {
            regmodel.beginTransmission(v);
        });
        When(OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))).AlwaysDo([&](uint8_t v) {
            return regmodel.write(v);
        });
        When(OverloadedMethod(ArduinoFake(Wire), read, int(void))).AlwaysDo([&]() { return regmodel.read(); });
        When(OverloadedMethod(ArduinoFake(Wire), endTransmission, uint8_t(void))).AlwaysDo([&]() {
            regmodel.endTransmission();
            return 0;
        });
    }

    RegisterModel regmodel;
};

TEST_F(RegisterModelTest, DefaultInitialisedState) {

    TAS5822::TAS5822<TwoWire> amp(Wire, defaultAddress, -1);

    // check that constructor writes to no registers
    EXPECT_EQ(static_cast<uint8_t>(0), regmodel.registers.size());

    amp.begin();

    regmodel.registerExistsAndHasValue(TAS5822::Register::DEVICE_CTRL_2, 0b00000011);
}

TEST_F(RegisterModelTest, MuteCommandWorks) {

    TAS5822::TAS5822<TwoWire> amp(Wire, defaultAddress, -1);
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS())
        ;
    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}