/*
 Protothreads demo for:
 1. TMAG6180-Q1 Angle Sensor (ADC/GPIO) - Datasheet SLYS037A
 2. TMAG5170 3D Hall Sensor (SPI) - Datasheet SBASAF4
*/

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/spi.h" // Added for TMAG5170
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>
#include "pt_cornell_rp2040_v1_4.h"

// ==========================================
// === Pin Definitions
// ==========================================
#define LED_PIN 25

// TMAG6180-Q1 Pins (Angle Sensor)
#define PIN_SIN_P 26 // ADC 0 -> [Pin 31]
#define PIN_COS_P 27 // ADC 1 -> [Pin 32]
#define PIN_Q0     2 // GPIO  -> [Pin 4]
#define PIN_Q1     3 // GPIO  -> [Pin 5]

// TMAG5170 Pins (3D Sensor)
#define SPI_PORT spi0
#define PIN_MISO 4 // GPIO  -> [Pin 6]
#define PIN_CS   5 // GPIO  -> [Pin 7]
#define PIN_SCK  6 // GPIO  -> [Pin 9]
#define PIN_MOSI 7 // GPIO  -> [Pin 10]

// ==========================================
// === Constants & Globals
// ==========================================
#define ADC_CENTER 2048.0f 

// TMAG5170 Registers [cite: 4940, 4947]
#define REG_DEVICE_CONFIG   0x00
#define REG_SENSOR_CONFIG   0x01
#define REG_X_CH_RESULT     0x09
#define REG_Y_CH_RESULT     0x0A
#define REG_Z_CH_RESULT     0x0B
#define REG_TEST_CONFIG     0x0F

// Range for A1 variant is +/- 50mT [cite: 3622]
#define TMAG5170_RANGE_MT 50.0f 

// Shared Data
volatile float global_angle = 0.0f;
volatile float global_x_mT = 0.0f;
volatile float global_y_mT = 0.0f;
volatile float global_z_mT = 0.0f;

// Test Data
volatile float test_angle = 0.0f;
volatile float test_angle_alt = 0.0f;
volatile float test_angle_alt_prev = 0.0f;
static int alt_sel = 2;
volatile float sin_test = 0.0f;
volatile float cos_test = 0.0f;
volatile int q0_test = 0;
volatile int q1_test = 0;

// ==========================================
// === Helper Functions
// ==========================================
static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1);
    asm volatile("nop \n nop \n nop");
}

// Reads a 16-bit result from a TMAG5170 register
float read_tmag5170_axis(uint8_t reg_addr) {
    uint8_t tx_buf[4] = {0x80 | reg_addr, 0x00, 0x00, 0x00};
    uint8_t rx_buf[4];

    cs_select();
    spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, 4);
    cs_deselect();

    int16_t raw_data = (int16_t)((rx_buf[1] << 8) | rx_buf[2]);
    
    // Conversion to mT
    return ((float)raw_data / 32768.0f) * TMAG5170_RANGE_MT;
}

// ==================================================
// === Thread: TMAG6180 Angle Sensor
// ==================================================
static PT_THREAD (protothread_angle(struct pt *pt))
{
    PT_BEGIN(pt);
    // Init ADC/GPIO for Angle Sensor
    adc_gpio_init(PIN_SIN_P);
    adc_gpio_init(PIN_COS_P);
    gpio_init(PIN_Q0); gpio_set_dir(PIN_Q0, GPIO_IN);
    gpio_init(PIN_Q1); gpio_set_dir(PIN_Q1, GPIO_IN);

    static float sin_val, cos_val, measured_angle, abs_angle;
    static uint16_t raw_sin, raw_cos;
    static int q0, q1, q0_q1;
    static int q0_q1_prev = 0;

    static float alt_angle_1, alt_angle_2, alt_angle_3;
    static float diff_1, diff_2, diff_3;

    while(1) {
        // Read Analog
        adc_select_input(0); raw_sin = adc_read();
        adc_select_input(1); raw_cos = adc_read();
        
        // Remove DC Offset
        sin_val = (float)raw_sin - ADC_CENTER;
        cos_val = (float)raw_cos - ADC_CENTER;
        sin_test = sin_val;
        cos_test = cos_val;

        // Calculate Basic Angle
        float angle_rad = atan2f(sin_val, cos_val);
        float angle_deg_raw = (angle_rad * 180.0f / M_PI);  // Range -180 ~ 180
        measured_angle = angle_deg_raw / 2.0f;              // Range -90 ~ 90
        test_angle = measured_angle;

        // Read Quadrant
        q0 = gpio_get(PIN_Q0);
        q1 = gpio_get(PIN_Q1);
        q0_q1 = (q0 << 1) | q1;
        q0_test = q0;
        q1_test = q1;

        // Extend to 360 Logic [cite: 916-941]
        if (q0_q1 == 0b00) {
            abs_angle = measured_angle;
        } else if (q0_q1 == 0b01) {
            abs_angle = measured_angle + 180.0f;
        } else if (q0_q1 == 0b11) {
            abs_angle = measured_angle + 180.0f;
        } else if (q0_q1 == 0b10) {
            abs_angle = measured_angle + 360.0f;
        }
        global_angle = abs_angle;

        // angle calculation (alternative approach)
        alt_angle_1 = measured_angle + 90.0f;
        alt_angle_2 = measured_angle + 270.0f;
        alt_angle_3 = measured_angle + 450.0f;

        if (q0_q1 != q0_q1_prev) {
            diff_1 = alt_angle_1 - test_angle_alt;
            diff_2 = alt_angle_2 - test_angle_alt;
            diff_3 = alt_angle_3 - test_angle_alt;

            int min_diff = 1;
            if ( diff_2 < diff_1 ) {
                min_diff = 2;
            }
            if ( (diff_3 < diff_1) && (diff_3 < diff_2) ) {
                min_diff = 3;
            }

            alt_sel = min_diff;
        }

        q0_q1_prev = q0_q1;

        if ( alt_sel == 1 ) {
            test_angle_alt = alt_angle_1;
        } else if ( alt_sel == 2 ) {
            test_angle_alt = alt_angle_2;
        } else {
            test_angle_alt = alt_angle_3;
        }
        PT_YIELD_usec(20000); // 20ms yield (50Hz update)
    }
    PT_END(pt);
}

// ==================================================
// === Thread: TMAG5170 3D Sensor
// ==================================================
static PT_THREAD (protothread_tmag5170(struct pt *pt))
{
    PT_BEGIN(pt);

    // 1. Initialize SPI
    spi_init(SPI_PORT, 1000 * 1000); // 1 MHz
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    cs_deselect();

    // Configure TMAG5170
    // Disable CRC
    uint8_t disable_crc[4] = {0x0F, 0x00, 0x04, 0x00};
    cs_select();
    spi_write_blocking(SPI_PORT, disable_crc, 4);
    cs_deselect();
    
    // Enable X, Y, Z Channels
    uint8_t enable_xyz[4] = {0x01, 0x01, 0xC0, 0x00};
    cs_select();
    spi_write_blocking(SPI_PORT, enable_xyz, 4);
    cs_deselect();

    // Set Continuous Measure Mode
    uint8_t set_active[4] = {0x00, 0x00, 0x20, 0x00};
    cs_select();
    spi_write_blocking(SPI_PORT, set_active, 4);
    cs_deselect();

    sleep_ms(5); // Allow start-up time

    while(1) {
        // Read X Axis (0x09)
        global_x_mT = read_tmag5170_axis(REG_X_CH_RESULT);
        
        // Read Y Axis (0x0A)
        global_y_mT = read_tmag5170_axis(REG_Y_CH_RESULT);
        
        // Read Z Axis (0x0B)
        global_z_mT = read_tmag5170_axis(REG_Z_CH_RESULT);

        PT_YIELD_usec(20000); // 20ms yield (50Hz update)
    }
    PT_END(pt);
}

// ==================================================
// === Serial Output Thread
// ==================================================
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);  
    while(1) {
         // Clear screen
         printf("\033[2J\033[H"); 
         
         printf("=== Sensor Data ===\r\n");
         printf("Angle (TMAG6180): %.2f deg\r\n", global_angle);
         printf("Test sin: %.2f\r\n", sin_test);
         printf("Test cos: %.2f\r\n", cos_test);
         printf("Test Q0: %d\r\n", q0_test);
         printf("Test Q1: %d\r\n", q1_test);
         printf("Test angle: %.2f deg\r\n", test_angle);
         printf("Test angle alt: %.2f deg\r\n", test_angle_alt);
         printf("alt_sel: %d\r\n", alt_sel);
         printf("3D Field (TMAG5170):\r\n");
         printf("  X: %.2f mT\r\n", global_x_mT);
         printf("  Y: %.2f mT\r\n", global_y_mT);
         printf("  Z: %.2f mT\r\n", global_z_mT);
         
         PT_YIELD_usec(100000); // Update display every 100ms
    } 
    PT_END(pt);
}

// ==================================================
// === Toggle Thread (Heartbeat)
// ==================================================
static PT_THREAD (protothread_toggle25(struct pt *pt))
{
    PT_BEGIN(pt);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while(1) {
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        PT_YIELD_usec(500000);
    }
    PT_END(pt);
}

// ========================================
// === Main
// ========================================
int main(){
  stdio_init_all();
  adc_init(); // Init ADC peripheral once
  
  // Wait for serial connection
  sleep_ms(2000);
  printf("\n\rStarting Sensor Demo...\n\r");

  // Add threads
  pt_add_thread(protothread_toggle25);
  pt_add_thread(protothread_angle);     // Thread for TMAG6180
  pt_add_thread(protothread_tmag5170);  // Thread for TMAG5170
  pt_add_thread(protothread_serial);
  
  pt_sched_method = SCHED_ROUND_ROBIN;
  pt_schedule_start;
}