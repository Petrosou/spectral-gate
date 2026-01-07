#include <iostream>
#include <iomanip>
#include <memory>
#include <string>

#include "hal/hal_interface.h"
#include "hal/hal_mock.h"
#include "core/decision.h"
#include "core/inference.h"
#include "core/spectral.h"

using namespace spectral_gate;

/**
 * @brief Spectral-Gate Energy-Adaptive Demo
 * 
 * This program demonstrates the "Energy-Adaptive" capabilities of the firmware.
 * It simulates a full day of operation showing how decision thresholds adapt
 * based on battery level:
 *   - Phase 1 (Morning): High battery, low confidence → TX_UNCERTAIN (Active Learning)
 *   - Phase 2 (Evening): Low battery, low confidence → SLEEP (Energy Conservation)
 *   - Phase 3 (Damage):  Low battery, high confidence → TX_ALERT (Safety Critical)
 */

//=============================================================================
// CSV Logging Helpers
//=============================================================================

void print_csv_header() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              SPECTRAL-GATE Energy-Adaptive Demo (STM32U5)                    ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Demonstrating battery-aware dynamic thresholding for IoT anomaly detection  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Base Threshold: 65% | Low Battery Multiplier: 1.2x | Critical Multiplier: 1.5x\n";
    std::cout << "Battery Levels: CRITICAL < 3000mV | LOW < 3300mV | NOMINAL >= 3700mV\n";
    std::cout << "\n";
}

void print_csv_table_header() {
    std::cout << "┌──────────┬────────────┬─────────────┬───────────┬─────────────┬───────────────┐\n";
    std::cout << "│   Time   │ V_bat (mV) │ Probability │ Threshold │   Decision  │    Reason     │\n";
    std::cout << "├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤\n";
}

void print_csv_row(const char* time, uint16_t battery_mv, float probability, 
                   float threshold, const char* decision, const char* reason) {
    std::cout << "│ " << std::setw(8) << std::left << time 
              << " │ " << std::setw(10) << std::right << battery_mv
              << " │ " << std::setw(10) << std::fixed << std::setprecision(1) << (probability * 100.0f) << "%"
              << " │ " << std::setw(8) << std::fixed << std::setprecision(1) << (threshold * 100.0f) << "%"
              << " │ " << std::setw(11) << std::left << decision
              << " │ " << std::setw(13) << reason
              << " │\n";
}

void print_csv_separator(const char* phase_name) {
    std::cout << "├──────────┴────────────┴─────────────┴───────────┴─────────────┴───────────────┤\n";
    std::cout << "│ " << std::setw(76) << std::left << phase_name << " │\n";
    std::cout << "├──────────┬────────────┬─────────────┬───────────┬─────────────┬───────────────┤\n";
}

void print_csv_footer() {
    std::cout << "└──────────┴────────────┴─────────────┴───────────┴─────────────┴───────────────┘\n";
}

//=============================================================================
// Demo Scenario Helpers
//=============================================================================

struct DemoScenario {
    const char* time;
    const char* phase;
    uint16_t battery_mv;
    float confidence;           // 0.0 to 1.0
    uint8_t predicted_class;    // 0=normal, 1=anomaly, 2=uncertain
    uint8_t num_peaks;
    float peak_magnitude;
};

float calculate_effective_threshold(uint16_t battery_mv, const core::ThresholdConfig& config) {
    float base = hal::fixed_to_float(config.base_confidence_threshold);
    
    if (battery_mv < hal::BATTERY_CRITICAL_MV) {
        return base * hal::fixed_to_float(config.critical_battery_multiplier);
    } else if (battery_mv < hal::BATTERY_LOW_MV) {
        return base * hal::fixed_to_float(config.low_battery_multiplier);
    }
    return base;
}

const char* get_decision_reason(core::Decision decision, uint16_t battery_mv, 
                                 uint8_t predicted_class, float confidence, float threshold) {
    switch (decision) {
        case core::Decision::TX_UNCERTAIN:
            return "Active Learn";
        case core::Decision::TX_ALERT:
            return "Safety Crit";
        case core::Decision::SLEEP:
            if (predicted_class == 2 && battery_mv < hal::BATTERY_LOW_MV) {
                return "Energy Veto";
            } else if (predicted_class == 1 && confidence < threshold) {
                return "Low Conf";
            } else if (predicted_class == 0) {
                return "Normal Op";
            }
            return "Conserve";
        default:
            return "Unknown";
    }
}

//=============================================================================
// Main Demo Function
//=============================================================================

void run_energy_adaptive_demo(hal::MockHAL& mock_hal) {
    core::ThresholdConfig config = core::get_default_config();
    
    // Define demo scenarios that demonstrate energy-adaptive behavior
    // Key insight: Same "low confidence" data produces different decisions based on battery
    DemoScenario scenarios[] = {
        // Phase 1: Morning - High Battery (4100mV)
        // Low confidence uncertain data → TX_UNCERTAIN (can afford active learning)
        {"06:00", "PHASE 1: MORNING - High Energy, Abundant Resources", 4100, 0.55f, 2, 3, 0.5f},
        {"07:00", nullptr, 4100, 0.58f, 2, 3, 0.5f},
        {"08:00", nullptr, 4050, 0.52f, 2, 3, 0.5f},
        {"09:00", nullptr, 4000, 0.60f, 2, 4, 0.5f},
        
        // Phase 2: Evening - Low Battery (2900mV < CRITICAL)
        // Same low confidence data → SLEEP (must conserve energy)
        {"17:00", "PHASE 2: EVENING - Low Energy, Conservation Mode", 2900, 0.55f, 2, 3, 0.5f},
        {"18:00", nullptr, 2850, 0.58f, 2, 3, 0.5f},
        {"19:00", nullptr, 2800, 0.52f, 2, 3, 0.5f},
        {"20:00", nullptr, 2750, 0.60f, 2, 4, 0.5f},
        
        // Phase 3: Damage Detected - Still Low Battery
        // High confidence anomaly → TX_ALERT (safety critical overrides energy conservation)
        // Note: Critical threshold = 65% * 1.5 = 97.5%, so confidence must exceed this
        {"21:00", "PHASE 3: DAMAGE DETECTED - Safety Critical Override", 2700, 0.98f, 1, 5, 0.9f},
        {"21:30", nullptr, 2650, 0.99f, 1, 6, 0.95f},
        {"22:00", nullptr, 2600, 0.985f, 1, 5, 0.85f},
        {"22:30", nullptr, 2550, 0.995f, 1, 7, 0.98f},
    };
    
    print_csv_header();
    print_csv_table_header();
    
    const char* current_phase = nullptr;
    uint32_t tx_uncertain_count = 0;
    uint32_t tx_alert_count = 0;
    uint32_t sleep_count = 0;
    
    for (const auto& scenario : scenarios) {
        // Print phase separator if entering new phase
        if (scenario.phase != nullptr && scenario.phase != current_phase) {
            if (current_phase != nullptr) {
                // Add visual separator between phases
            }
            print_csv_separator(scenario.phase);
            current_phase = scenario.phase;
        }
        
        // Configure mock HAL with scenario parameters
        mock_hal.set_battery_voltage(scenario.battery_mv);
        
        // Create spectral result
        core::SpectralResult spectral_result;
        spectral_result.dominant_frequency = hal::float_to_fixed(150.0f);
        spectral_result.peak_magnitude = hal::float_to_fixed(scenario.peak_magnitude);
        spectral_result.spectral_centroid = hal::float_to_fixed(200.0f);
        spectral_result.num_peaks = scenario.num_peaks;
        
        // Create inference result with controlled values
        core::InferenceResult inference_result;
        inference_result.confidence = hal::float_to_fixed(scenario.confidence);
        inference_result.predicted_class = scenario.predicted_class;
        
        // Calculate effective threshold for display
        float effective_threshold = calculate_effective_threshold(scenario.battery_mv, config);
        
        // Run decision logic
        core::Decision decision = core::evaluate_structure(
            spectral_result,
            inference_result,
            scenario.battery_mv,
            config
        );
        
        // Get decision reason
        const char* reason = get_decision_reason(
            decision, scenario.battery_mv, scenario.predicted_class,
            scenario.confidence, effective_threshold
        );
        
        // Print CSV row
        print_csv_row(
            scenario.time,
            scenario.battery_mv,
            scenario.confidence,
            effective_threshold,
            core::decision_to_string(decision),
            reason
        );
        
        // Track statistics
        switch (decision) {
            case core::Decision::TX_UNCERTAIN:
                ++tx_uncertain_count;
                mock_hal.transmit_alert(0, static_cast<uint8_t>(scenario.confidence * 100));
                break;
            case core::Decision::TX_ALERT:
                ++tx_alert_count;
                mock_hal.transmit_alert(1, static_cast<uint8_t>(scenario.confidence * 100));
                break;
            case core::Decision::SLEEP:
                ++sleep_count;
                mock_hal.enter_sleep(1000);
                break;
        }
    }
    
    print_csv_footer();
    
    // Print summary
    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "                           DEMO SUMMARY                                         \n";
    std::cout << "════════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "\n";
    std::cout << "  Energy-Adaptive Behavior Demonstrated:\n";
    std::cout << "  ──────────────────────────────────────\n";
    std::cout << "  • Phase 1 (High Battery): TX_UNCERTAIN decisions = " << tx_uncertain_count << "\n";
    std::cout << "    → System invests energy in active learning when resources abundant\n";
    std::cout << "\n";
    std::cout << "  • Phase 2 (Low Battery):  SLEEP decisions = " << sleep_count << "\n";
    std::cout << "    → Same uncertain data VETOED to conserve energy\n";
    std::cout << "\n";
    std::cout << "  • Phase 3 (Critical):     TX_ALERT decisions = " << tx_alert_count << "\n";
    std::cout << "    → Safety-critical alerts ALWAYS transmitted regardless of battery\n";
    std::cout << "\n";
    std::cout << "  HAL Statistics:\n";
    std::cout << "  ───────────────\n";
    std::cout << "  • Total Transmissions: " << mock_hal.get_transmit_count() << "\n";
    std::cout << "  • Total Sleep Time:    " << mock_hal.get_total_sleep_ms() << " ms\n";
    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════════════════════════════════════\n";
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // Create Mock HAL (Dependency Injection)
    hal::MockHAL mock_hal(hal::BATTERY_NOMINAL_MV);
    
    // Run the energy-adaptive demo scenario
    run_energy_adaptive_demo(mock_hal);
    
    return 0;
}
