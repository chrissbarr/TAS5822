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

class RegisterModel {
public:
    RegisterModel(uint8_t addr) : addr(addr) {}

    void beginTransmission(uint8_t val) {
        if (val == addr) { active = true; }
    }

    void endTransmission() { active = false; }

    bool write(uint8_t val) {
        if (!active) { return false; }

        switch (writeIndex) {
        case 0: {
            targetReg = val;
            writeIndex++;
            break;
        }
        case 1: {
            registers[targetReg] = val;
            writeIndex = 0;
            break;
        }
        }
        return true;
    }

    uint8_t read() {
        if (registers.count(writeIndex) != 0) {
            return registers[writeIndex];
        } else {
            return 0;
        }
    }

    void registerExistsAndHasValue(TAS5822::Register reg, uint8_t value, uint8_t bitmask = 0xFF) {
        uint8_t regInt = static_cast<uint8_t>(reg);
        bool hasRegister = registers.count(regInt);
        EXPECT_EQ(true, hasRegister) << "Register " << std::hex << int(regInt) << " was not written.";
        if (hasRegister) {
            uint8_t regValueMasked = (registers[regInt] & bitmask);
            EXPECT_EQ(value, regValueMasked) << "Register " << std::hex << int(regInt) << " "
                                                << std::bitset<8>(regValueMasked) << " != " << std::bitset<8>(value);
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
        When(OverloadedMethod(ArduinoFake(Wire), requestFrom, void(uint8_t, uint8_t))).AlwaysReturn();
        When(OverloadedMethod(ArduinoFake(Wire), beginTransmission, void(uint8_t))).AlwaysDo([&](uint8_t v) {
            regmodel.beginTransmission(v);
        });
        When(OverloadedMethod(ArduinoFake(Wire), write, size_t(uint8_t))).AlwaysDo([&](uint8_t v) {
            return regmodel.write(v);
        });
        When(OverloadedMethod(ArduinoFake(Wire), read, uint8_t(void))).AlwaysDo([&]() {
            return regmodel.read();
        });
        When(OverloadedMethod(ArduinoFake(Wire), endTransmission, uint8_t(void))).AlwaysDo([&]() {
            regmodel.endTransmission();
            return 0;
        });
    }

    RegisterModel regmodel;

};

TEST_F(RegisterModelTest, DefaultInitialisedState) {

    TAS5822::TAS5822<TwoWire> amp(Wire, defaultAddress, -1);

    EXPECT_EQ(0, regmodel.registers.size());

    amp.begin();

    regmodel.registerExistsAndHasValue(TAS5822::Register::DEVICE_CTRL_2, 0b00000011);
}

// TEST_F(RegisterModelTest, MuteCommandWorks) {

//     TAS5822::TAS5822<TwoWire> amp(Wire, defaultAddress, -1);
//     amp.begin();

//     amp.setMuted(true);
//     //regmodel.registerExistsAndHasValue(TAS5822::Register::DEVICE_CTRL_2, 0b00000011);
// }

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS())
        ;
    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}