#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <cstdint>
#include <cstddef>

namespace spectral_gate {
namespace hal {

// Fixed-point representation: Q15.16 format (16 bits integer, 16 bits fractional)
using fixed_t = int32_t;
constexpr int FIXED_SHIFT = 16;
constexpr fixed_t FIXED_ONE = 1 << FIXED_SHIFT;

// Convert float to fixed-point (for simulation/testing only)
inline fixed_t float_to_fixed(float val) {
    return static_cast<fixed_t>(val * FIXED_ONE);
}

// Convert fixed-point to float (for display/debugging only)
inline float fixed_to_float(fixed_t val) {
    return static_cast<float>(val) / FIXED_ONE;
}

// Fixed-point multiplication
inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return static_cast<fixed_t>((static_cast<int64_t>(a) * b) >> FIXED_SHIFT);
}

// Sensor data buffer configuration
constexpr size_t VIBRATION_BUFFER_SIZE = 256;
constexpr size_t NUM_SPECTRAL_BINS = 64;

// Battery voltage thresholds (in millivolts)
constexpr uint16_t BATTERY_CRITICAL_MV = 3000;
constexpr uint16_t BATTERY_LOW_MV = 3300;
constexpr uint16_t BATTERY_NOMINAL_MV = 3700;

/**
 * @brief Hardware Abstraction Layer Interface
 * 
 * Pure virtual class defining the contract between core logic and hardware.
 * Implements Dependency Inversion Principle - core logic depends on this
 * abstraction, not concrete hardware implementations.
 */
class IHardwareAbstraction {
public:
    virtual ~IHardwareAbstraction() = default;

    /**
     * @brief Read vibration sensor data into buffer
     * @param buffer Pointer to buffer for storing samples
     * @param buffer_size Size of buffer in samples
     * @return Number of samples actually read
     */
    virtual size_t read_vibration_data(int16_t* buffer, size_t buffer_size) = 0;

    /**
     * @brief Get current battery voltage
     * @return Battery voltage in millivolts
     */
    virtual uint16_t get_battery_voltage_mv() = 0;

    /**
     * @brief Get system tick count (milliseconds)
     * @return Current tick count
     */
    virtual uint32_t get_tick_ms() = 0;

    /**
     * @brief Enter low-power sleep mode
     * @param duration_ms Sleep duration in milliseconds
     */
    virtual void enter_sleep(uint32_t duration_ms) = 0;

    /**
     * @brief Transmit alert via radio (LoRa/BLE simulation)
     * @param alert_type Type of alert (0=uncertain, 1=confirmed)
     * @param confidence Confidence level (0-100)
     * @return true if transmission successful
     */
    virtual bool transmit_alert(uint8_t alert_type, uint8_t confidence) = 0;

    /**
     * @brief Check if external interrupt (wake) occurred
     * @return true if wake event pending
     */
    virtual bool is_wake_event_pending() = 0;

    /**
     * @brief Clear wake event flag
     */
    virtual void clear_wake_event() = 0;
};

} // namespace hal
} // namespace spectral_gate

#endif // HAL_INTERFACE_H
