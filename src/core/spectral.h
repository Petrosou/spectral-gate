#ifndef SPECTRAL_H
#define SPECTRAL_H

#include <cstdint>
#include <cstddef>
#include "hal/hal_interface.h"
#include "decision.h"

namespace spectral_gate {
namespace core {

/**
 * @brief Spectral analysis processor
 * 
 * Performs frequency-domain analysis on vibration sensor data.
 * Uses simplified peak-finding and spectral feature extraction
 * suitable for embedded deployment.
 */
class SpectralProcessor {
public:
    /**
     * @brief Initialize spectral processor
     * @param num_bins Number of frequency bins for analysis
     * @param sample_rate Sample rate in Hz
     */
    SpectralProcessor(size_t num_bins, uint32_t sample_rate);

    /**
     * @brief Process raw vibration data and extract spectral features
     * @param samples Raw ADC samples (int16)
     * @param num_samples Number of samples
     * @return Spectral analysis result
     */
    SpectralResult process(const int16_t* samples, size_t num_samples);

    /**
     * @brief Extract feature vector for inference
     * @param samples Raw ADC samples
     * @param num_samples Number of samples
     * @param features Output feature array (fixed-point)
     * @param max_features Maximum features to extract
     * @return Number of features extracted
     */
    size_t extract_features(
        const int16_t* samples,
        size_t num_samples,
        hal::fixed_t* features,
        size_t max_features
    );

    /**
     * @brief Get number of frequency bins
     */
    size_t get_num_bins() const { return num_bins_; }

private:
    size_t num_bins_;
    uint32_t sample_rate_;
    
    /**
     * @brief Compute magnitude spectrum using simplified DFT
     * For embedded use, only computes select frequency bins
     */
    void compute_magnitude_spectrum(
        const int16_t* samples,
        size_t num_samples,
        hal::fixed_t* magnitudes
    );

    /**
     * @brief Find peaks in magnitude spectrum
     */
    uint8_t find_peaks(
        const hal::fixed_t* magnitudes,
        size_t count,
        hal::fixed_t threshold
    );

    /**
     * @brief Compute spectral centroid
     */
    hal::fixed_t compute_centroid(
        const hal::fixed_t* magnitudes,
        size_t count
    );
};

} // namespace core
} // namespace spectral_gate

#endif // SPECTRAL_H
