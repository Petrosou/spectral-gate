#include "decision.h"

namespace spectral_gate {
namespace core {

using namespace hal;

ThresholdConfig get_default_config() {
    ThresholdConfig config;
    config.base_confidence_threshold = float_to_fixed(0.65f);       // 65% base threshold
    config.low_battery_multiplier = float_to_fixed(1.2f);           // 20% higher when low
    config.critical_battery_multiplier = float_to_fixed(1.5f);      // 50% higher when critical
    config.min_peaks_for_detection = 2;
    return config;
}

Decision evaluate_structure(
    const SpectralResult& spectral,
    const InferenceResult& inference,
    uint16_t battery_mv,
    const ThresholdConfig& config
) {
    // Step 1: Determine effective threshold based on battery level
    fixed_t effective_threshold = config.base_confidence_threshold;
    
    if (battery_mv < BATTERY_CRITICAL_MV) {
        // Critical battery: significantly raise threshold
        effective_threshold = fixed_mul(config.base_confidence_threshold, 
                                        config.critical_battery_multiplier);
    } else if (battery_mv < BATTERY_LOW_MV) {
        // Low battery: moderately raise threshold
        effective_threshold = fixed_mul(config.base_confidence_threshold,
                                        config.low_battery_multiplier);
    }
    
    // Step 2: Check if spectral analysis shows sufficient activity
    bool sufficient_spectral_activity = 
        (spectral.num_peaks >= config.min_peaks_for_detection) &&
        (spectral.peak_magnitude > float_to_fixed(0.1f));
    
    // Step 3: Early exit if no significant spectral activity
    if (!sufficient_spectral_activity) {
        return Decision::SLEEP;
    }
    
    // Step 4: Evaluate based on inference confidence and class
    fixed_t confidence = inference.confidence;
    uint8_t predicted_class = inference.predicted_class;
    
    // Class 0 = normal operation
    if (predicted_class == 0) {
        return Decision::SLEEP;
    }
    
    // Class 1 = anomaly detected
    if (predicted_class == 1) {
        if (confidence >= effective_threshold) {
            return Decision::TX_ALERT;
        } else if (confidence >= fixed_mul(effective_threshold, float_to_fixed(0.7f))) {
            // Confidence between 70% and 100% of threshold -> uncertain
            return Decision::TX_UNCERTAIN;
        }
        return Decision::SLEEP;
    }
    
    // Class 2 = model uncertain
    if (predicted_class == 2) {
        // Only transmit uncertain if we have high spectral activity
        // and battery is not critical
        if (battery_mv >= BATTERY_LOW_MV && 
            spectral.num_peaks >= (config.min_peaks_for_detection + 1)) {
            return Decision::TX_UNCERTAIN;
        }
        return Decision::SLEEP;
    }
    
    // Default fallback
    return Decision::SLEEP;
}

const char* decision_to_string(Decision d) {
    switch (d) {
        case Decision::SLEEP:        return "SLEEP";
        case Decision::TX_ALERT:     return "TX_ALERT";
        case Decision::TX_UNCERTAIN: return "TX_UNCERTAIN";
        default:                     return "UNKNOWN";
    }
}

} // namespace core
} // namespace spectral_gate
