// UART0 Library
// Guide : Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include <string.h>

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

char str[20];

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN
            | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DIR_R |= UART_TX_MASK;           // enable output on UART0 TX pin
    GPIO_PORTA_DIR_R &= ~UART_RX_MASK;           // enable input on UART0 RX pin
    GPIO_PORTA_DR2R_R |= UART_TX_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK; // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK; // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
    // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R = 0;                 // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                 // use system clock (40 MHz)
    UART0_IBRD_R = 21; // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
    // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate; // calculate divisor (r) in units of 1/128,
                                                      // where r = fcyc / 16 * baudRate
    UART0_IBRD_R = divisorTimes128 >> 7;        // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128 + 1)) >> 1 & 63; // set fractional value to round(fract(r)*64)
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF)
        ;               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE)
        ;               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}
// Receive character from user interface :
void getsUart0(USER_DATA *data)
{
    uint8_t i = 0;
    uint8_t count = 0;
    char c;
    c = getcUart0();
    // gets is taking input from user uart
    putcUart0(c);
    // put is like print
    if (c != 10 && c != 13 && c != 8 && c != 127 && c != 32)
    {
        data->buffer[i] = c;
        ++count;
    }
    while (count != MAX_CHARS)
    {
        c = getcUart0();
        if (count > 0)
        {
            if (c == 8 || c == 127)
            {
                count = count - 1;
                i = i - 1;
            }
            else if (c == 10 || c == 13)
            {
                i = i + 1;
                data->buffer[i] = '\0';
                ++count;
                break;
            }
            else if (c < 32)
            {
                continue;
            }
            else
            {
                i = i + 1;
                data->buffer[i] = c;
                ++count;
                putcUart0(c);
            }
        }
    }
    putsUart0("\n\rCommand Entered : ");
    lower_case(data->buffer);
    putsUart0(data->buffer);
    putsUart0("\n\r");
    return;
}

//convert uppercase string to lowercase string
void lower_case(char buffer[])
{
    int character_count = 0;
    while (buffer[character_count] != '\0')
    {
        if (buffer[character_count] >= 65 && buffer[character_count] <= 90)
        {
            buffer[character_count] = buffer[character_count] + 32;
        }
        character_count++;
    }
}

// function takes the buffer string from getsUart0() and process it
void parseFields(USER_DATA* data)
{
    uint8_t i = 0, j = 0, cval = 0;
    char prevc = 'd';
    //97 - 122 ascii letters
    //48 - 57 numbers
    //32 - 47 delimiters
    data->fieldCount = 0;
    while (data->buffer[i] != '\0' && data->fieldCount < MAX_FIELDS)
    {
        cval = data->buffer[i];
        if ((97 <= cval) && (cval <= 122) && (prevc != 'a'))
        {
            data->fieldType[j] = 'a';
            data->fieldPosition[j] = i;
            j = j + 1;
            data->fieldCount = data->fieldCount + 1;
            prevc = 'a';
        }
        else if ((48 <= cval) && (cval <= 57) && (prevc != 'n'))
        {
            data->fieldType[j] = 'n';
            data->fieldPosition[j] = i;
            j = j + 1;
            data->fieldCount = data->fieldCount + 1;
            prevc = 'n';
        }
        else if ((cval == 44 || cval == 32 || cval == 61 || cval == 46) && (prevc != 'd'))
        {
            data->fieldType[j] = 'd';
            data->fieldPosition[j] = i;
            j = j + 1;
            data->fieldCount = data->fieldCount + 1;
            prevc = 'd';
        }
        else
        {
            i = i + 1;
            continue;
        }
        i = i + 1;
    }

    for (i = 0; i < data->fieldCount; i++)
    {
        if (data->fieldType[i] == 'd')
        {
            data->buffer[data->fieldPosition[i]] = '\0';
        }
    }

}

char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    uint8_t fn = 1;
    uint8_t i = 0, j = 0, k = 0;
    if (fieldNumber <= MAX_FIELDS)
    {
        for (i = 0; i < data->fieldCount; i++)
        {
            if (fn == fieldNumber)
            {
                for (j = data->fieldPosition[i]; data->buffer[j] != '\0'; j++)
                {
                    str[k] = data->buffer[j];
                    k = k + 1;
                }
                str[k] = '\0';
                return str;
            }
            else
            {
                if (data->fieldType[i] == 'd')
                {
                    fn = fn + 1;
                }
            }
        }
    }
    return 0;
}

uint8_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    uint8_t fn = 1;
    uint8_t i = 0, j = 0, k = 0;
    if (fieldNumber <= MAX_FIELDS)
    {
        for (i = 0; i < data->fieldCount; i++)
        {
            if (fn == fieldNumber)
            {
                if (data->fieldType[i] == 'n')
                {
                    for (j = data->fieldPosition[i]; data->buffer[j] != '\0';
                            j++)
                    {
                        str[k] = data->buffer[j];
                        k = k + 1;
                    }
                    str[k] = '\0';
                    return atoi(str);
                }
                else
                {
                    return 0;
                }
            }
            else
            {
                if (data->fieldType[i] == 'd')
                {
                    fn = fn + 1;
                }
            }
        }
    }
    return 999;
}

bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    uint8_t i = 0;
    uint8_t count = 0;
    for (i = 0; data->buffer[i] != '\0'; i++)
    {
        if (strCommand[i] != data->buffer[i])
        {
            return false;
        }
    }
    for (i = 0; i < data->fieldCount; i++)
    {
        if (data->fieldType[i] == 'd')
        {
            count = count + 1;
        }
    }
    if (minArguments <= count)
    {
        return true;
    }
    return false;
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}
