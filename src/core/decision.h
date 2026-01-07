#ifndef DECISION_H
#define DECISION_H

#include <cstdint>
#include "hal/hal_interface.h"

namespace spectral_gate {
namespace core {

/**
 * @brief Decision outcomes from the evaluation logic
 */
enum class Decision : uint8_t {
    SLEEP = 0,          // No significant activity, return to sleep
    TX_ALERT = 1,       // Confirmed anomaly, transmit alert
    TX_UNCERTAIN = 2    // Uncertain detection, transmit for cloud analysis
};

/**
 * @brief Spectral analysis result structure
 */
struct SpectralResult {
    hal::fixed_t dominant_frequency;    // Dominant frequency bin (fixed-point)
    hal::fixed_t peak_magnitude;        // Peak magnitude (fixed-point)
    hal::fixed_t spectral_centroid;     // Spectral centroid (fixed-point)
    uint8_t num_peaks;                  // Number of significant peaks detected
};

/**
 * @brief Inference result structure
 */
struct InferenceResult {
    hal::fixed_t confidence;            // Model confidence (0.0 to 1.0 in fixed-point)
    uint8_t predicted_class;            // 0=normal, 1=anomaly, 2=uncertain
};

/**
 * @brief Configuration for battery-aware dynamic thresholding
 */
struct ThresholdConfig {
    hal::fixed_t base_confidence_threshold;     // Base threshold for anomaly detection
    hal::fixed_t low_battery_multiplier;        // Multiplier when battery is low
    hal::fixed_t critical_battery_multiplier;   // Multiplier when battery is critical
    uint8_t min_peaks_for_detection;            // Minimum spectral peaks required
};

/**
 * @brief Evaluate structure and make decision based on spectral and inference results
 * 
 * Implements battery-aware dynamic thresholding:
 * - At nominal battery: uses base threshold
 * - At low battery: raises threshold to reduce TX frequency
 * - At critical battery: only TX for high-confidence anomalies
 * 
 * @param spectral Spectral analysis result
 * @param inference Model inference result
 * @param battery_mv Current battery voltage in millivolts
 * @param config Threshold configuration
 * @return Decision outcome
 */
Decision evaluate_structure(
    const SpectralResult& spectral,
    const InferenceResult& inference,
    uint16_t battery_mv,
    const ThresholdConfig& config
);

/**
 * @brief Get default threshold configuration
 * @return Default ThresholdConfig values
 */
ThresholdConfig get_default_config();

/**
 * @brief Convert Decision enum to string for display
 * @param d Decision value
 * @return C-string representation
 */
const char* decision_to_string(Decision d);

} // namespace core
} // namespace spectral_gate

#endif // DECISION_H
