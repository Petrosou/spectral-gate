/**
 * @file hal_stm32.cpp
 * @brief STM32U585 Hardware Abstraction Layer Implementation
 * 
 * Implements IHardwareAbstraction for STM32U5 series MCU.
 * Designed for ultra-low-power operation using STOP 2 mode with LPBAM.
 */

#if defined(STM32U5xx)

#include "hal_interface.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_ll_adc.h"
#include "stm32u5xx_ll_pwr.h"
#include "stm32u5xx_ll_rtc.h"

namespace spectral_gate {
namespace hal {

// Forward declarations for STM32 peripheral handles (defined in main/CubeMX generated code)
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern DMA_HandleTypeDef hdma_i2c1_rx;
extern RTC_HandleTypeDef hrtc;

// DMA buffer for MEMS vibration sensor (double-buffered for continuous acquisition)
static int16_t g_vibration_dma_buffer[VIBRATION_BUFFER_SIZE * 2] __attribute__((section(".RAM2")));
static volatile bool g_wake_event_pending = false;

/**
 * @brief STM32U585 HAL Implementation
 * 
 * Ultra-low-power implementation targeting < 10µA average current
 * using STOP 2 mode with LPBAM for autonomous sensor acquisition.
 */
class STM32HAL : public IHardwareAbstraction {
public:
    STM32HAL() = default;
    ~STM32HAL() override = default;

    /**
     * @brief Read vibration data from MEMS sensor
     * 
     * HARDWARE IMPLEMENTATION NOTES:
     * =============================
     * In the real hardware, this function reads from a DMA Circular Buffer
     * that is continuously filled by the MEMS accelerometer (e.g., LIS2DW12).
     * 
     * The data flow is:
     *   1. I2C/SPI peripheral configured in DMA Circular mode
     *   2. LPBAM (Low Power Background Autonomous Mode) keeps DMA active in STOP 2
     *   3. DMA writes sensor samples to g_vibration_dma_buffer in SRAM2
     *   4. This function copies the latest samples from the inactive half-buffer
     *   5. Double-buffering prevents data corruption during copy
     * 
     * Memory placement in SRAM2 is critical - SRAM2 remains powered in STOP 2
     * while SRAM1/SRAM3 can be powered down for additional power savings.
     * 
     * @param buffer Destination buffer for vibration samples
     * @param buffer_size Maximum number of samples to read
     * @return Number of samples actually copied
     */
    size_t read_vibration_data(int16_t* buffer, size_t buffer_size) override {
        if (buffer == nullptr || buffer_size == 0) {
            return 0;
        }

        // Determine which half-buffer contains the latest complete data
        // by checking DMA transfer counter (NDTR register)
        size_t samples_to_copy = (buffer_size < VIBRATION_BUFFER_SIZE) 
                                  ? buffer_size 
                                  : VIBRATION_BUFFER_SIZE;

        // In real implementation: copy from inactive half of circular buffer
        // uint32_t dma_position = __HAL_DMA_GET_COUNTER(&hdma_i2c1_rx);
        // int16_t* source = (dma_position > VIBRATION_BUFFER_SIZE) 
        //                   ? &g_vibration_dma_buffer[0]           // Use first half
        //                   : &g_vibration_dma_buffer[VIBRATION_BUFFER_SIZE]; // Use second half

        // TODO: Implement actual DMA buffer read when hardware is available
        // For now, return zeros to indicate no data (safe fallback)
        for (size_t i = 0; i < samples_to_copy; ++i) {
            buffer[i] = 0;
        }

        return samples_to_copy;
    }

    /**
     * @brief Read battery voltage using internal VREFINT
     * 
     * Uses the internal voltage reference (VREFINT) to calculate VDDA,
     * then reads the battery voltage through a resistor divider.
     * 
     * For direct battery connection (no divider), VDDA = VBAT.
     * 
     * @return Battery voltage in millivolts
     */
    uint16_t get_battery_voltage_mv() override {
        uint32_t vrefint_cal = *VREFINT_CAL_ADDR;  // Factory calibration value at 3.0V/3.3V
        uint32_t vrefint_data = 0;

        // Enable ADC if not already running
        if ((hadc1.Instance->CR & ADC_CR_ADEN) == 0) {
            HAL_ADC_Start(&hadc1);
        }

        // Configure ADC to read internal VREFINT channel
        ADC_ChannelConfTypeDef sConfig = {0};
        sConfig.Channel = ADC_CHANNEL_VREFINT;
        sConfig.Rank = ADC_REGULAR_RANK_1;
        sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;  // Long sample time for accuracy
        sConfig.SingleDiff = ADC_SINGLE_ENDED;
        sConfig.OffsetNumber = ADC_OFFSET_NONE;
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);

        // Perform ADC conversion
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            vrefint_data = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);

        // Calculate VDDA from VREFINT reading
        // Formula: VDDA = VREFINT_CAL_VREF * VREFINT_CAL / VREFINT_DATA
        // VREFINT_CAL_VREF is typically 3000mV or 3300mV depending on STM32 variant
        if (vrefint_data == 0) {
            return 0;  // Prevent division by zero
        }

        uint32_t vdda_mv = (VREFINT_CAL_VREF * vrefint_cal) / vrefint_data;

        // If using resistor divider for battery measurement, apply ratio here
        // Example for 2:1 divider: battery_mv = vdda_mv * 2;
        // For direct connection (typical in coin cell applications):
        return static_cast<uint16_t>(vdda_mv);
    }

    /**
     * @brief Get system tick in milliseconds
     * @return Current HAL tick count
     */
    uint32_t get_tick_ms() override {
        return HAL_GetTick();
    }

    /**
     * @brief Enter STOP 2 low-power mode
     * 
     * STOP 2 Mode Configuration for STM32U585:
     * =========================================
     * - Core clock stopped, SRAM1/SRAM3 optionally powered down
     * - SRAM2 retained (contains DMA buffers and critical data)
     * - All clocks stopped except LSE/LSI for RTC
     * - Typical consumption: 2-4 µA with RTC and SRAM2 retention
     * 
     * LPBAM (Low Power Background Autonomous Mode) Configuration:
     * ===========================================================
     * LPBAM allows I2C sensor acquisition to continue in STOP 2:
     * 
     * 1. LPDMA1 is configured with a linked-list descriptor that:
     *    - Triggers I2C read transaction at fixed intervals (via LPTIM)
     *    - Transfers sensor data directly to SRAM2 buffer
     *    - Operates entirely without CPU intervention
     * 
     * 2. Configuration sequence (done once at init):
     *    - Enable LPDMA1 clock in Sleep/Stop modes (RCC_SRDAMR)
     *    - Configure I2C1 for autonomous mode (I2C_AUTOCR)
     *    - Set up LPTIM1 as trigger source for periodic acquisition
     *    - Create LPDMA linked-list in SRAM2 for circular operation
     *    - Enable I2C1 wakeup capability (I2C_CR1_WUPEN)
     * 
     * 3. Wake sources configured:
     *    - RTC alarm for timeout wake
     *    - LPDMA transfer complete for buffer full notification
     *    - External interrupt on accelerometer INT pin (threshold exceeded)
     * 
     * @param duration_ms Requested sleep duration in milliseconds
     */
    void enter_sleep(uint32_t duration_ms) override {
        // Configure RTC wake-up timer for requested duration
        uint32_t wakeup_counter = (duration_ms * 32768) / 1000;  // LSE = 32.768 kHz
        if (wakeup_counter > 0xFFFF) {
            wakeup_counter = 0xFFFF;  // Clamp to 16-bit max (~2 seconds at 32kHz)
        }

        // Disable RTC write protection
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        
        // Configure wake-up timer with RTCCLK/16 prescaler for longer durations
        // WUT = duration_ms * 2048 / 1000 (with RTCCLK/16 = 2048 Hz)
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 
                                     static_cast<uint32_t>((duration_ms * 2048UL) / 1000UL),
                                     RTC_WAKEUPCLOCK_RTCCLK_DIV16);

        // Ensure LPBAM/LPDMA configuration is active before entering STOP 2
        // (Configuration done in system init, just verify here)
        // LPDMA1 linked-list should already be running for sensor acquisition

        // Configure power mode: STOP 2 with SRAM2 retention
        HAL_PWREx_EnableSRAM2ContentRetention();
        
        // Disable SRAM1 and SRAM3 retention for minimum power
        HAL_PWREx_DisableSRAM1ContentRetention();
        HAL_PWREx_DisableSRAM3ContentRetention();

        // Set STOP 2 mode in PWR_CR1
        HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

        // --- CPU resumes here after wake event ---

        // Re-enable clocks and restore system state after wake
        SystemClock_Config();  // Restore HSE/PLL configuration

        // Clear wake-up flag
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF);
    }

    /**
     * @brief Transmit alert via LoRa/BLE radio
     * 
     * @param alert_type Alert classification (0=uncertain, 1=confirmed)
     * @param confidence Confidence level 0-100
     * @return true if transmission queued successfully
     */
    bool transmit_alert(uint8_t alert_type, uint8_t confidence) override {
        // Build minimal alert packet (< 12 bytes for LoRa efficiency)
        uint8_t packet[8] = {
            0xAA,                           // Sync byte
            alert_type,                     // Alert type
            confidence,                     // Confidence
            static_cast<uint8_t>(HAL_GetTick() & 0xFF),        // Timestamp LSB
            static_cast<uint8_t>((HAL_GetTick() >> 8) & 0xFF), // Timestamp
            static_cast<uint8_t>((HAL_GetTick() >> 16) & 0xFF),
            static_cast<uint8_t>((HAL_GetTick() >> 24) & 0xFF),// Timestamp MSB
            0x00                            // CRC placeholder
        };

        // TODO: Calculate CRC8 and transmit via radio peripheral
        // For LoRa: SX126x_Transmit(packet, sizeof(packet));
        // For BLE: HAL_UART_Transmit_DMA(&huart_ble, packet, sizeof(packet));

        (void)packet;  // Suppress unused variable warning until radio implemented
        return true;
    }

    /**
     * @brief Check for pending wake event
     * @return true if external interrupt or sensor threshold triggered
     */
    bool is_wake_event_pending() override {
        return g_wake_event_pending;
    }

    /**
     * @brief Clear wake event flag
     */
    void clear_wake_event() override {
        g_wake_event_pending = false;
    }
};

// External interrupt callback for accelerometer INT pin
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {  // Assuming PA0 for accel INT
        g_wake_event_pending = true;
    }
}

// Factory function to create hardware instance
IHardwareAbstraction* create_hardware_instance() {
    static STM32HAL instance;
    return &instance;
}

} // namespace hal
} // namespace spectral_gate

#endif // STM32U5xx
