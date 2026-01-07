#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include "hal_interface.h"
#include <random>
#include <chrono>

namespace spectral_gate {
namespace hal {

/**
 * @brief Mock HAL implementation for PC-based simulation
 * 
 * Simulates STM32U5 hardware behavior for testing and development.
 * Generates synthetic vibration data and provides configurable
 * battery voltage for testing battery-aware thresholding.
 */
class MockHAL : public IHardwareAbstraction {
public:
    /**
     * @brief Construct mock HAL with default settings
     */
    MockHAL();

    /**
     * @brief Construct mock HAL with specific battery voltage
     * @param initial_battery_mv Initial battery voltage in millivolts
     */
    explicit MockHAL(uint16_t initial_battery_mv);

    // IHardwareAbstraction interface implementation
    size_t read_vibration_data(int16_t* buffer, size_t buffer_size) override;
    uint16_t get_battery_voltage_mv() override;
    uint32_t get_tick_ms() override;
    void enter_sleep(uint32_t duration_ms) override;
    bool transmit_alert(uint8_t alert_type, uint8_t confidence) override;
    bool is_wake_event_pending() override;
    void clear_wake_event() override;

    // Test configuration methods
    
    /**
     * @brief Set battery voltage for testing
     * @param voltage_mv Voltage in millivolts
     */
    void set_battery_voltage(uint16_t voltage_mv);

    /**
     * @brief Set vibration signal type for data generation
     * @param type 0=noise, 1=sinusoidal, 2=anomaly pattern
     */
    void set_vibration_pattern(uint8_t type);

    /**
     * @brief Set primary frequency for sinusoidal pattern
     * @param freq_hz Frequency in Hz
     */
    void set_signal_frequency(uint32_t freq_hz);

    /**
     * @brief Set signal amplitude (0-32767)
     * @param amplitude Signal amplitude
     */
    void set_signal_amplitude(int16_t amplitude);

    /**
     * @brief Set noise level (0-32767)
     * @param level Noise level
     */
    void set_noise_level(int16_t level);

    /**
     * @brief Trigger a wake event
     */
    void trigger_wake_event();

    /**
     * @brief Get count of transmitted alerts
     */
    uint32_t get_transmit_count() const { return transmit_count_; }

    /**
     * @brief Get total sleep time accumulated
     */
    uint32_t get_total_sleep_ms() const { return total_sleep_ms_; }

private:
    uint16_t battery_voltage_mv_;
    uint8_t vibration_pattern_;
    uint32_t signal_frequency_hz_;
    int16_t signal_amplitude_;
    int16_t noise_level_;
    bool wake_event_pending_;
    uint32_t transmit_count_;
    uint32_t total_sleep_ms_;
    uint32_t sample_phase_;
    
    std::mt19937 rng_;
    std::chrono::steady_clock::time_point start_time_;

    /**
     * @brief Generate noise sample
     */
    int16_t generate_noise();

    /**
     * @brief Generate sinusoidal sample
     */
    int16_t generate_sinusoid();

    /**
     * @brief Generate anomaly pattern sample
     */
    int16_t generate_anomaly();
};

} // namespace hal
} // namespace spectral_gate

#endif // HAL_MOCK_H
