# Spectral-Gate

**Energy-Adaptive TinyML Anomaly Detection for Ultra-Low-Power IoT**

[![Build Status](https://github.com/yourusername/spectral-gate/actions/workflows/cmake_test.yml/badge.svg)](https://github.com/yourusername/spectral-gate/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Overview

Spectral-Gate is an energy-aware structural health monitoring firmware designed for the STM32U5 microcontroller. It combines **spectral analysis** with a **quantized TinyML inference engine** to detect anomalies in vibration data while dynamically adapting its behavior based on battery state.

### Key Innovation: Battery-Aware Dynamic Thresholding

The system implements a novel decision algorithm that adjusts confidence thresholds based on available energy:

| Battery State | Voltage | Threshold Multiplier | Behavior |
|---------------|---------|---------------------|----------|
| **Nominal** | ≥ 3700mV | 1.0x (65%) | Normal operation, active learning enabled |
| **Low** | < 3300mV | 1.2x (78%) | Conservative mode, reduce transmissions |
| **Critical** | < 3000mV | 1.5x (97.5%) | Emergency mode, only safety-critical alerts |

## Features

- **Fixed-Point Arithmetic**: All computations use Q15 fixed-point math for deterministic, efficient execution
- **Quantized Neural Network**: INT8 quantized single-layer perceptron for anomaly classification
- **Spectral Analysis**: FFT-based feature extraction with peak detection
- **Hardware Abstraction Layer (HAL)**: Clean separation between core logic and hardware-specific code
- **Energy-Adaptive Decisions**: Three-tier decision system (SLEEP, TX_ALERT, TX_UNCERTAIN)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │   Spectral  │  │   TinyML     │  │     Decision      │   │
│  │  Processor  │──│  Inference   │──│      Engine       │   │
│  └─────────────┘  └──────────────┘  └───────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                Hardware Abstraction Layer                    │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │  Vibration  │  │   Battery    │  │   Radio/Sleep     │   │
│  │   Sensor    │  │   Monitor    │  │    Control        │   │
│  └─────────────┘  └──────────────┘  └───────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│               STM32U5 Hardware / Mock HAL                    │
└─────────────────────────────────────────────────────────────┘
```

## Project Structure

```
spectral-gate/
├── src/
│   ├── core/
│   │   ├── decision.cpp/h    # Battery-aware decision logic
│   │   ├── inference.cpp/h   # Quantized TinyML engine
│   │   └── spectral.cpp/h    # FFT and feature extraction
│   ├── hal/
│   │   ├── hal_interface.h   # HAL abstract interface
│   │   ├── hal_mock.cpp/h    # PC simulation HAL
│   │   └── hal_stm32u5.cpp   # (Future) Real hardware HAL
│   └── main.cpp              # Demo application
├── data/
│   ├── model_weights.h       # Quantized model weights
│   └── generate_physics.py   # Physics-based data generator
├── tests/
│   └── test_main.cpp         # Unit tests
├── cmake/
│   └── arm-none-eabi.cmake   # STM32 cross-compile toolchain
├── CMakeLists.txt
└── README.md
```

## Building

### Prerequisites

- CMake 3.16+
- C++17 compatible compiler (GCC, Clang, or MSVC)

### Desktop Simulation (Recommended for Development)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Run the Demo

```bash
./build/Release/spectral_gate      # Windows
./build/spectral_gate              # Linux/macOS
```

### Run Unit Tests

```bash
cd build
ctest --output-on-failure
```

### Cross-Compile for STM32U5 (Advanced)

```bash
mkdir build-arm && cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake
cmake --build .
```

## Energy Budget

The system is designed for multi-year battery operation on a CR2032 coin cell:

| Operation | Current Draw | Duration | Energy per Event |
|-----------|-------------|----------|------------------|
| Deep Sleep | 2 µA | Continuous | Baseline |
| Wake + Sample | 5 mA | 10 ms | 50 µJ |
| FFT + Inference | 15 mA | 5 ms | 75 µJ |
| LoRa TX (14 dBm) | 40 mA | 50 ms | 2000 µJ |

**Target**: 10-year battery life with 1 wake/hour and <1% TX rate.

## Simulation Results

The following output demonstrates the **3-Phase Energy-Adaptive Demo** showing how the same uncertain sensor data produces different decisions based on battery state:

```
╔══════════════════════════════════════════════════════════════════════════════╗
║              SPECTRAL-GATE Energy-Adaptive Demo (STM32U5)                    ║
╠══════════════════════════════════════════════════════════════════════════════╣
║  Demonstrating battery-aware dynamic thresholding for IoT anomaly detection  ║
╚══════════════════════════════════════════════════════════════════════════════╝

Base Threshold: 65% | Low Battery Multiplier: 1.2x | Critical Multiplier: 1.5x
Battery Levels: CRITICAL < 3000mV | LOW < 3300mV | NOMINAL >= 3700mV

┌──────────┬────────────┬─────────────┬───────────┬─────────────┬───────────────┐
│   Time   │ V_bat (mV) │ Probability │ Threshold │   Decision  │    Reason     │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ PHASE 1: MORNING - High Energy, Abundant Resources                           │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ 06:00    │       4100 │       55.0% │     65.0% │ TX_UNCERTAIN│ Active Learn  │
│ 07:00    │       4100 │       58.0% │     65.0% │ TX_UNCERTAIN│ Active Learn  │
│ 08:00    │       4050 │       52.0% │     65.0% │ TX_UNCERTAIN│ Active Learn  │
│ 09:00    │       4000 │       60.0% │     65.0% │ TX_UNCERTAIN│ Active Learn  │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ PHASE 2: EVENING - Low Energy, Conservation Mode                             │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ 17:00    │       2900 │       55.0% │     97.5% │ SLEEP       │ Energy Veto   │
│ 18:00    │       2850 │       58.0% │     97.5% │ SLEEP       │ Energy Veto   │
│ 19:00    │       2800 │       52.0% │     97.5% │ SLEEP       │ Energy Veto   │
│ 20:00    │       2750 │       60.0% │     97.5% │ SLEEP       │ Energy Veto   │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ PHASE 3: DAMAGE DETECTED - Safety Critical Override                          │
├──────────┼────────────┼─────────────┼───────────┼─────────────┼───────────────┤
│ 21:00    │       2700 │       98.0% │     97.5% │ TX_ALERT    │ Safety Crit   │
│ 21:30    │       2650 │       99.0% │     97.5% │ TX_ALERT    │ Safety Crit   │
│ 22:00    │       2600 │       98.5% │     97.5% │ TX_ALERT    │ Safety Crit   │
│ 22:30    │       2550 │       99.5% │     97.5% │ TX_ALERT    │ Safety Crit   │
└──────────┴────────────┴─────────────┴───────────┴─────────────┴───────────────┘

════════════════════════════════════════════════════════════════════════════════
                           DEMO SUMMARY
════════════════════════════════════════════════════════════════════════════════

  Energy-Adaptive Behavior Demonstrated:
  ──────────────────────────────────────
  • Phase 1 (High Battery): TX_UNCERTAIN decisions = 4
    → System invests energy in active learning when resources abundant

  • Phase 2 (Low Battery):  SLEEP decisions = 4
    → Same uncertain data VETOED to conserve energy

  • Phase 3 (Critical):     TX_ALERT decisions = 4
    → Safety-critical alerts ALWAYS transmitted regardless of battery

  HAL Statistics:
  ───────────────
  • Total Transmissions: 8
  • Total Sleep Time:    4000 ms

════════════════════════════════════════════════════════════════════════════════
```

### Key Observations

1. **Phase 1 (Morning)**: With abundant battery (4100mV), the system transmits uncertain readings for cloud-based active learning
2. **Phase 2 (Evening)**: Same uncertainty levels are **vetoed** when battery drops below critical (2900mV)
3. **Phase 3 (Damage)**: High-confidence anomalies **always transmit** regardless of battery state — safety overrides energy conservation

## API Reference

### Decision Engine

```cpp
// Evaluate sensor data and make energy-aware decision
Decision evaluate_structure(
    const SpectralResult& spectral,
    const InferenceResult& inference,
    uint16_t battery_mv,
    const ThresholdConfig& config
);

// Decision outcomes
enum class Decision : uint8_t {
    SLEEP = 0,        // Return to low-power sleep
    TX_ALERT = 1,     // Transmit confirmed anomaly
    TX_UNCERTAIN = 2  // Transmit for active learning
};
```

### Inference Engine

```cpp
// Create engine with compiled weights
InferenceEngine engine = create_default_engine();

// Run inference on spectral features
InferenceResult result = engine.run(features, num_features);
```

## Target Hardware

- **MCU**: STM32U585 (Cortex-M33, 160MHz, 2MB Flash, 786KB SRAM)
- **Sensor**: MEMS accelerometer (e.g., LIS2DW12)
- **Radio**: LoRa SX1262 or NB-IoT module
- **Power**: CR2032 coin cell (225 mAh)

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting PRs.

## Acknowledgments

- STM32 HAL libraries from STMicroelectronics
- Fixed-point math inspired by libfixmath
- TinyML concepts from TensorFlow Lite Micro
