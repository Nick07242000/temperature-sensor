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
void setDisplayValue(uint8_t display, uint8_t value);
uint8_t getSevenSegSetValue(uint8_t value);
uint8_t getSevenSegClearValue(uint8_t value);

uint8_t TMR_INTER_COUNT = 0;
uint8_t UART_INTER_COUNT = 0;
uint8_t ENABLED_SEVEN_SEG = 0;
uint32_t ADC_VALUE;

int main()
{
    configPRIO();
    configPINS();

    LPC_GPIO0->FIOSET |= 255;      // Sets all pins to high by default
    LPC_GPIO0->FIOSET |= (1 << 8); // Enables first display

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

    for (int i = 0; i < 14; i++)
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

void ADC_IRQHandler(void)
{
    ADC_VALUE = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_0);
}

void UART2_IRQHandler()
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
        break;
    case 2:
        break;
    case 3:
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
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOSET |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 10);
        break;
    case 1: // Enables third display
        LPC_GPIO0->FIOCLR |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOSET |= (1 << 10);
        break;
    case 2: // Enables first display
        LPC_GPIO0->FIOSET |= (1 << 8);
        LPC_GPIO0->FIOCLR |= (1 << 9);
        LPC_GPIO0->FIOCLR |= (1 << 10);
        break;
    default:
        break;
    }

    ENABLED_SEVEN_SEG++;
    return
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

void setDisplayValue(uint8_t display, uint8_t value)
{
    uint8_t setVal = getSevenSegSetValue(value);
    uint8_t clrVal = getSevenSegClearValue(value);

    switch (display)
    {
    case 0:
        break;
    case 1:
        break;
    case 2:
        break;
    default:
        break;
    }
}

uint8_t getSevenSegSetValue(uint8_t value)
{
    switch (value)
    {
    case 0:
        return 0b00000011;
    case 1:
        return 0b10011111;
    case 2:
        return 0b00100101;
    case 3:
        return 0b00001101;
    case 4:
        return 0b10011001;
    case 5:
        return 0b01001001;
    case 6:
        return 0b01000001;
    case 7:
        return 0b00011111;
    case 8:
        return 0b00000001;
    case 9:
        return 0b00001001;
    default:
        return;
    }
}

uint8_t getSevenSegClearValue(uint8_t value)
{
    switch (value)
    {
    case 0:
        return 0b11111100;
    case 1:
        return 0b01100000;
    case 2:
        return 0b11011010;
    case 3:
        return 0b11110010;
    case 4:
        return 0b01100110;
    case 5:
        return 0b10110110;
    case 6:
        return 0b10111110;
    case 7:
        return 0b11100000;
    case 8:
        return 0b11111110;
    case 9:
        return 0b11110110;
    default:
        return;
    }
}