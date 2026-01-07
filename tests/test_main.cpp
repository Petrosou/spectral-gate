#include <iostream>
#include <cassert>
#include <cmath>

#include "hal/hal_interface.h"
#include "hal/hal_mock.h"
#include "core/decision.h"
#include "core/inference.h"
#include "core/spectral.h"

using namespace spectral_gate;

// Simple test framework macros
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    test_##name(); \
    std::cout << "PASSED\n"; \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))

// Test fixed-point math
TEST(fixed_point_conversion) {
    hal::fixed_t one = hal::float_to_fixed(1.0f);
    ASSERT_EQ(one, hal::FIXED_ONE);
    
    float back = hal::fixed_to_float(one);
    ASSERT_TRUE(std::abs(back - 1.0f) < 0.0001f);
    
    hal::fixed_t half = hal::float_to_fixed(0.5f);
    ASSERT_EQ(half, hal::FIXED_ONE / 2);
}

TEST(fixed_point_multiply) {
    hal::fixed_t a = hal::float_to_fixed(2.0f);
    hal::fixed_t b = hal::float_to_fixed(3.0f);
    hal::fixed_t result = hal::fixed_mul(a, b);
    
    float expected = 6.0f;
    float actual = hal::fixed_to_float(result);
    ASSERT_TRUE(std::abs(actual - expected) < 0.01f);
}

// Test decision logic
TEST(decision_sleep_on_low_activity) {
    core::SpectralResult spectral{};
    spectral.num_peaks = 0;
    spectral.peak_magnitude = hal::float_to_fixed(0.01f);
    
    core::InferenceResult inference{};
    inference.confidence = hal::float_to_fixed(0.9f);
    inference.predicted_class = 1;
    
    core::ThresholdConfig config = core::get_default_config();
    
    core::Decision result = core::evaluate_structure(
        spectral, inference, hal::BATTERY_NOMINAL_MV, config
    );
    
    ASSERT_EQ(result, core::Decision::SLEEP);
}

TEST(decision_alert_on_high_confidence) {
    core::SpectralResult spectral{};
    spectral.num_peaks = 3;
    spectral.peak_magnitude = hal::float_to_fixed(0.5f);
    
    core::InferenceResult inference{};
    inference.confidence = hal::float_to_fixed(0.85f);
    inference.predicted_class = 1;
    
    core::ThresholdConfig config = core::get_default_config();
    
    core::Decision result = core::evaluate_structure(
        spectral, inference, hal::BATTERY_NOMINAL_MV, config
    );
    
    ASSERT_EQ(result, core::Decision::TX_ALERT);
}

TEST(decision_battery_threshold_scaling) {
    core::SpectralResult spectral{};
    spectral.num_peaks = 3;
    spectral.peak_magnitude = hal::float_to_fixed(0.5f);
    
    core::InferenceResult inference{};
    inference.confidence = hal::float_to_fixed(0.70f);
    inference.predicted_class = 1;
    
    core::ThresholdConfig config = core::get_default_config();
    
    // At nominal battery - should alert
    core::Decision result1 = core::evaluate_structure(
        spectral, inference, hal::BATTERY_NOMINAL_MV, config
    );
    
    // At critical battery - threshold raised, should not alert
    core::Decision result2 = core::evaluate_structure(
        spectral, inference, hal::BATTERY_CRITICAL_MV - 100, config
    );
    
    ASSERT_EQ(result1, core::Decision::TX_ALERT);
    ASSERT_TRUE(result2 != core::Decision::TX_ALERT);
}

// Test mock HAL
TEST(mock_hal_vibration_data) {
    hal::MockHAL mock;
    int16_t buffer[256];
    
    size_t read = mock.read_vibration_data(buffer, 256);
    ASSERT_EQ(read, 256u);
}

TEST(mock_hal_battery) {
    hal::MockHAL mock(3500);
    ASSERT_EQ(mock.get_battery_voltage_mv(), 3500);
    
    mock.set_battery_voltage(3200);
    ASSERT_EQ(mock.get_battery_voltage_mv(), 3200);
}

// Test spectral processor
TEST(spectral_processor_basic) {
    core::SpectralProcessor proc(64, 1000);
    
    int16_t samples[256];
    for (size_t i = 0; i < 256; ++i) {
        samples[i] = static_cast<int16_t>(1000 * std::sin(2.0 * 3.14159 * 50 * i / 1000));
    }
    
    core::SpectralResult result = proc.process(samples, 256);
    ASSERT_TRUE(result.peak_magnitude > 0);
}

int main() {
    std::cout << "\n=== Spectral-Gate Unit Tests ===\n\n";
    
    RUN_TEST(fixed_point_conversion);
    RUN_TEST(fixed_point_multiply);
    RUN_TEST(decision_sleep_on_low_activity);
    RUN_TEST(decision_alert_on_high_confidence);
    RUN_TEST(decision_battery_threshold_scaling);
    RUN_TEST(mock_hal_vibration_data);
    RUN_TEST(mock_hal_battery);
    RUN_TEST(spectral_processor_basic);
    
    std::cout << "\n=== All tests passed! ===\n\n";
    return 0;
}
