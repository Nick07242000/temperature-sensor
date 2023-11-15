/* libs included */
#include "lpc17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"


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


/* global variables declaration */
uint8_t tmr_inter_count = 0;
uint8_t uart_inter_count = 0;
uint8_t enabled_seven_seg = 0;

uint16_t adc_value;

// all displays 0 by default
uint32_t seven_seg_on_vals[3] = { 50823168, 1124564992, 50823168 };
// displays ordered as 1, 2, 3
uint32_t seven_seg_off_vals[3] = { 67108864, 67108864, 67108864 };


/* func definitions */
int main()
{
    configPRIO();
    configPINS();
    configADC();
    configTMR();
    configUART();

    while (1) {}
}

void configPRIO()
{
    NVIC_SetPriority(UART0_IRQn, 0);
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_SetPriority(TIMER0_IRQn, 2);
}

void configPINS()
{
    PINSEL_CFG_Type cfg;
    cfg.Portnum = PINSEL_PORT_0;
    cfg.Funcnum = PINSEL_FUNC_0;
    cfg.Pinmode = PINSEL_PINMODE_PULLUP;
    cfg.OpenDrain = PINSEL_PINMODE_NORMAL;

    uint8_t gpioPins[14] = { 0, 1, 6, 7, 8, 9, 15, 16, 17, 18, 24, 25, 26, 30 };

    for (int i = 0; i <= 14; i++)
    {
        cfg.Pinnum = gpioPins[i];
        PINSEL_ConfigPin(&cfg);
    }

    cfg.Funcnum = PINSEL_FUNC_1;

    cfg.Pinnum = PINSEL_PIN_2;
    PINSEL_ConfigPin(&cfg);
    cfg.Pinnum = PINSEL_PIN_3;
    PINSEL_ConfigPin(&cfg);

    cfg.Pinnum = PINSEL_PIN_23;
    cfg.Pinmode = PINSEL_PINMODE_TRISTATE;
    PINSEL_ConfigPin(&cfg);
}

void configADC()
{
    ADC_Init(LPC_ADC, 200000);
    ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, ENABLE);
    ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0, ENABLE);

    NVIC_EnableIRQ(ADC_IRQn);

    return;
}

void configTMR()
{
    TIM_TIMERCFG_Type tmrCfg;
    TIM_MATCHCFG_Type mchCfg;

    tmrCfg.PrescaleOption = TIM_PRESCALE_USVAL;
    tmrCfg.PrescaleValue = 0;

    mchCfg.MatchChannel = 0;
    mchCfg.IntOnMatch = ENABLE;
    mchCfg.ResetOnMatch = ENABLE;
    mchCfg.StopOnMatch = DISABLE;
    mchCfg.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    mchCfg.MatchValue = 660000;

    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &tmrCfg);
    TIM_ConfigMatch(LPC_TIM0, &mchCfg);
    TIM_Cmd(LPC_TIM0, ENABLE);

    NVIC_EnableIRQ(TIMER0_IRQn);

    return;
}

void configUART()
{
    UART_CFG_Type uartCfg;
    UART_ConfigStructInit(&uartCfg);

    UART_FIFO_CFG_Type fifoCfg;
    UART_FIFOConfigStructInit(&fifoCfg);

    UART_Init(LPC_UART2, &uartCfg);
    UART_FIFOConfig((LPC_UART_TypeDef *)LPC_UART0, &fifoCfg);
    UART_TxCmd((LPC_UART_TypeDef *)LPC_UART0, ENABLE);
    UART_IntConfig((LPC_UART_TypeDef *)LPC_UART0, UART_INTCFG_RBR, ENABLE);

    NVIC_EnableIRQ(UART0_IRQn);

    return;
}

void TIMER0_IRQHandler()
{
    TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);

    switchActiveDisplay();

    tmr_inter_count++;

    if (tmr_inter_count == 64)
    {
        ADC_StartCmd(LPC_ADC, ADC_START_NOW);
    }

    if (tmr_inter_count == 128)
    {
        uint8_t split_adc_value[2] = {(uint8_t)(adc_value), (uint8_t)(adc_value >> 8)};
        UART_Send((LPC_UART_TypeDef *)LPC_UART0, split_adc_value, 2, NONE_BLOCKING);
        tmr_inter_count = 0;
    }

    return;
}

void ADC_IRQHandler()
{
    adc_value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_0);
}

void UART0_IRQHandler()
{
    uint8_t value = UART_ReceiveByte((LPC_UART_TypeDef *)LPC_UART0);

    if (value == 255)
    {
        uart_inter_count = 0;
        return;
    }

    switch (uart_inter_count)
    {
    case 4: // resets counter and executes case 0
        uart_inter_count = 0;
    case 0:
        setLED(value);
        break;
    case 1:
        loadSevenSegValue(value, 0);
        break;
    case 2:
        loadSevenSegValue(value, 1);
        break;
    case 3:
        loadSevenSegValue(value, 2);
        break;
    default:
        break;
    }

    uart_inter_count++;
    return;
}

void switchActiveDisplay()
{
    switch (enabled_seven_seg)
    {
    case 3: // resets counter and executes case 0
        enabled_seven_seg = 0;
    case 0: // enables second display
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOSET |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 7);
        setDisplayValue(1);
        break;
    case 1: // enables third display
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOSET |= (1 << 7);
        setDisplayValue(2);
        break;
    case 2: // enables first display
        LPC_GPIO0->FIOSET |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 7);
        setDisplayValue(0);
        break;
    default:
        break;
    }

    enabled_seven_seg++;
}

void setDisplayValue(uint8_t display)
{
    LPC_GPIO0->FIOCLR |= seven_seg_on_vals[display];
    LPC_GPIO0->FIOSET |= seven_seg_off_vals[display];
}

void setLED(uint8_t value)
{
    switch (value)
    {
    case 1:
        LPC_GPIO0->FIOSET |= (1 << 11);
        LPC_GPIO0->FIOCLR |= (1 << 12);
        LPC_GPIO0->FIOCLR |= (1 << 13);
        break;
    case 2:
        LPC_GPIO0->FIOCLR |= (1 << 11);
        LPC_GPIO0->FIOSET |= (1 << 12);
        LPC_GPIO0->FIOCLR |= (1 << 13);
        break;
    case 4:
        LPC_GPIO0->FIOCLR |= (1 << 11);
        LPC_GPIO0->FIOCLR |= (1 << 12);
        LPC_GPIO0->FIOSET |= (1 << 13);
        break;
    default:
        break;
    }

    return;
}

void loadSevenSegValue(uint8_t value, uint8_t display) // segs enabled by low
{
    switch (value)
    {
    case 0:
        seven_seg_on_vals[display] = 50823168;  // enables segs A,B,C,D,E,F
        seven_seg_off_vals[display] = 67108864; // disables segs G
        break;
    case 1:
        seven_seg_on_vals[display] = 163840;     // enables segs B,C
        seven_seg_off_vals[display] = 184844288; // disables segs A,D,E,F,G
        break;
    case 2:
        seven_seg_on_vals[display] = 84344832;  // enables segs A,B,D,E,G
        seven_seg_off_vals[display] = 33554432; // disables segs F
        break;
    case 3:
        seven_seg_on_vals[display] = 67600384;  // enables segs A,B,C,D,G
        seven_seg_off_vals[display] = 50331648; // disables segs E,F
        break;
    case 4:
        seven_seg_on_vals[display] = 100827136; // enables segs B,C,F,G
        seven_seg_off_vals[display] = 17104896; // disables segs A,D,E
        break;
    case 5:
        seven_seg_on_vals[display] = 101023744; // enables segs A,C,D,F,G
        seven_seg_off_vals[display] = 16908288; // disables segs B,E
        break;
    case 6:
        seven_seg_on_vals[display] = 84377600;  // enables segs A,B,C,D,E,G
        seven_seg_off_vals[display] = 33554432; // disables segs F
        break;
    case 7:
        seven_seg_on_vals[display] = 425984;     // enables segs A,B,C
        seven_seg_off_vals[display] = 117506048; // disables segs D,E,F,G,H
        break;
    case 8:
        seven_seg_on_vals[display] = 117932032; // enables segs A,B,C,D,E,F,G
        seven_seg_off_vals[display] = 0;        // disables no segs
        break;
    case 9:
        seven_seg_on_vals[display] = 101154816; // enables segs A,B,C,D,F,G
        seven_seg_off_vals[display] = 16777216; // disables segs E
        break;
    default:
        break;
    }

    return;
}
