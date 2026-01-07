#include "hal_mock.h"
#include <cmath>
#include <iostream>
#include <thread>

namespace spectral_gate {
namespace hal {

MockHAL::MockHAL()
    : battery_voltage_mv_(BATTERY_NOMINAL_MV),
      vibration_pattern_(1),  // Default: sinusoidal
      signal_frequency_hz_(100),
      signal_amplitude_(8000),
      noise_level_(500),
      wake_event_pending_(false),
      transmit_count_(0),
      total_sleep_ms_(0),
      sample_phase_(0),
      rng_(std::random_device{}()),
      start_time_(std::chrono::steady_clock::now())
{
}

MockHAL::MockHAL(uint16_t initial_battery_mv)
    : battery_voltage_mv_(initial_battery_mv),
      vibration_pattern_(1),
      signal_frequency_hz_(100),
      signal_amplitude_(8000),
      noise_level_(500),
      wake_event_pending_(false),
      transmit_count_(0),
      total_sleep_ms_(0),
      sample_phase_(0),
      rng_(std::random_device{}()),
      start_time_(std::chrono::steady_clock::now())
{
}

int16_t MockHAL::generate_noise() {
    std::uniform_int_distribution<int16_t> dist(-noise_level_, noise_level_);
    return dist(rng_);
}

int16_t MockHAL::generate_sinusoid() {
    // Generate clean sinusoidal signal at configured frequency
    constexpr uint32_t SAMPLE_RATE = 1000;  // 1kHz sample rate
    
    double phase = 2.0 * 3.14159265358979 * signal_frequency_hz_ * sample_phase_ / SAMPLE_RATE;
    int16_t signal = static_cast<int16_t>(signal_amplitude_ * std::sin(phase));
    
    // Add noise
    signal += generate_noise();
    
    ++sample_phase_;
    return signal;
}

int16_t MockHAL::generate_anomaly() {
    // Generate multi-frequency pattern simulating mechanical anomaly
    constexpr uint32_t SAMPLE_RATE = 1000;
    
    double phase1 = 2.0 * 3.14159265358979 * 50 * sample_phase_ / SAMPLE_RATE;   // 50 Hz base
    double phase2 = 2.0 * 3.14159265358979 * 150 * sample_phase_ / SAMPLE_RATE;  // 150 Hz harmonic
    double phase3 = 2.0 * 3.14159265358979 * 237 * sample_phase_ / SAMPLE_RATE;  // 237 Hz anomaly
    
    int16_t signal = static_cast<int16_t>(
        signal_amplitude_ * 0.5 * std::sin(phase1) +
        signal_amplitude_ * 0.3 * std::sin(phase2) +
        signal_amplitude_ * 0.4 * std::sin(phase3)
    );
    
    // Add burst noise occasionally (simulating impact events)
    std::uniform_int_distribution<int> burst_dist(0, 100);
    if (burst_dist(rng_) < 5) {
        signal += generate_noise() * 3;
    } else {
        signal += generate_noise();
    }
    
    ++sample_phase_;
    return signal;
}

size_t MockHAL::read_vibration_data(int16_t* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size == 0) {
        return 0;
    }
    
    for (size_t i = 0; i < buffer_size; ++i) {
        switch (vibration_pattern_) {
            case 0:  // Pure noise
                buffer[i] = generate_noise();
                break;
            case 1:  // Sinusoidal
                buffer[i] = generate_sinusoid();
                break;
            case 2:  // Anomaly pattern
                buffer[i] = generate_anomaly();
                break;
            default:
                buffer[i] = generate_noise();
                break;
        }
    }
    
    return buffer_size;
}

uint16_t MockHAL::get_battery_voltage_mv() {
    return battery_voltage_mv_;
}

uint32_t MockHAL::get_tick_ms() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    return static_cast<uint32_t>(duration.count());
}

void MockHAL::enter_sleep(uint32_t duration_ms) {
    total_sleep_ms_ += duration_ms;
    
    // Simulate actual sleep (scaled down for simulation speed)
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms / 100));
    
    // Simulate battery drain during sleep (very slow)
    if (battery_voltage_mv_ > 2800) {
        battery_voltage_mv_ -= 1;  // 1mV per sleep cycle
    }
}

bool MockHAL::transmit_alert(uint8_t alert_type, uint8_t confidence) {
    ++transmit_count_;
    
    std::cout << "[TX] Alert Type: " << (alert_type == 1 ? "CONFIRMED" : "UNCERTAIN")
              << ", Confidence: " << static_cast<int>(confidence) << "%"
              << ", Battery: " << battery_voltage_mv_ << "mV"
              << std::endl;
    
    // Simulate transmission power consumption
    if (battery_voltage_mv_ > 2900) {
        battery_voltage_mv_ -= 10;  // TX consumes ~10mV equivalent
    }
    
    return true;
}

bool MockHAL::is_wake_event_pending() {
    return wake_event_pending_;
}

void MockHAL::clear_wake_event() {
    wake_event_pending_ = false;
}

void MockHAL::set_battery_voltage(uint16_t voltage_mv) {
    battery_voltage_mv_ = voltage_mv;
}

void MockHAL::set_vibration_pattern(uint8_t type) {
    vibration_pattern_ = type;
    sample_phase_ = 0;  // Reset phase on pattern change
}

void MockHAL::set_signal_frequency(uint32_t freq_hz) {
    signal_frequency_hz_ = freq_hz;
}

void MockHAL::set_signal_amplitude(int16_t amplitude) {
    signal_amplitude_ = amplitude;
}

void MockHAL::set_noise_level(int16_t level) {
    noise_level_ = level;
}

void MockHAL::trigger_wake_event() {
    wake_event_pending_ = true;
}

} // namespace hal
} // namespace spectral_gate
