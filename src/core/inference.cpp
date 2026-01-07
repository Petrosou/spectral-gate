#include "inference.h"
#include "model_weights.h"
#include <algorithm>

namespace spectral_gate {
namespace core {

using namespace hal;

InferenceEngine::InferenceEngine(
    const int8_t* weights,
    const int8_t* biases,
    size_t input_size,
    size_t output_size,
    fixed_t scale_factor
) : weights_(weights),
    biases_(biases),
    input_size_(input_size),
    output_size_(output_size),
    scale_factor_(scale_factor)
{
}

fixed_t InferenceEngine::dot_product(const fixed_t* input, size_t output_idx) {
    int64_t accumulator = 0;
    
    // Compute weighted sum for this output neuron
    for (size_t i = 0; i < input_size_; ++i) {
        // Weight index: output_idx * input_size + i (row-major)
        size_t weight_idx = output_idx * input_size_ + i;
        int8_t weight = weights_[weight_idx];
        
        // Multiply input (fixed-point) by quantized weight
        // Input is Q15.16, weight is int8 (-128 to 127)
        accumulator += static_cast<int64_t>(input[i]) * weight;
    }
    
    // Scale by quantization factor and add bias
    fixed_t result = static_cast<fixed_t>(accumulator >> 7);  // Divide by 128 (weight scale)
    result = fixed_mul(result, scale_factor_);
    
    // Add bias (scaled from int8)
    int8_t bias = biases_[output_idx];
    result += static_cast<fixed_t>(bias) << (FIXED_SHIFT - 7);
    
    return result;
}

void InferenceEngine::normalize_outputs(fixed_t* outputs, size_t count) {
    // Simple normalization: find max and scale to [0, 1]
    // This is a simplified softmax approximation for embedded use
    
    fixed_t max_val = outputs[0];
    fixed_t min_val = outputs[0];
    
    for (size_t i = 1; i < count; ++i) {
        if (outputs[i] > max_val) max_val = outputs[i];
        if (outputs[i] < min_val) min_val = outputs[i];
    }
    
    fixed_t range = max_val - min_val;
    if (range < float_to_fixed(0.001f)) {
        // Uniform distribution if outputs are nearly equal
        fixed_t uniform = FIXED_ONE / static_cast<fixed_t>(count);
        for (size_t i = 0; i < count; ++i) {
            outputs[i] = uniform;
        }
        return;
    }
    
    // Normalize to [0, 1] range
    fixed_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        outputs[i] = fixed_mul(outputs[i] - min_val, 
                               (FIXED_ONE << 8) / (range >> 8));  // Avoid overflow
        if (outputs[i] < 0) outputs[i] = 0;
        sum += outputs[i];
    }
    
    // Normalize so sum = 1.0
    if (sum > 0) {
        for (size_t i = 0; i < count; ++i) {
            outputs[i] = (static_cast<int64_t>(outputs[i]) * FIXED_ONE) / sum;
        }
    }
}

uint8_t InferenceEngine::argmax(const fixed_t* outputs, size_t count) {
    uint8_t max_idx = 0;
    fixed_t max_val = outputs[0];
    
    for (size_t i = 1; i < count; ++i) {
        if (outputs[i] > max_val) {
            max_val = outputs[i];
            max_idx = static_cast<uint8_t>(i);
        }
    }
    
    return max_idx;
}

InferenceResult InferenceEngine::run(const fixed_t* features, size_t num_features) {
    InferenceResult result;
    result.confidence = 0;
    result.predicted_class = 0;
    
    // Validate input size
    if (num_features != input_size_) {
        return result;
    }
    
    // Compute output for each class
    constexpr size_t MAX_OUTPUTS = 8;
    fixed_t outputs[MAX_OUTPUTS];
    size_t actual_outputs = (output_size_ < MAX_OUTPUTS) ? output_size_ : MAX_OUTPUTS;
    
    for (size_t i = 0; i < actual_outputs; ++i) {
        outputs[i] = dot_product(features, i);
    }
    
    // Apply activation approximation (ReLU-like, then normalize)
    for (size_t i = 0; i < actual_outputs; ++i) {
        if (outputs[i] < 0) outputs[i] = 0;
    }
    
    normalize_outputs(outputs, actual_outputs);
    
    // Find predicted class and confidence
    result.predicted_class = argmax(outputs, actual_outputs);
    result.confidence = outputs[result.predicted_class];
    
    // Clamp confidence to [0, 1]
    if (result.confidence > FIXED_ONE) result.confidence = FIXED_ONE;
    if (result.confidence < 0) result.confidence = 0;
    
    return result;
}

InferenceEngine create_default_engine() {
    return InferenceEngine(
        MODEL_WEIGHTS,
        MODEL_BIASES,
        MODEL_INPUT_SIZE,
        MODEL_OUTPUT_SIZE,
        MODEL_SCALE_FACTOR
    );
}

} // namespace core
} // namespace spectral_gate
