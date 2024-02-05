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

    struct Register {
        uint8_t value{0};
        int writeCount{0};
        int readCount{0};
    };

    void reset() {
        registers.clear();
        registers.reserve(regMax);

        for (int i = 0; i < regMax; i++) { registers.push_back(Register()); }

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
            registers[targetReg].value = val;
            registers[targetReg].writeCount += 1;
            writeIndex = 0;
            break;
        }
        }
        return true;
    }

    uint8_t read() {
        if (!active) { return false; }
        // std::cout << "Reading Register: " << int(targetReg) << " = " << int(registers[targetReg]) << "\n";
        registers[targetReg].readCount += 1;
        return registers[targetReg].value;
    }

    // management methods
    void setRegister(TAS5822::Register reg, uint8_t value) { registers[static_cast<uint8_t>(reg)].value = value; }

    const Register& getRegister(TAS5822::Register reg) const { return registers[static_cast<uint8_t>(reg)]; }

    const int getTotalRegisterWriteCount() {
        int count = 0;
        for (const auto& reg : registers) { count += reg.writeCount; }
        return count;
    }

    const int getTotalRegisterReadCount() {
        int count = 0;
        for (const auto& reg : registers) { count += reg.writeCount; }
        return count;
    }

private:
    const uint8_t regMax = 255;
    std::vector<Register> registers;
    uint8_t addr;
    bool active = false;
    int writeIndex = 0;
    uint8_t targetReg = 0;
};

void testRegisterWasWritten(RegisterModel& m, TAS5822::Register reg) {
    int writeCount = m.getRegister(reg).writeCount;
    EXPECT_GT(writeCount, 0) << "Register " << std::hex << static_cast<int>(reg) << " write count " << writeCount
                             << " !> 0";
}

void testRegisterHasValue(RegisterModel& m, TAS5822::Register reg, uint8_t value, uint8_t bitmask = 0xFF) {
    uint8_t actualMasked = (m.getRegister(reg).value & bitmask);
    uint8_t expectedMasked = (value & bitmask);
    EXPECT_EQ(expectedMasked, actualMasked)
        << "Register " << std::hex << static_cast<int>(reg) << " Actual " << std::bitset<8>(actualMasked)
        << " != " << std::bitset<8>(expectedMasked) << " Expected";
}

class RegisterModelTest : public testing::Test {
protected:
    const uint8_t defaultAddress{0x44};

    RegisterModelTest() : regmodel(defaultAddress) {}
    void SetUp() override { When(Method(ArduinoFake(), delay)).AlwaysReturn(); }

    RegisterModel regmodel;
};

// Check that writeRegister sets value
TEST_F(RegisterModelTest, BasicRegisterWritePattern) {
    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    amp.writeRegister(TAS5822::Register::DEVICE_CTRL_2, 123);
    testRegisterWasWritten(regmodel, TAS5822::Register::DEVICE_CTRL_2);
    testRegisterHasValue(regmodel, TAS5822::Register::DEVICE_CTRL_2, 123);
}

// Check that readRegister gets value
TEST_F(RegisterModelTest, BasicRegisterReadPattern) {
    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    regmodel.setRegister(TAS5822::Register::DEVICE_CTRL_2, 121);
    EXPECT_EQ(static_cast<uint8_t>(121), amp.readRegister(TAS5822::Register::DEVICE_CTRL_2));
}

// Check state after initialisation is as expected
TEST_F(RegisterModelTest, DefaultInitialisedState) {

    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);

    // check that constructor writes to no registers
    EXPECT_EQ(0, regmodel.getTotalRegisterWriteCount());
    EXPECT_EQ(0, regmodel.getTotalRegisterReadCount());

    amp.begin();

    // check that begin writes to some registers
    EXPECT_GT(regmodel.getTotalRegisterWriteCount(), 0);

    // check registers have expected values
    testRegisterHasValue(regmodel, TAS5822::Register::DEVICE_CTRL_2, 0b00000011);
}

TEST_F(RegisterModelTest, MuteCommandWorks) {

    TAS5822::TAS5822<RegisterModel> amp(regmodel, defaultAddress, -1);
    amp.begin();

    // Set register to known value
    amp.writeRegister(TAS5822::Register::DEVICE_CTRL_2, 0xFF);

    // verify setMuted(true) sets correct bit to correct value
    amp.setMuted(true);
    testRegisterHasValue(
        regmodel,
        TAS5822::Register::DEVICE_CTRL_2,
        0b00001000, /* Expect Mute bit is HIGH */
        0b00001000  /* Mute bit bit-mask */
    );

    // verify setMuted(false) sets correct bit to correct value
    amp.setMuted(false);
    testRegisterHasValue(
        regmodel,
        TAS5822::Register::DEVICE_CTRL_2,
        0b00000000, /* Expect Mute bit is LOW */
        0b00001000  /* Mute bit bit-mask */
    );

    // verify no other bits were changed by previous operations
    testRegisterHasValue(
        regmodel,
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
        testRegisterHasValue(regmodel, TAS5822::Register::AGAIN, static_cast<uint8_t>(result.second));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS())
        ;
    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}