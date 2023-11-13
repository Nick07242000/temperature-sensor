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

uint8_t TMR_INTER_COUNT = 0;

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

void configPRIO() // TODO: Fix prio numbers
{
    NVIC_SetPriority(UART0_IRQn, 9);
    NVIC_SetPriority(ADC_IRQn, 9);
    NVIC_SetPriority(TMR0_IRQn, 9);
}

void configPINS()
{
    PINSEL_CFG_Type cfg;

    cfg.Portnum = PINSEL_PORT_0;
    cfg.Funcnum = PINSEL_FUNC_0;
    cfg.Pinmode = PINSEL_PINMODE_PULLUP;
    cfg.OpenDrain = PINSEL_PINMODE_NORMAL;

    for (int i = 0; i < 12; i++)
    {
        cfg.Pinnum = i;
        PINSEL_ConfigPin(*cfg);
    }

    cfg.Pinnum = PINSEL_PIN_23;
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
    // handle inter count and leds/adc/uart

    TMR_INTER_COUNT++;

    if (TMR_INTER_COUNT == 64)
    {
        ADC_StartCmd(LPC_ADC, ADC_START_NOW);
    }

    if (TMR_INTER_COUNT == 128)
    {
        // send uart data
    }

    return;
}

void ADC_IRQHandler(void)
{
    uint32_t value = ADC_ChannelGetData(LPC_ADC, _ADC_CHANNEL);
    UART_Send(LPC_UART0, value, sizeof(value), NONE_BLOCKING);
}

void UART2_IRQHandler()
{
    uint32_t intsrc, tmp;

    // Inter source
    intsrc = UART_GetIntId(LPC_UART0);
    tmp = intsrc & UART_IIR_INTID_MASK;

    if (tmp == UART_IIR_INTID_RBR)
    {
        // read data
    }

    return;
}
