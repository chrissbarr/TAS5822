/* Project Scope */
#include "TAS5822.h"

/* Libraries */
#include <Arduino.h>
#include <gtest/gtest.h>

/* C++ Standard Library */
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS())
        ;
    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}