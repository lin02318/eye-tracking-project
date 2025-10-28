
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
#include <math.h>
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
float Sin_P;
float Cos_P;
float angle;

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
      printf("Sin P: %f V -- Cos P: %f V\n",
              result0 * conversion_factor, result1 * conversion_factor);

      // Calculate angle
      Sin_P = result0 * conversion_factor - 1.65f;
      Cos_P = result1 * conversion_factor - 1.65f;
      angle = atan2f(Sin_P, Cos_P) * (180.0f / 3.14159265f);
      printf("Angle: %f degrees\n", angle);

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

// === SPI 引脚定义 ===
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  2
#define PIN_MOSI 3

// === SPI 常量定义 ===
#define SPI_PORT spi0
#define SPI_BAUDRATE 1000000  // 1 MHz
#define SPI_MODE 1            // TMAG5170 uses Mode 1 (CPOL=0, CPHA=1)

// === TMAG5170 寄存器地址 ===
#define TMAG5170_DEVICE_CONFIG      0x00
#define TMAG5170_SENSOR_CONFIG      0x01
#define TMAG5170_SYSTEM_CONFIG      0x02
#define TMAG5170_TEMPERATURE_RESULT 0x0C
#define TMAG5170_X_RESULT           0x0D
#define TMAG5170_Y_RESULT           0x0E
#define TMAG5170_Z_RESULT           0x0F

// === 生成读/写命令帧函数 ===
// TMAG5170 Frame: [31:28] CRC | [27:24] CMD | [23:8] DATA | [7:0] ADDR

uint32_t make_read_frame(uint8_t reg_addr) {
    uint8_t CRC_phase = 0x0;      // 暂不使用 CRC
    uint8_t CMD_phase = 0x1;      // 0x1 = READ
    uint16_t DATA_phase = 0x0000; // dummy data
    uint8_t ADDR_phase = reg_addr;

    uint32_t frame = ((uint32_t)CRC_phase << 28)
                   | ((uint32_t)CMD_phase << 24)
                   | ((uint32_t)DATA_phase << 8)
                   | ((uint32_t)ADDR_phase);
    return frame;
}

uint32_t make_write_frame(uint8_t reg_addr, uint16_t data) {
    uint8_t CRC_phase = 0x0; // 暂不使用 CRC
    uint8_t CMD_phase = 0x2; // 0x2 = WRITE
    uint16_t DATA_phase = data;
    uint8_t ADDR_phase = reg_addr;

    uint32_t frame = ((uint32_t)CRC_phase << 28)
                   | ((uint32_t)CMD_phase << 24)
                   | ((uint32_t)DATA_phase << 8)
                   | ((uint32_t)ADDR_phase);
    return frame;
}

// === SPI 初始化 ===
void tmag5170_spi_init() {
    spi_init(SPI_PORT, SPI_BAUDRATE);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1); // 片选拉高（未激活）

    sleep_ms(100);
}

// === 32-bit SPI 传输函数 ===
uint32_t spi_transfer32(uint32_t txdata) {
    uint8_t tx_buf[4] = {
        (uint8_t)(txdata >> 24),
        (uint8_t)(txdata >> 16),
        (uint8_t)(txdata >> 8),
        (uint8_t)(txdata & 0xFF)
    };
    uint8_t rx_buf[4];
    gpio_put(PIN_CS, 0);
    spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, 4);
    gpio_put(PIN_CS, 1);
    uint32_t rxdata = ((uint32_t)rx_buf[0] << 24) |
                      ((uint32_t)rx_buf[1] << 16) |
                      ((uint32_t)rx_buf[2] << 8)  |
                      ((uint32_t)rx_buf[3]);
    return rxdata;
}

// === 写寄存器 ===
void tmag5170_write(uint8_t reg_addr, uint16_t value) {
    uint32_t cmd = make_write_frame(reg_addr, value);
    spi_transfer32(cmd);
    sleep_us(10);
}

// === 读寄存器 ===
uint16_t tmag5170_read(uint8_t reg_addr) {
    uint32_t cmd = make_read_frame(reg_addr);
    uint32_t rx;

    // 第一次发命令，芯片准备数据
    spi_transfer32(cmd);
    sleep_us(5);

    // 第二次读取返回值（真正数据）
    rx = spi_transfer32(0x00000000);
    uint16_t data = (rx >> 8) & 0xFFFF; // 提取 Data phase
    return data;
}

// === 主程序 ===
int main() {
    stdio_init_all();
    tmag5170_spi_init();

    printf("TMAG5170 SPI Communication (32-bit Frame) Test Start...\n");

    // === 示例：读取 DEVICE_CONFIG ===
    uint16_t dev_config = tmag5170_read(TMAG5170_DEVICE_CONFIG);
    printf("DEVICE_CONFIG = 0x%04X\n", dev_config);

    // === 示例：读取 X/Y/Z 磁场 & 温度 ===
    while (1) {
        uint16_t x_val = tmag5170_read(TMAG5170_X_RESULT);
        uint16_t y_val = tmag5170_read(TMAG5170_Y_RESULT);
        uint16_t z_val = tmag5170_read(TMAG5170_Z_RESULT);
        uint16_t temp  = tmag5170_read(TMAG5170_TEMPERATURE_RESULT);

        printf("X=0x%04X  Y=0x%04X  Z=0x%04X  Temp=0x%04X\n",
               x_val, y_val, z_val, temp);
        sleep_ms(500);
    }
}
