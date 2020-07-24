/*
 * dhcp.h
 *
 *  Created on: Mar 22, 2020
 *      Author: SHUBHAM
 */

#ifndef MQTT_H_
#define MQTT_H_
#include <stdint.h>
#include <stdbool.h>
#define MAX_PACKET_SIZE 1522

extern uint8_t data[MAX_PACKET_SIZE];
extern uint8_t data2[MAX_PACKET_SIZE];

typedef struct _dhcpstatemachine
{
    uint8_t state;
    uint32_t xid;
    uint8_t siaddr[4];
    uint8_t giadder[4];
    uint8_t yiaddr[4];
    uint8_t ciaddr[4];
    uint32_t t1;
    uint32_t t2;

}dhcpstatemachine;

extern dhcpstatemachine dhcpsm;
extern uint8_t mqttstate;
extern uint8_t tcpstate;
extern uint8_t qos;
extern uint8_t ipflag;
extern uint8_t pbtn;
extern uint8_t uartflag;
extern uint8_t udpflag;
extern uint8_t pubflag1;
extern uint8_t pubflag2;
extern uint8_t pubflag3;
extern char topic1[20];
extern char topic2[20];
extern char topic3[20];
extern char msg1[30];
extern char msg2[30];
extern char msg3[30];
extern uint8_t opflag;
extern char opcmd[15];

char topicbuffer[4][30];


bool etherIsDhcpAck(uint8_t packet[]);
bool isInput(char* str);
bool isOutput(char* str);
bool isTopic(char* str);
void sendEtherMqttPing(uint8_t packet[]);
void sendMqttPublish(uint8_t packet[], char topic[], char message[],
                     uint8_t qos);
void waitPbPress();

#endif /* MQTT_H_ */
