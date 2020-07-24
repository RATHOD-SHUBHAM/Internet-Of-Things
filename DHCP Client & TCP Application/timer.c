// Timer Service Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Timer 4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "timer.h"
#include "gpio.h"
#include "dhcp.h"
#include "eth0.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

#define NUM_TIMERS 10
/*
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4*/

_callback fn[NUM_TIMERS];
uint32_t period[NUM_TIMERS];
uint32_t ticks[NUM_TIMERS];
bool reload[NUM_TIMERS];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
/*
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN
            | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}
*/
void initTimer()
{
    uint8_t i;

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R4;
    _delay_cycles(3);
    // Configure Timer 4 for 1 sec tick
    TIMER4_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER4_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER4_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER4_TAILR_R = 40000000;                       // set load value (1 Hz rate)
    TIMER4_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
    TIMER4_IMR_R |= TIMER_IMR_TATOIM;                // turn-on interrupt
    NVIC_EN2_R |= 1 << (INT_TIMER4A-80);             // turn-on interrupt 86 (TIMER4A)

    for (i = 0; i < NUM_TIMERS; i++)
    {
        period[i] = 0;
        ticks[i] = 0;
        fn[i] = NULL;
        reload[i] = false;
    }
}

bool startOneshotTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while (i < NUM_TIMERS && !found)
    {
        found = fn[i] == NULL;
        if (found)
        {
            period[i] = seconds;
            ticks[i] = seconds;
            fn[i] = callback;
            reload[i] = false;
        }
        i++;
    }
    return found;
}

bool startPeriodicTimer(_callback callback, uint32_t seconds)
{
    uint8_t i = 0;
    bool found = false;
    while (i < NUM_TIMERS && !found)
    {
        found = fn[i] == NULL;
        if (found)
        {
            period[i] = seconds;
            ticks[i] = seconds;
            fn[i] = callback;
            reload[i] = true;
        }
        i++;
    }
    return found;
}

bool stopTimer(_callback callback)
{
     uint8_t i = 0;
     bool found = false;
     while (i < NUM_TIMERS && !found)
     {
         found = fn[i] == callback;
         if (found)
             ticks[i] = 0;
         i++;
     }
     return found;
}

bool restartTimer(_callback callback)
{
     uint8_t i = 0;
     bool found = false;
     while (i < NUM_TIMERS && !found)
     {
         found = fn[i] == callback;
         if (found)
             ticks[i] = period[i];
         i++;
     }
     return found;
}

void tickIsr()
{
    uint8_t i;
    for (i = 0; i < NUM_TIMERS; i++)
    {
        if (ticks[i] != 0)
        {
            ticks[i]--;
            if (ticks[i] == 0)
            {
                if (reload[i])
                    ticks[i] = period[i];
                (*fn[i])();
            }
        }
    }
    TIMER4_ICR_R = TIMER_ICR_TATOCINT;
}

// Placeholder random number function
uint32_t random32()
{
    return TIMER4_TAV_R;
}

void discoverMessage()
{
    sendDhcpMessage(data, 1);
}

void ackTimer()
{

}

/*
void main()
{
    initHw();
    initTimer();


    startOneshotTimer(flash3, 5);
    startOneshotTimer(flash4, 10);
}
*/
