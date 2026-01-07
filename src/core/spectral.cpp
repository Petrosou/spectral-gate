#include "spectral.h"
#include <cmath>

namespace spectral_gate {
namespace core {

using namespace hal;

// Precomputed sine/cosine table for common frequencies (embedded optimization)
// In a real implementation, this would be generated at compile time
namespace {
    constexpr int32_t SIN_TABLE_SIZE = 64;
    
    // Approximate sine using integer math (Taylor series approximation)
    fixed_t fast_sin(uint32_t angle_256) {
        // angle_256 is angle * 256 / (2*PI), i.e., 256 = full circle
        angle_256 &= 255;
        
        // Convert to radians approximation
        // sin(x) ≈ x - x³/6 for small x (using fixed-point)
        int32_t x = angle_256;
        if (x > 128) x = 256 - x;  // Mirror for second half
        if (x > 64) x = 128 - x;   // Mirror for second quarter
        
        // Scale to fixed-point range
        fixed_t result = (x * FIXED_ONE) / 64;
        
        // Apply sign for quadrants
        if (angle_256 > 128) result = -result;
        
        return result;
    }
    
    fixed_t fast_cos(uint32_t angle_256) {
        return fast_sin(angle_256 + 64);  // cos(x) = sin(x + π/2)
    }
}

SpectralProcessor::SpectralProcessor(size_t num_bins, uint32_t sample_rate)
    : num_bins_(num_bins), sample_rate_(sample_rate)
{
}

void SpectralProcessor::compute_magnitude_spectrum(
    const int16_t* samples,
    size_t num_samples,
    fixed_t* magnitudes
) {
    // Simplified DFT for select frequency bins
    // In production, would use optimized FFT or Goertzel algorithm
    
    for (size_t k = 0; k < num_bins_; ++k) {
        int64_t real_sum = 0;
        int64_t imag_sum = 0;
        
        // Frequency for this bin
        uint32_t freq_mult = (k * 256) / num_bins_;
        
        for (size_t n = 0; n < num_samples; ++n) {
            uint32_t angle = (freq_mult * n) % 256;
            
            fixed_t cos_val = fast_cos(angle);
            fixed_t sin_val = fast_sin(angle);
            
            // Accumulate (sample is int16, result scaled to fixed-point)
            real_sum += static_cast<int64_t>(samples[n]) * cos_val;
            imag_sum += static_cast<int64_t>(samples[n]) * sin_val;
        }
        
        // Normalize by sample count
        real_sum /= static_cast<int64_t>(num_samples);
        imag_sum /= static_cast<int64_t>(num_samples);
        
        // Compute magnitude: sqrt(real² + imag²)
        // Using approximation: |z| ≈ max(|Re|,|Im|) + 0.4*min(|Re|,|Im|)
        int64_t abs_real = (real_sum >= 0) ? real_sum : -real_sum;
        int64_t abs_imag = (imag_sum >= 0) ? imag_sum : -imag_sum;
        
        int64_t max_val = (abs_real > abs_imag) ? abs_real : abs_imag;
        int64_t min_val = (abs_real < abs_imag) ? abs_real : abs_imag;
        
        // Magnitude approximation
        magnitudes[k] = static_cast<fixed_t>((max_val + (min_val * 4) / 10) >> FIXED_SHIFT);
    }
}

uint8_t SpectralProcessor::find_peaks(
    const fixed_t* magnitudes,
    size_t count,
    fixed_t threshold
) {
    uint8_t peak_count = 0;
    
    for (size_t i = 1; i < count - 1; ++i) {
        // Check if this bin is a local maximum above threshold
        if (magnitudes[i] > threshold &&
            magnitudes[i] > magnitudes[i - 1] &&
            magnitudes[i] > magnitudes[i + 1]) {
            ++peak_count;
        }
    }
    
    return peak_count;
}

fixed_t SpectralProcessor::compute_centroid(
    const fixed_t* magnitudes,
    size_t count
) {
    int64_t weighted_sum = 0;
    int64_t magnitude_sum = 0;
    
    for (size_t i = 0; i < count; ++i) {
        weighted_sum += static_cast<int64_t>(magnitudes[i]) * static_cast<int64_t>(i);
        magnitude_sum += magnitudes[i];
    }
    
    if (magnitude_sum == 0) {
        return 0;
    }
    
    // Return centroid as bin index in fixed-point
    return static_cast<fixed_t>((weighted_sum * FIXED_ONE) / magnitude_sum);
}

SpectralResult SpectralProcessor::process(const int16_t* samples, size_t num_samples) {
    SpectralResult result;
    result.dominant_frequency = 0;
    result.peak_magnitude = 0;
    result.spectral_centroid = 0;
    result.num_peaks = 0;
    
    if (num_samples == 0 || samples == nullptr) {
        return result;
    }
    
    // Allocate magnitude buffer (stack allocation for embedded)
    constexpr size_t MAX_BINS = 128;
    fixed_t magnitudes[MAX_BINS];
    size_t actual_bins = (num_bins_ < MAX_BINS) ? num_bins_ : MAX_BINS;
    
    // Compute spectrum
    compute_magnitude_spectrum(samples, num_samples, magnitudes);
    
    // Find peak magnitude and dominant frequency bin
    fixed_t max_mag = 0;
    size_t max_bin = 0;
    
    for (size_t i = 1; i < actual_bins; ++i) {  // Skip DC bin
        if (magnitudes[i] > max_mag) {
            max_mag = magnitudes[i];
            max_bin = i;
        }
    }
    
    result.peak_magnitude = max_mag;
    
    // Convert bin to frequency (fixed-point)
    // freq = bin * sample_rate / (2 * num_bins)
    result.dominant_frequency = static_cast<fixed_t>(
        (static_cast<int64_t>(max_bin) * sample_rate_ * FIXED_ONE) / 
        (2 * actual_bins)
    );
    
    // Find peaks above 20% of max
    fixed_t peak_threshold = fixed_mul(max_mag, float_to_fixed(0.2f));
    result.num_peaks = find_peaks(magnitudes, actual_bins, peak_threshold);
    
    // Compute spectral centroid
    result.spectral_centroid = compute_centroid(magnitudes, actual_bins);
    
    return result;
}

size_t SpectralProcessor::extract_features(
    const int16_t* samples,
    size_t num_samples,
    fixed_t* features,
    size_t max_features
) {
    if (max_features < num_bins_) {
        return 0;
    }
    
    // Compute magnitude spectrum directly into features array
    compute_magnitude_spectrum(samples, num_samples, features);
    
    // Normalize features
    fixed_t max_val = 0;
    for (size_t i = 0; i < num_bins_; ++i) {
        if (features[i] > max_val) max_val = features[i];
    }
    
    if (max_val > 0) {
        for (size_t i = 0; i < num_bins_; ++i) {
            features[i] = (static_cast<int64_t>(features[i]) * FIXED_ONE) / max_val;
        }
    }
    
    return num_bins_;
}

} // namespace core
} // namespace spectral_gate
