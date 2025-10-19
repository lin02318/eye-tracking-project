
/*
 Protothreads 1.4 demo code:
 for more info see
 https://people.ece.cornell.edu/land/courses/ece4760/RP2040/protothreads_1_4/index_Protothreads_1_4.html

 ONE thread on ONE core:

 Core 0:
 -- blinky thread: just blinks on rp2040

 Core 1:
-- no threads

 */

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <pico/multicore.h>
#include "stdlib.h"

// ==========================================
// === protothreads globals
// ==========================================
// protothreads header
#include "pt_cornell_rp2040_v1_4.h"
#define LED_PIN 25
#define ADC_PIN0 26
#define ADC_PIN1 27

// ADC convertion
const float conversion_factor = 3.3f / (1 << 12);   // 12 bit ADC
uint16_t result0, result1;   // ADC read results
float voltage_difference ;

// ==================================================
// === toggle25 thread 
// ==================================================
//  
static PT_THREAD (protothread_toggle25(struct pt *pt))
{
    // every thread begins with PT_BEGIN(pt);
    PT_BEGIN(pt);
    // always use static variables in a thread!!
    // if you eliminate the 'static' watch what happens
    static bool LED_state = false ;
    //
     // set up LED gpio 25
     gpio_init(LED_PIN) ;	
     gpio_set_dir(LED_PIN, GPIO_OUT) ;
     gpio_put(LED_PIN, true);

    while(1) {
      // toggle gpio 25
      LED_state = !LED_state ;
      gpio_put(LED_PIN, LED_state);

      // Read ADC values
      adc_select_input(0); // Select ADC input 0 (GPIO26)
      result0 = adc_read();
      adc_select_input(1); // Select ADC input 1 (GPIO27)
      result1 = adc_read();
      voltage_difference = (result1 - result0) * conversion_factor;
      printf("Raw value 0: 0x%03x, voltage: %f V -- Raw value 1: 0x%03x, voltage: %f V -- Voltage difference: %f V\n",
              result0, result0 * conversion_factor, result1, result1 * conversion_factor, voltage_difference);

      PT_YIELD_usec(100000) ;
      // NEVER exit WHILE in a thread
    } // END WHILE(1)
    // every thread ends with PT_END(pt);
    PT_END(pt);
} // end blink thread

// ========================================
// === core 0 main
// ========================================
int main(){
  // needed, but I dont know why
  sleep_ms(10);
  //===  start the serial i/o ==================
  stdio_init_all() ;
  // announce the threader version on system reset
  // if there is a seral terminal attached
  printf("\n\rProtothreads RP2040 v1.4 two-core, priority\n\r");

  // Initialize ADC
  adc_init();
  adc_gpio_init(ADC_PIN0);
  adc_gpio_init(ADC_PIN1);

  // === config threads ========================
  // for core 0
  pt_add_thread(protothread_toggle25);
  
  // === initalize the scheduler ===============
  // method is either:
  //   SCHED_PRIORITY or SCHED_ROUND_ROBIN
  pt_sched_method = SCHED_ROUND_ROBIN ;
  pt_schedule_start ;
  // !!pt_schedule_start NEVER exits
  // ===========================================
} // end main
///////////
// end ////
///////////