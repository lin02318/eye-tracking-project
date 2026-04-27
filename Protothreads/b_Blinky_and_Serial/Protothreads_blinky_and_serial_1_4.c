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

// --- Global Recording & Timing State ---
volatile bool is_recording = false;
volatile bool sample_now = false;   // Our new IRQ flag

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

    // 3. SENSOR_CONFIG: XYZ enabled (0x01C0) + Set ranges to +/- 25mT (0x0015)
    // Resulting value: 0x01D5
    tmag5170_write_reg(TMAG5170_SENSOR_CONFIG, 0x01D5);
}

// ==================================================
// === 100 Hz Hardware Timer Interrupt
// ==================================================
bool repeating_timer_callback(struct repeating_timer *t) {
    // If we are recording, tell the main thread it is time to sample!
    if (is_recording) {
        sample_now = true;
    }
    return true; // Return true to keep the timer repeating
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
        
        // Fast blink (50ms) if recording, normal heartbeat (500ms) otherwise
        if (is_recording) {
            PT_YIELD_usec(50000); 
        } else {
            PT_YIELD_usec(500000); 
        }
    }
    PT_END(pt);
}

// ==================================================
// === Sensor Read Thread (Hardware 100Hz Paced)
// ==================================================
static PT_THREAD (protothread_sensors(struct pt *pt))
{
    PT_BEGIN(pt);

    static uint32_t start_time;
    static struct repeating_timer timer; // Timer structure

    while(1) {
        // --- 1. Wait for Trigger ---
        if (!is_recording) {
            int c;
            // Drain the buffer and check for 'r'
            while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
                if (c == 'r' || c == 'R') {
                    is_recording = true;
                    start_time = time_us_32();
                    sample_now = false; // Reset flag just in case
                    
                    // START THE TIMER: -10000 us means exactly 10ms from the *start* of the last call
                    add_repeating_timer_us(-10000, repeating_timer_callback, NULL, &timer);
                    
                    // Print header
                    printf("Time(ms),5170_X,5170_Y,5170_Z,6180_SIN,6180_COS\r\n");
                }
            }
        }

        // --- 2. Record and Stream Data ---
        if (is_recording) {
            
            // === WAIT FOR HARDWARE INTERRUPT ===
            // The thread pauses here until the 10ms timer fires.
            // When it wakes up, it resumes exactly on the next line!
            PT_YIELD_UNTIL(pt, sample_now);
            sample_now = false; // Acknowledge and clear the flag

            // --- CALCULATE TIME AFTER WAKING UP ---
            uint32_t current_time = time_us_32();
            uint32_t elapsed_us = current_time - start_time;

            if (elapsed_us > 5000000) { // 5 Seconds
                is_recording = false;
                cancel_repeating_timer(&timer); // STOP THE TIMER
                printf("DONE\r\n"); 
            } else {
                // --- Read TMAG5170 Raw Data ---
                int16_t x_raw = tmag5170_read_reg(TMAG5170_X_CH_RESULT); 
                int16_t y_raw = tmag5170_read_reg(TMAG5170_Y_CH_RESULT); 
                int16_t z_raw = tmag5170_read_reg(TMAG5170_Z_CH_RESULT); 

                // --- Read TMAG6180 Raw Data ---
                adc_select_input(0);
                adc_read(); // Dummy read
                int16_t v_sin_raw = (int16_t)adc_read() - 2048; 
                
                adc_select_input(1);
                adc_read(); // Dummy read
                int16_t v_cos_raw = (int16_t)adc_read() - 2048;

                // Output raw integers
                printf("%u,%d,%d,%d,%d,%d\r\n", 
                    elapsed_us / 1000, x_raw, y_raw, z_raw, v_sin_raw, v_cos_raw);
            }
        } else {
            // Yield briefly while waiting for Python trigger
            PT_YIELD_usec(50000); 
        }
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
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
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