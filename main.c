#include "lpc17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

void configPRIO();
void configPINS();
void configADC();
void configTMR();
void configUART();
void switchActiveDisplay();
void setLED(uint8_t value);
void setDisplayValue(uint8_t display);
uint8_t loadSevenSegValue(uint8_t value, uint8_t display);

uint8_t TMR_INTER_COUNT = 0;
uint8_t UART_INTER_COUNT = 0;
uint8_t ENABLED_SEVEN_SEG = 0;

uint32_t ADC_VALUE;

uin32_t SEVEN_SEG_ON_VALS[3] = [ 50823168, 1124564992, 50823168 ]; // all displays 0 by defauly
uin32_t SEVEN_SEG_OFF_VALS[3] = [ 67108864, 67108864, 67108864 ];  // displays ordered as 1, 2, 3

int main()
{
    configPRIO();
    configPINS();
    configADC();
    configTMR();
    configUART();

    while (1)
    {
    }
}

void configPRIO()
{
    NVIC_SetPriority(UART0_IRQn, 0);
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_SetPriority(TMR0_IRQn, 2);
}

void configPINS()
{
    PINSEL_CFG_Type cfg;
    cfg.Portnum = PINSEL_PORT_0;
    cfg.Funcnum = PINSEL_FUNC_0;
    cfg.Pinmode = PINSEL_PINMODE_PULLUP;
    cfg.OpenDrain = PINSEL_PINMODE_NORMAL;

    uint8_t gpioPins[14] = [ 0, 1, 6, 7, 8, 9, 15, 16, 17, 18, 24, 25, 26, 30 ];

    for (int i = 0; i <= 14; i++)
    {
        cfg.Pinnum = gpioPins[i];
        PINSEL_ConfigPin(*cfg);
    }

    cfg.Pinnum = PINSEL_PIN_23;
    cfg.Funcnum = PINSEL_FUNC_1;
    cfg.Pinmode = PINSEL_PINMODE_TRISTATE;
    PINSEL_ConfigPin(*cfg);
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
    UART_FIFOConfig(LPC_UART0, &fifoCfg);
    UART_TxCmd(LPC_UART0, ENABLE);
    UART_IntConfig(LPC_UART0, UART_INTCFG_RBR, ENABLE);

    return;
}

void TIMER0_IRQHandler()
{
    TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);

    switchActiveDisplay();

    TMR_INTER_COUNT++;

    if (TMR_INTER_COUNT == 64)
    {
        ADC_StartCmd(LPC_ADC, ADC_START_NOW);
    }

    if (TMR_INTER_COUNT == 128)
    {
        UART_Send(LPC_UART0, ADC_VALUE, sizeof(ADC_VALUE), NONE_BLOCKING);
        TMR_INTER_COUNT = 0;
    }

    return;
}

void ADC_IRQHandler()
{
    ADC_VALUE = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_0);
}

void UART0_IRQHandler()
{
    uint8_t value = UART_ReceiveByte(LPC_UART0);

    if (value == 255)
    {
        UART_INTER_COUNT = 0;
        return;
    }

    switch (UART_INTER_COUNT)
    {
    case 4: // Resets counter and executes case 0
        UART_INTER_COUNT = 0;
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

    UART_INTER_COUNT++;
    return;
}

void switchActiveDisplay()
{
    switch (ENABLED_SEVEN_SEG)
    {
    case 3: // Resets counter and executes case 0
        ENABLED_SEVEN_SEG = 0;
    case 0: // Enables second display
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOSET |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 7);
        setDisplayValue(2);
        break;
    case 1: // Enables third display
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOSET |= (1 << 7);
        setDisplayValue(3);
        break;
    case 2: // Enables first display
        LPC_GPIO0->FIOSET |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 7);
        setDisplayValue(1);
        break;
    default:
        break;
    }

    ENABLED_SEVEN_SEG++;
    return
}

void setDisplayValue(uint8_t display)
{
    LPC_GPIO0->FIOCLR |= SEVEN_SEG_ON_VALS[display];
    LPC_GPIO0->FIOSET |= SEVEN_SEG_OFF_VALS[display];
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

uint8_t loadSevenSegValue(uint8_t value, uint8_t display) // Segments enabled by low
{
    switch (value)
    {
    case 0:
        SEVEN_SEG_ON_VALS[display] = 50823168;  // Enables segments A,B,C,D,E,F
        SEVEN_SEG_OFF_VALS[display] = 67108864; // Disables segments G
        break;
    case 1:
        SEVEN_SEG_ON_VALS[display] = 163840;     // Enables segments B,C
        SEVEN_SEG_OFF_VALS[display] = 184844288; // Disables segments A,D,E,F,G
        break;
    case 2:
        SEVEN_SEG_ON_VALS[display] = 84344832;  // Enables segments A,B,D,E,G
        SEVEN_SEG_OFF_VALS[display] = 33554432; // Disables segments F
        break;
    case 3:
        SEVEN_SEG_ON_VALS[display] = 67600384;  // Enables segments A,B,C,D,G
        SEVEN_SEG_OFF_VALS[display] = 50331648; // Disables segments E,F
        break;
    case 4:
        SEVEN_SEG_ON_VALS[display] = 100827136; // Enables segments B,C,F,G
        SEVEN_SEG_OFF_VALS[display] = 17104896; // Disables segments A,D,E
        break;
    case 5:
        SEVEN_SEG_ON_VALS[display] = 101023744; // Enables segments A,C,D,F,G
        SEVEN_SEG_OFF_VALS[display] = 16908288; // Disables segments B,E
        break;
    case 6:
        SEVEN_SEG_ON_VALS[display] = 84377600;  // Enables segments A,B,C,D,E,G
        SEVEN_SEG_OFF_VALS[display] = 33554432; // Disables segments F
        break;
    case 7:
        SEVEN_SEG_ON_VALS[display] = 425984;     // Enables segments A,B,C
        SEVEN_SEG_OFF_VALS[display] = 117506048; // Disables segments D,E,F,G,H
        break;
    case 8:
        SEVEN_SEG_ON_VALS[display] = 117932032; // Enables segments A,B,C,D,E,F,G
        SEVEN_SEG_OFF_VALS[display] = 0;        // Disables no segments
        break;
    case 9:
        SEVEN_SEG_ON_VALS[display] = 101154816; // Enables segments A,B,C,D,F,G
        SEVEN_SEG_OFF_VALS[display] = 16777216; // Disables segments E
        break;
    default:
        break;
    }

    return;
}
