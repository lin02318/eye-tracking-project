/*
 Protothreads 1.4 demo code modified for TMAG6180-Q1 Angle Sensor
 Based on datasheet: SLYS037A - Revised March 2024
 
 Core 0:
 -- blinky thread: heartbeat to show system is running
 -- angle thread: reads ADC/GPIO, computes 360 degree angle
 -- serial thread: prints the calculated angle
*/

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/adc.h" // Added for Analog Reading
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>         // Added for atan2f
#include <pico/multicore.h>
#include "stdlib.h"

// ==========================================
// === protothreads globals
// ==========================================
#include "pt_cornell_rp2040_v1_4.h"

#define LED_PIN 25

// Sensor Pin Definitions
#define PIN_SIN_P 26 // ADC 0
#define PIN_COS_P 27 // ADC 1
#define PIN_Q0     2 // GPIO
#define PIN_Q1     3 // GPIO

// ADC conversion factor (3.3V / 4096)
// Sensor is ratiometric, so VCC fluctuations cancel out if VREF is VCC
#define ADC_CENTER 2048.0f 

// Global variables for shared data
volatile float global_angle = 0.0f;
volatile int32_t blink_time = 500000; // 500ms default

// ==================================================
// === toggle25 thread 
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
// === angle calculation thread
// ==================================================
/* Implements the logic from TMAG6180-Q1 Datasheet Page 28 [cite: 909]
   to extend AMR 180 range to 360 using Hall sensors (Q0/Q1).
*/
static PT_THREAD (protothread_angle(struct pt *pt))
{
    PT_BEGIN(pt);

    // Initialize Hardware
    adc_init();
    adc_gpio_init(PIN_SIN_P);
    adc_gpio_init(PIN_COS_P);
    
    gpio_init(PIN_Q0);
    gpio_set_dir(PIN_Q0, GPIO_IN);
    gpio_init(PIN_Q1);
    gpio_set_dir(PIN_Q1, GPIO_IN);

    static float sin_val, cos_val, measured_angle, abs_angle;
    static uint16_t raw_sin, raw_cos;
    static int q0, q1, q1_q0;

    while(1) {
        // 1. Read Analog Values (Single-Ended Mode)
        adc_select_input(0); // SIN_P
        raw_sin = adc_read();
        
        adc_select_input(1); // COS_P
        raw_cos = adc_read();

        // Remove DC Offset (Vcc/2) [cite: 436]
        sin_val = (float)raw_sin - ADC_CENTER;
        cos_val = (float)raw_cos - ADC_CENTER;

        // 2. Calculate Basic Angle (0-180 range logic)
        // Formula: theta = atan2(Vsin/Vcos) / 2 [cite: 355]
        // Note: atan2f returns radians (-PI to PI). 
        float angle_rad = atan2f(sin_val, cos_val);
        
        // Convert to degrees and divide by 2 per datasheet eq (1)
        float angle_deg_raw = (angle_rad * 180.0f / M_PI) / 2.0f; 

        // Datasheet Step: "If arctan2 function returns from -90deg to 90deg...
        // convert to 0-180 angle range" [cite: 915]
        measured_angle = 90.0f - angle_deg_raw; 

        // 3. Read Quadrant Bits
        q0 = gpio_get(PIN_Q0);
        q1 = gpio_get(PIN_Q1);
        q1_q0 = (q1 << 1) | q0; // Combine into 2-bit integer (00, 01, 10, 11)

        // 4. Extend to 360 Degrees
        // Logic copied directly from Datasheet Page 28 [cite: 916-941]
        
        if (measured_angle > 45.0f && measured_angle < 135.0f) {
            if (q1_q0 == 0b00 || q1_q0 == 0b10) { // around 90 deg
                abs_angle = measured_angle;
            } else { // q1_q0 is 11 or 01, around 270 deg
                abs_angle = measured_angle + 180.0f;
            }
        } 
        else { // measured_angle is 0-45 or 135-180
            if (q1_q0 == 0b00 || q1_q0 == 0b01) { // around 0 deg
                if (measured_angle >= 135.0f) {
                    abs_angle = measured_angle + 180.0f;
                } else {
                    // measured_angle is 0-45
                    abs_angle = measured_angle;
                }
            } 
            else { // Q1_Q0 is 10 or 11, around 180 deg
                if (measured_angle >= 135.0f) {
                    abs_angle = measured_angle;
                } else {
                    // measured_angle is 0-45
                    abs_angle = measured_angle + 180.0f;
                }
            }
        }

        // Update global variable for the serial thread
        global_angle = abs_angle;

        // Yield for 10ms (approx 100Hz update rate)
        PT_YIELD_usec(10000);
    }
    PT_END(pt);
}

// ==================================================
// === serial output thread
// ==================================================
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);  
    while(1) {
         // Print the calculated angle continuously
         // \033[2J\033[H clears terminal on some VT100 clients
         sprintf(pt_serial_out_buffer, "Angle: %.2f deg\r\n", global_angle);
         
         // Non-blocking write
         serial_write; 
         
         // Update terminal approx every 100ms
         PT_YIELD_usec(100000);
    } 
    PT_END(pt);
}

// ========================================
// === core 0 main
// ========================================
int main(){
  sleep_ms(10);
  //===  start the serial i/o ==================
  stdio_init_all();
  printf("\n\rTMAG6180-Q1 Angle Sensor Demo\n\r");

  // === config threads ========================
  pt_add_thread(protothread_toggle25);
  pt_add_thread(protothread_angle);   // Added Angle Thread
  pt_add_thread(protothread_serial);
  
  // === initialize the scheduler =============
  pt_sched_method = SCHED_ROUND_ROBIN;
  pt_schedule_start;
}