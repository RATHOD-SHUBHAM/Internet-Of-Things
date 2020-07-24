/*
 * ifttt.c
 *
 *  Created on: May 5, 2020
 *      Author: SHUBHAM
 */
#include <mqtt.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "eth0.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"

#define BLUE_LED PORTF,2

void iftttParser(USER_DATA usrdata)
{
    pubflag1 = 999;
    pubflag2 = 999;
    pubflag3 = 999;
    opflag = 999;
    uartflag = 999;
    udpflag = 999;
    pbtn = 999;
    memset(topic1, 0, sizeof(topic1));
    memset(topic2, 0, sizeof(topic2));
    memset(topic3, 0, sizeof(topic3));
    memset(msg1, 0, sizeof(msg1));
    memset(msg2, 0, sizeof(msg2));
    memset(msg3, 0, sizeof(msg3));
    memset(opcmd, 0, sizeof(opcmd));
    char cond2[15];
    char* cond = getFieldString(&usrdata, 2);
    strcpy(cond2, cond);
    if (isTopic(cond2))
    {
        pubflag1 = 1;
        //if topic then action
        //if topic=value then action
        //if topic=value and topic=value then action
        char dec[5];
        char* dec2 = getFieldString(&usrdata, 3);
        strcpy(dec, dec2);
        uint8_t value = getFieldInteger(&usrdata, 3);
        if (strcmp(dec, "then"))
        {
            //if topic then action
            char act2[15];
            char* act = getFieldString(&usrdata, 4);
            strcpy(act2, act);
            if (isTopic(act2))
            {
                //if topic then topic=value
                pubflag3 = 1;
                strcpy(topic1, cond2);
                strcpy(topic3, act2);
                strcpy(msg3, getFieldString(&usrdata, 5));
            }
            else
            {
                //if topic then output=value
                if (isOutput(act2))
                {
                    //valid output
                    opflag = 1;
                    strcpy(topic1, cond2);
                    strcpy(opcmd, act2);
                    strcpy(msg3, getFieldString(&usrdata, 5));
                }
                else
                {
                    putsUart0("\n\rInvalid output!\n\r");
                }
            }
        }
        else
        {
            char* op = getFieldString(&usrdata, 4);
            if (strcmp(op, "and") == 0)
            {
                char cmd2[10];
                char* comd2 = getFieldString(&usrdata, 5);
                strcpy(cmd2, comd2);
                if (isTopic(cmd2))
                {
                    //if topic=value and topic=value then action
                    char act2[15];
                    char* act = getFieldString(&usrdata, 8);
                    strcpy(act2, act);
                    if (isTopic(act2))
                    {
                        //if topic=value and topic=value then topic=value
                        pubflag2 = 1;
                        pubflag3 = 1;
                        strcpy(topic1, cond2);
                        strcpy(topic2, cmd2);
                        strcpy(topic3, act2);
                        strcpy(topicbuffer[0], topic1);
                        strcpy(topicbuffer[1], getFieldString(&usrdata, 3));
                        strcpy(msg1, getFieldString(&usrdata, 3));
                        strcpy(topicbuffer[2], topic2);
                        strcpy(topicbuffer[3], getFieldString(&usrdata, 6));
                        strcpy(msg2, getFieldString(&usrdata, 6));
                        strcpy(msg3, getFieldString(&usrdata, 9));

                    }
                    else
                    {
                        //if topic=value and topic=value then output=value
                        if (isOutput(act2))
                        {
                            //valid command
                            pubflag2 = 1;
                            opflag = 1;
                            strcpy(topic1, cond);
                            strcpy(topic2, comd2);
                            strcpy(opcmd, act);
                            strcpy(topicbuffer[0], topic1);
                            strcpy(topicbuffer[1], getFieldString(&usrdata, 3));
                            strcpy(topicbuffer[3], getFieldString(&usrdata, 6));
                            strcpy(topicbuffer[2], topic2);
                            strcpy(msg1, getFieldString(&usrdata, 3));
                            strcpy(msg2, getFieldString(&usrdata, 6));
                            strcpy(msg3, getFieldString(&usrdata, 9));
                        }
                        else
                        {
                            putsUart0("\n\rInvalid Output!\n\r");
                        }
                    }
                }
                else
                {
                    putsUart0("\n\rInvalid Command!\n\r");
                }
            }
            else
            {
                //if topic=value then action
                char act2[15];
                char* act = getFieldString(&usrdata, 5);
                strcpy(act2, act);
                if (isTopic(act2))
                {
                    //if topic=value then topic=value
                    pubflag3 = 1;
                    strcpy(topic1, cond);
                    strcpy(topic3, act2);
                    strcpy(msg1, getFieldString(&usrdata, 3));
                    strcpy(msg3, getFieldString(&usrdata, 6));
                }
                else
                {
                    //if topic=value then output=value
                    if (isOutput(act2))
                    {
                        //valid command
                        opflag = 1;
                        strcpy(topic1, cond);
                        strcpy(opcmd, act2);
                        strcpy(msg1, getFieldString(&usrdata, 3));
                        strcpy(msg3, getFieldString(&usrdata, 6));
                    }
                    else
                    {
                        putsUart0("\n\rInvalid Output!\n\r");
                    }
                }
            }

        }
    }
    else
    {
        char dec[5];
        char* dec2 = getFieldString(&usrdata, 3);
        strcpy(dec, dec2);
        uint8_t value = getFieldInteger(&usrdata, 3);
        if (strcmp(dec, "then") == 0)
        {
            //if input then action
            char act2[15];
            char* act = getFieldString(&usrdata, 4);
            strcpy(act2, act);
            if (isTopic(act2))
            {
                //if input then topic=value
                if (mqttstate == 1 && tcpstate == 1)
                {
                    char topic[10];
                    char msg[20];
                    strcpy(topic, getFieldString(&usrdata, 4));
                    strcpy(msg, getFieldString(&usrdata, 5));
                    sendMqttPublish(data2, topic, msg, qos);
                }
                else
                {
                    putsUart0("\n\rMQTT Client not connected!\n\r");
                }
            }
            else
            {
                //if input then output=value
                if (isOutput(act2))
                {
                    //valid command
                    if (strcmp(act2, "udp") == 0)
                    {
                        sendUdpPacket(data2, getFieldString(&usrdata, 5));
                    }
                    if (strcmp(act2, "uart") == 0)
                    {
                        putsUart0("\n\r");
                        putsUart0(getFieldString(&usrdata, 5));
                        putsUart0("\n\r");
                    }
                    if (strcmp(act2, "led") == 0)
                    {
                        uint8_t intcheck = getFieldInteger(&usrdata, 5);
                        if (intcheck == 999)
                        {
                            putsUart0("\n\rEnter 0|1 as input.\n\r");
                        }
                        if (intcheck == 1)
                        {
                            setPinValue(BLUE_LED, 1);
                            waitMicrosecond(100000);
                            setPinValue(BLUE_LED, 0);
                            waitMicrosecond(100000);
                        }
                    }
                }
                else
                {
                    putsUart0("\n\rInvalid Output!\n\r");
                }
            }
        }
        else
        {
            //if input=value then action
            //if input=value and topic=value then action
            char* op = getFieldString(&usrdata, 4);
            if (strcmp(op, "and") == 0)
            {
                char cmd2[10];
                char *cond2 = getFieldString(&usrdata, 5);
                strcpy(cmd2, cond2);
                if (isTopic(cmd2))
                {
                    //if input=value and topic=value then action
                    char act2[15];
                    char* act = getFieldString(&usrdata, 8);
                    strcpy(act2, act);
                    if (isTopic(act2))
                    {
                        //if input=value and topic=value then topic=value
                        pubflag2 = 1;
                        pubflag3 = 1;
                        strcpy(topic2, getFieldString(&usrdata, 5));
                        strcpy(topic3, getFieldString(&usrdata, 8));
                        strcpy(msg2, getFieldString(&usrdata, 6));
                        strcpy(msg3, getFieldString(&usrdata, 9));
                        uint8_t intcheck1 = getFieldInteger(&usrdata, 3);
                        if (strcmp(cmd2, "pushbutton") == 0)
                        {
                            if (intcheck1 == 999)
                            {
                                putsUart0(
                                        "\n\rEnter 0|1 as pushbutton value.\n\r");
                            }
                            else
                            {
                                pbtn = intcheck1;
                            }
                        }
                        else if (strcmp(cmd2, "uart") == 0)
                        {
                            uartflag = 1;
                            strcpy(msg1, getFieldString(&usrdata, 3));
                        }
                        else if (strcmp(cmd2, "udp") == 0)
                        {
                            udpflag = 1;
                            strcpy(msg1, getFieldString(&usrdata, 3));
                        }
                        else
                        {
                            putsUart0("\n\rInvalid Input!!\n\r");
                        }
                    }
                    else
                    {
                        //if input=value and topic=value then output=value
                        if (isOutput(act2))
                        {
                            //valid command
                            pubflag2 = 1;
                            opflag = 1;
                            strcpy(opcmd, getFieldString(&usrdata, 8));
                            strcpy(topic2, getFieldString(&usrdata, 5));
                            strcpy(msg2, getFieldString(&usrdata, 6));
                            strcpy(msg3, getFieldString(&usrdata, 9));
                            uint8_t intcheck1 = getFieldInteger(&usrdata, 3);
                            if (strcmp(cmd2, "pushbutton") == 0)
                            {
                                if (intcheck1 == 999)
                                {
                                    putsUart0(
                                            "\n\rEnter 0|1 as pushbutton value.\n\r");
                                }
                                else
                                {
                                    pbtn = intcheck1;
                                }
                            }
                            else if (strcmp(cmd2, "uart") == 0)
                            {
                                uartflag = 1;
                                strcpy(msg1, getFieldString(&usrdata, 3));
                            }
                            else if (strcmp(cmd2, "udp") == 0)
                            {
                                udpflag = 1;
                                strcpy(msg1, getFieldString(&usrdata, 3));
                            }
                            else
                            {
                                putsUart0("\n\rInvalid Input!!\n\r");
                            }
                            if (strcmp(opcmd, "led") == 0
                                    && getFieldInteger(&usrdata, 9) == 999)
                            {
                                putsUart0("\n\rEnter 0|1 as LED value!\n\r");
                            }
                        }
                        else
                        {
                            putsUart0("\n\rInvalid Output!\n\r");
                        }
                    }
                }
                else
                {
                    putsUart0("\n\rInvalid condition!\n\r");
                }
            }
            else
            {
                char act2[15];
                char* act = getFieldString(&usrdata, 5);
                strcpy(act2, act);
                if (!isOutput(act2))
                {
                    //if input=value then topic=value
                    uint8_t intcheck = getFieldInteger(&usrdata, 3);
                    pubflag3 = 1;
                    strcpy(topic3, getFieldString(&usrdata, 5));
                    strcpy(msg3, getFieldString(&usrdata, 6));
                    if (strcmp(cond2, "pushbutton") == 0)
                    {
                        if (intcheck == 999)
                        {
                            putsUart0("\n\rEnter 0|1 as pushbutton value.\n\r");
                        }
                        else
                        {
                            if (intcheck == 1)
                            {
                                waitPbPress();
                                sendMqttPublish(data2, topic3, msg3, qos);
                            }
                        }
                    }
                    else if (strcmp(cond2, "uart") == 0)
                    {
                        uartflag = 1;
                        strcpy(msg1, getFieldString(&usrdata, 3));
                    }
                    else if (strcmp(cond2, "udp") == 0)
                    {
                        udpflag = 1;
                        strcpy(msg1, getFieldString(&usrdata, 3));
                    }
                    else
                    {
                        putsUart0("\n\rInvalid Input!!\n\r");
                    }

                }
                else
                {
                    //if input=value then output=value
                    if (isOutput(act2))
                    {
                        //valid command
                        uint8_t intcheck = getFieldInteger(&usrdata, 3);
                        opflag = 1;
                        strcpy(opcmd, getFieldString(&usrdata, 5));
                        strcpy(msg3, getFieldString(&usrdata, 6));
                        if (strcmp(cond2, "pushbutton") == 0)
                        {
                            if (intcheck == 999)
                            {
                                putsUart0(
                                        "\n\rEnter 0|1 as pushbutton value.\n\r");
                            }
                            else
                            {
                                if (intcheck == 1)
                                {
                                    waitPbPress();
                                    if (strcmp(opcmd, "led") == 0)
                                    {
                                        if (strcmp(msg3, "1") == 0)
                                        {
                                            setPinValue(BLUE_LED, 1);
                                            waitMicrosecond(100000);
                                            setPinValue(BLUE_LED, 0); 
                                            waitMicrosecond(100000);
                                        }
                                    }
                                    if (strcmp(opcmd, "uart") == 0)
                                    {
                                        putsUart0(
                                                "\n\rPerforming action uart..\n\r");
                                        putsUart0(msg3);
                                        putsUart0("\n\r");
                                    }
                                    if (strcmp(opcmd, "udp") == 0)
                                    {
                                        sendUdpPacket(data2, msg3);
                                    }
                                }
                            }
                        }
                        else if (strcmp(cond2, "uart") == 0)
                        {
                            uartflag = 1;
                            strcpy(msg1, getFieldString(&usrdata, 3));
                        }
                        else if (strcmp(cond2, "udp") == 0)
                        {
                            udpflag = 1;
                            strcpy(msg1, getFieldString(&usrdata, 3));
                        }
                        else
                        {
                            putsUart0("\n\rInvalid Input!!\n\r");
                        }
                    }
                    else
                    {
                        putsUart0("\n\rInvalid Output!\n\r");
                    }
                }
            }
        }
    }

}
