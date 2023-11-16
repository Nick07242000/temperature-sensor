/* libs included */
#include "lpc17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_gpdma.h"


/* func declarations */
void configPRIO();
void configPINS();
void configADC();
void configTMR();
void configUART();
void switchActiveDisplay();
void setLED(uint8_t value);
void setDisplayValue(uint8_t display);
void loadSevenSegValue(uint8_t value, uint8_t display);
void trigger_memory_transaction(uint16_t* value);


/* global variables declaration */
uint8_t tmr_inter_count = 0;
uint8_t uart_inter_count = 0;
uint8_t enabled_seven_seg = 0;

uint16_t adc_value;

// all displays 0 by default
// displays ordered as 1, 2, 3
uint32_t port_0_on_vals[3] = {50823168, 50823168, 50823168};
uint32_t port_0_off_vals[3] = {0, 0, 0};
// two bits on port 1: segment g and point
uint8_t port_1_on_vals[3] = {0, 0, 0};
uint8_t port_1_off_vals[3] = {1, 1, 1};

uint16_t last_10_values[10] = {0};


/* func definitions */
int main(void) {
  configPRIO();
  configPINS();
  configADC();
  configUART();
  configTMR();

  while (1) {}

  return 0;
}


void configPRIO(void) {
  NVIC_SetPriority(UART0_IRQn, 0);
  NVIC_SetPriority(TIMER0_IRQn, 1);
  NVIC_SetPriority(ADC_IRQn, 2);
}


void configPINS(void) {
  PINSEL_CFG_Type cfg;
  // both from port 1 first
  cfg.Portnum = PINSEL_PORT_1;
  cfg.Funcnum = PINSEL_FUNC_0;
  cfg.Pinmode = PINSEL_PINMODE_PULLUP;
  cfg.OpenDrain = PINSEL_PINMODE_NORMAL;

  cfg.Pinnum = PINSEL_PIN_30;
  PINSEL_ConfigPin(&cfg);
  cfg.Pinnum = PINSEL_PIN_31;
  PINSEL_ConfigPin(&cfg);

  // all from port 0
  cfg.Portnum = PINSEL_PORT_0;
  uint8_t gpioPins[12] = {0, 1, 6, 7, 8, 9, 15, 16, 17, 18, 24, 25};

  for (int i = 0; i < 12; i++) {
    cfg.Pinnum = gpioPins[i];
    PINSEL_ConfigPin(&cfg);
  }

  // UART pins
  cfg.Funcnum = PINSEL_FUNC_1;
  cfg.Pinnum = PINSEL_PIN_2;
  PINSEL_ConfigPin(&cfg);
  cfg.Pinnum = PINSEL_PIN_3;
  PINSEL_ConfigPin(&cfg);

  // pins direction setting
  GPIO_SetDir(1, 3 << 30, 1);
  GPIO_SetDir(0, 0b11010001111000001111000011, 1);
}


void configADC(void) {
  ADC_Init(LPC_ADC, 200000);

  // set pin for input
  PINSEL_CFG_Type pin_0_23;
  pin_0_23.Portnum = PINSEL_PORT_0;
  pin_0_23.Pinnum = PINSEL_PIN_23;
  pin_0_23.Funcnum = PINSEL_FUNC_1;
  pin_0_23.Pinmode = PINSEL_PINMODE_TRISTATE;
  pin_0_23.OpenDrain = PINSEL_PINMODE_NORMAL;
  PINSEL_ConfigPin(&pin_0_23);

  ADC_ChannelCmd(LPC_ADC, 0, ENABLE);

  NVIC_EnableIRQ(ADC_IRQn);
}


void configTMR(void) {
  TIM_TIMERCFG_Type tmrCfg;
  TIM_MATCHCFG_Type mchCfg;

  tmrCfg.PrescaleOption = TIM_PRESCALE_TICKVAL;
  tmrCfg.PrescaleValue = 1;

  mchCfg.MatchChannel = 0;
  mchCfg.IntOnMatch = ENABLE;
  mchCfg.ResetOnMatch = ENABLE;
  mchCfg.StopOnMatch = DISABLE;
  mchCfg.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
  mchCfg.MatchValue = 165000;

  TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &tmrCfg);
  TIM_ConfigMatch(LPC_TIM0, &mchCfg);
  TIM_Cmd(LPC_TIM0, ENABLE);

  NVIC_EnableIRQ(TIMER0_IRQn);
}


void configUART(void) {
  UART_CFG_Type uartCfg;
  UART_ConfigStructInit(&uartCfg);
  uartCfg.Baud_rate = 300;
  UART_Init((LPC_UART_TypeDef *)LPC_UART0, &uartCfg);

  UART_FIFO_CFG_Type fifoCfg;
  UART_FIFOConfigStructInit(&fifoCfg);
  UART_FIFOConfig((LPC_UART_TypeDef *)LPC_UART0, &fifoCfg);

  UART_TxCmd((LPC_UART_TypeDef *)LPC_UART0, ENABLE);
  UART_IntConfig((LPC_UART_TypeDef *)LPC_UART0, UART_INTCFG_RBR, ENABLE);

  NVIC_EnableIRQ(UART0_IRQn);
}


void TIMER0_IRQHandler(void) {
  switchActiveDisplay();

  tmr_inter_count++;

  if (tmr_inter_count == 64) {
    ADC_StartCmd(LPC_ADC, ADC_START_NOW);
  }

  if (tmr_inter_count == 128) {
    uint8_t split_adc_value[2] = {(uint8_t)(adc_value), (uint8_t)(adc_value >> 8)};
    UART_Send((LPC_UART_TypeDef *)LPC_UART0, split_adc_value, 2, BLOCKING);
    tmr_inter_count = 0;
  }

  TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
}


void ADC_IRQHandler(void) {
  adc_value = ((ADC_GlobalGetData(LPC_ADC) >> 4) & 0xFFF);
}


void UART0_IRQHandler(void) {
  uint8_t value = UART_ReceiveByte((LPC_UART_TypeDef *)LPC_UART0);
  static uint16_t bcd_last_value = 0;

  if (value == 255) {
    uart_inter_count = 0;
    return;
  }

  switch (uart_inter_count) {
    case 0:
      setLED(value);
      break;
    case 1:
      loadSevenSegValue(value, 0);
      // MSB
      bcd_last_value = (uint16_t)value;
      break;
    case 2:
      loadSevenSegValue(value, 1);
      bcd_last_value |=  (((uint16_t)value) << 4);
      break;
    default: // 3
      loadSevenSegValue(value, 2);
      // LSB
      bcd_last_value |= (((uint16_t)value) << 8);
      // make space for last value
      for(int8_t i = 9; i > 0; i--) {
        last_10_values[i] = last_10_values[i - 1];
      }
      // use GPDMA to transfer last value
      trigger_memory_transaction(&bcd_last_value);
  }

  uart_inter_count++;
}


void trigger_memory_transaction(uint16_t* value) {
  // initialize GPDMA
  GPDMA_Init();
  // set up GPDMA
  GPDMA_Channel_CFG_Type gpdma;
  gpdma.ChannelNum = 0;
  gpdma.TransferSize = 1;
  gpdma.TransferWidth = GPDMA_WIDTH_HALFWORD;
  gpdma.SrcMemAddr = (uint32_t)value;
  gpdma.DstMemAddr = (uint32_t)last_10_values;
  gpdma.TransferType = GPDMA_TRANSFERTYPE_M2M;
  gpdma.DMALLI = 0;
  GPDMA_Setup(&gpdma);
  // enable channel 0
  GPDMA_ChannelCmd(0, ENABLE);
}


void switchActiveDisplay(void) {
  switch (enabled_seven_seg) {
    case 3: // resets counter and executes case 0
      enabled_seven_seg = 0;
    case 0: // enables second display
      LPC_GPIO0->FIOCLR |= (1 << 9);
      LPC_GPIO0->FIOSET |= (1 << 8);
      LPC_GPIO0->FIOCLR |= (1 << 7);
      LPC_GPIO1->FIOCLR = (1 << 31); // enables dot
      setDisplayValue(1);
      break;
    case 1: // enables third display
      LPC_GPIO0->FIOCLR |= (1 << 9);
      LPC_GPIO0->FIOCLR |= (1 << 8);
      LPC_GPIO0->FIOSET |= (1 << 7);
      LPC_GPIO1->FIOSET = (1 << 31); // disables dot
      setDisplayValue(2);
      break;
    default: // 2: enables first display
      LPC_GPIO0->FIOSET |= (1 << 9);
      LPC_GPIO0->FIOCLR |= (1 << 8);
      LPC_GPIO0->FIOCLR |= (1 << 7);
      setDisplayValue(0);
  }

  enabled_seven_seg++;
}


void setDisplayValue(uint8_t display) {
  // handle port 0 bits
  LPC_GPIO0->FIOCLR = port_0_on_vals[display];
  LPC_GPIO0->FIOSET = port_0_off_vals[display];
  // handle port 1 bit
  LPC_GPIO1->FIOCLR = (port_1_on_vals[display] << 30);
  LPC_GPIO1->FIOSET = (port_1_off_vals[display] << 30);
}


void setLED(uint8_t value) {
  switch (value) {
    case 1:
      LPC_GPIO0->FIOSET = (1 << 1);
      LPC_GPIO0->FIOCLR = (1 << 0);
      LPC_GPIO0->FIOCLR = (1 << 6);
      break;
    case 2:
      LPC_GPIO0->FIOCLR = (1 << 1);
      LPC_GPIO0->FIOSET = (1 << 0);
      LPC_GPIO0->FIOCLR = (1 << 6);
      break;
    default:// 4
      LPC_GPIO0->FIOCLR = (1 << 1);
      LPC_GPIO0->FIOCLR = (1 << 0);
      LPC_GPIO0->FIOSET = (1 << 6);
  }
}


// segs enabled by low
void loadSevenSegValue(uint8_t value, uint8_t display) {
  switch (value) {
    case 0:
      port_0_on_vals[display] = 50823168;   // enables segs A,B,C,D,E,F
      port_0_off_vals[display] = 0;         // disables no segs
      port_1_on_vals[display] = 0;          // segment G disabled
      port_1_off_vals[display] = 1;         // segment G disabled
      break;
    case 1:
      port_0_on_vals[display] = 163840;     // enables segs B,C
      port_0_off_vals[display] = 50659328;  // disables segs A,D,E,F
      port_1_on_vals[display] = 0;          // segment G disabled
      port_1_off_vals[display] = 1;         // segment G disabled
      break;
    case 2:
      port_0_on_vals[display] = 17235968;   // enables segs A,B,D,E
      port_0_off_vals[display] = 33587200;  // disables segs C,F
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    case 3:
      port_0_on_vals[display] = 491520;     // enables segs A,B,C,D
      port_0_off_vals[display] = 50331648;  // disables segs E,F
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    case 4:
      port_0_on_vals[display] = 33718272;   // enables segs B,C,F
      port_0_off_vals[display] = 17104896;  // disables segs A,D,E
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    case 5:
      port_0_on_vals[display] = 33914880;   // enables segs A,C,D,F
      port_0_off_vals[display] = 16908288;  // disables segs B,E
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    case 6:
      port_0_on_vals[display] = 50692096;   // enables segs A,B,C,D,E
      port_0_off_vals[display] = 131072;  // disables segs F
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    case 7:
      port_0_on_vals[display] = 425984;     // enables segs A,B,C
      port_0_off_vals[display] = 50397184;  // disables segs D,E,F
      port_1_on_vals[display] = 0;          // segment G disabled
      port_1_off_vals[display] = 1;         // segment G disabled
      break;
    case 8:
      port_0_on_vals[display] = 50823168;   // enables segs A,B,C,D,E,F
      port_0_off_vals[display] = 0;         // disables no segs
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
      break;
    default: // 9
      port_0_on_vals[display] = 34045952;   // enables segs A,B,C,D,F
      port_0_off_vals[display] = 16777216;  // disables segs E
      port_1_on_vals[display] = 1;          // segment G enabled
      port_1_off_vals[display] = 0;         // segment G enabled
  }
}
