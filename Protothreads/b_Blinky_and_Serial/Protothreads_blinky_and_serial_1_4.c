/*
 Protothreads 1.4 demo code tailored for TMAG5170 (SPI) and TMAG6180 (ADC)
 TWO threads on ONE core:
 -- blinky thread: heart-beat LED
 -- sensor thread: reads SPI and ADC sensors, calculates angle, prints to serial
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "pt_cornell_rp2040_v1_4.h"

// --- Hardware Pins ---
#define LED_PIN 25
#define SPI_PORT spi0
#define PIN_MISO 16  // Pin 21
#define PIN_CS   17  // Pin 22
#define PIN_SCK  18  // Pin 24
#define PIN_MOSI 19  // Pin 25

#define ADC_SIN_PIN 26  // Pin 31
#define ADC_COS_PIN 27  // Pin 32

// --- TMAG5170 Registers ---
#define TMAG5170_DEVICE_CONFIG 0x00
#define TMAG5170_SENSOR_CONFIG 0x01
#define TMAG5170_X_CH_RESULT   0x09
#define TMAG5170_Y_CH_RESULT   0x0A
#define TMAG5170_Z_CH_RESULT   0x0B

int32_t blink_time = 500000; // 0.5s heartbeat

// --- TMAG5170 SPI Helpers ---
void tmag5170_write_reg(uint8_t reg, uint16_t data) {
    uint8_t tx[4];
    // Bits 31:28 = 0000 (Write), Bits 30:24 = address
    tx[0] = (reg & 0x7F); 
    tx[1] = (data >> 8) & 0xFF;
    tx[2] = data & 0xFF;
    tx[3] = 0x00; // CMD/CRC padded with 0 since we disable CRC

    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, tx, 4);
    gpio_put(PIN_CS, 1);
    sleep_us(10);
}

int16_t tmag5170_read_reg(uint8_t reg) {
    uint8_t tx[4] = {0};
    uint8_t rx[4] = {0};
    
    // Bits 31:28 = 1000 (Read), Bits 30:24 = address
    tx[0] = 0x80 | (reg & 0x7F); 
    
    gpio_put(PIN_CS, 0);
    spi_write_read_blocking(SPI_PORT, tx, rx, 4);
    gpio_put(PIN_CS, 1);
    sleep_us(10);

    // The 16-bit result is in bits 23-8 (rx[1] and rx[2])
    return (int16_t)((rx[1] << 8) | rx[2]);
}

void tmag5170_init() {
    // 1. Disable CRC requirement: Send 0x0F000407 
    uint8_t crc_disable_cmd[4] = {0x0F, 0x00, 0x04, 0x07};
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, crc_disable_cmd, 4);
    gpio_put(PIN_CS, 1);
    sleep_ms(1);

    // 2. DEVICE_CONFIG: Operating mode = Continuous (bits 6-4 = 010b)
    tmag5170_write_reg(TMAG5170_DEVICE_CONFIG, 0x0020);

    // 3. SENSOR_CONFIG: Enable X, Y, Z channels (bits 9-6 = 0111b)
    tmag5170_write_reg(TMAG5170_SENSOR_CONFIG, 0x01C0);
}

// ==================================================
// === Blinky Thread
// ==================================================
static PT_THREAD (protothread_toggle25(struct pt *pt))
{
    PT_BEGIN(pt);
    static bool LED_state = false;
    gpio_init(LED_PIN);	
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, true);

    while(1) {
        LED_state = !LED_state;
        gpio_put(LED_PIN, LED_state);
        PT_YIELD_usec(blink_time);
    }
    PT_END(pt);
}

// ==================================================
// === Sensor Read Thread
// ==================================================
static PT_THREAD (protothread_sensors(struct pt *pt))
{
    PT_BEGIN(pt);  
    while(1) {
        // --- Read TMAG5170 (Digital 3D) ---
        int16_t x_raw = tmag5170_read_reg(TMAG5170_X_CH_RESULT);
        int16_t y_raw = tmag5170_read_reg(TMAG5170_Y_CH_RESULT);
        int16_t z_raw = tmag5170_read_reg(TMAG5170_Z_CH_RESULT);

        // Convert to mT (assuming default 50mT range)
        float x_mT = (x_raw / 32768.0f) * 50.0f;
        float y_mT = (y_raw / 32768.0f) * 50.0f;
        float z_mT = (z_raw / 32768.0f) * 50.0f;

        float angle_xy_rad = atan2( -y_mT, -x_mT );
        float angle_xy_deg = angle_xy_rad * (180.0f / (float)M_PI);
        float angle_xz_rad = atan2( -z_mT, -2*x_mT );
        float angle_xz_deg = angle_xz_rad * (180.0f / (float)M_PI);

        // --- Read TMAG6180 (Analog AMR) ---
        adc_select_input(0);
        uint16_t sin_raw = adc_read();
        adc_select_input(1);
        uint16_t cos_raw = adc_read();

        // 12-bit ADC midpoint is ~2048 (representing Vcc/2)
        float v_sin = (sin_raw - 2048.0f);
        float v_cos = (cos_raw - 2048.0f);

        // Calculate Angle (AMR sensor gives 2 periods per 360 degree physical rotation)
        float angle_rad = atan2f(v_sin, v_cos) / 2.0f; 
        float angle_deg = angle_rad * (180.0f / (float)M_PI);
        // if (angle_deg < 0) angle_deg += 180.0f; // Normalize to 0-180

        // --- Output (Aligned with Raw Data) ---
        sprintf(pt_serial_out_buffer, 
            "TMAG5170: X=%7.2f Y=%7.2f Z=%7.2f mT angle_xy=%7.2f angle_xz=%7.2f [Raw: %6d, %6d, %6d] | TMAG6180: Angle=%6.2f deg [Raw: S=%4u, C=%4u]\r\n", 
            x_mT, y_mT, z_mT, angle_xy_deg, angle_xz_deg, x_raw, y_raw, z_raw, angle_deg, sin_raw, cos_raw);
        serial_write; 

        PT_YIELD_usec(100000); // Read 10 times a second
    } 
    PT_END(pt);
}

// ========================================
// === Core 0 Main
// ========================================
int main(){
    sleep_ms(10);
    stdio_init_all();
    printf("\n\rStarting TMAG5170 and TMAG6180 Dual-Sensor Read\n\r");

    // Initialize SPI
    spi_init(SPI_PORT, 1000 * 1000); // 1 MHz
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Initialize CS pin
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // Initialize TMAG5170
    tmag5170_init();

    // Initialize ADC
    adc_init();
    adc_gpio_init(ADC_SIN_PIN);
    adc_gpio_init(ADC_COS_PIN);

    // Config threads
    pt_add_thread(protothread_toggle25);
    pt_add_thread(protothread_sensors);
  
    pt_sched_method = SCHED_ROUND_ROBIN;
    pt_schedule_start;
}