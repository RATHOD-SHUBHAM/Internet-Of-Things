/*
 * dhcp.c
 *
 *  Created on: Mar 22, 2020
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
#include "timer.h"
#include "ifttt.h"

#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define INIT 1
#define SELECTING 2
#define REQUESTING 3
#define BOUND 4
#define RENEWING 5
#define REBIND 6

#define SYN 0x02;
#define ACK 0x10;
#define PSH 0x08;
#define FIN 0x01;

#define MAX_PACKET_SIZE 1522

uint8_t data[MAX_PACKET_SIZE];
uint8_t data2[MAX_PACKET_SIZE];

uint32_t runningseq = 0;    // this is to maintain sequence number 
uint32_t expack = 0;

uint16_t sourceport = 0;
uint16_t destport = 1883;   // this is made global,, port no 1833 is for mqtt

uint8_t mqttstate = 0;
uint8_t tcpstate = 0;   // conmection if connection is established

uint16_t subid[5];
char subtopics[5][10];  // subtopic index goes to subid , 

uint8_t subcount = 0;
uint16_t pubcount = 2342;   

uint8_t qos = 0;

char input1[10] = "pushbutton"; //supported input 
char input2[4] = "uart";
char input3[3] = "udp";

char output1[3] = "led";    // supported output 
char output2[4] = "uart";
char output3[3] = "udp";

uint8_t ipflag = 999;    //given 999 so that it will be easy in iftp.
uint8_t pbtn = 999;
uint8_t uartflag = 999;
uint8_t udpflag = 999;      //so that we know that it is waiting
uint8_t pubflag1 = 999;
uint8_t pubflag2 = 999;
uint8_t pubflag3 = 999;
char topic1[20];
char topic2[20];
char topic3[20];
char msg1[30];
char msg2[30];
char msg3[30];
uint8_t opflag = 999;
char opcmd[15];

char topicbuffer[4][30];

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

void initEeprom()
{
    SYSCTL_RCGCEEPROM_R = 1;
    _delay_cycles(3);
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING)
        ;
}

void writeEeprom(uint16_t add, uint32_t data)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    EEPROM_EERDWR_R = data;
    while (EEPROM_EEDONE_R & EEPROM_EEDONE_WORKING)
        ;
}

uint32_t readEeprom(uint16_t add)
{
    EEPROM_EEBLOCK_R = add >> 4;
    EEPROM_EEOFFSET_R = add & 0xF;
    return EEPROM_EERDWR_R;
}

void waitPbPress()
{
    while (getPinValue(PUSH_BUTTON))
        ;
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("HW: ");
    for (i = 0; i < 6; i++)
    {
        sprintf(str, "%02x", mac[i]);
        putsUart0(str);
        if (i < 6 - 1)
            putcUart0(':');
    }
    putcUart0('\n');
    etherGetIpAddress(ip);
    putsUart0("\rIP: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\n');
    etherGetIpSubnetMask(ip);
    putsUart0("\rSN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    etherGetIpGatewayAddress(ip);
    putsUart0("\rGW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (etherIsLinkUp())
        putsUart0("\rLink is up\n\r");
    else
        putsUart0("\rLink is down\n\r");
}

void updateAck(uint8_t packet[])
{
    uint16_t tcpPayloadSize;
    uint16_t tcplength;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    tcplength = htons(ip->length) - 20;
    tcpPayloadSize = tcplength - (tcp->off * 4);
    if (tcpPayloadSize == 0)
    {
        expack = expack + 1;
    }
    else
    {
        expack = expack + tcpPayloadSize;
    }

}

uint16_t searchDhcpOptions(uint8_t packet[], uint8_t val)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    uint16_t i;
    for (i = 0; i < 312; i = i + dhcp->options[i + 1] + 2)
    {
        if (dhcp->options[i] == val)
        {
            return i;
        }
    }
    return 450;
}

bool etherIsTcp(uint8_t packet[])   //  if the packet is tcp or not
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));    // what is this
    bool ok;
    ok = (ip->protocol == 6);
    /*if (ok)
     {
     sum = 0;
     etherSumWords(ip->sourceIp, 8);
     tmp16 = ip->protocol;
     sum += (tmp16 & 0xff) << 8;
     etherSumWords(&tcp->length, 2);
     // add tcp header and data
     etherSumWords(tcp, tcp->length);
     ok = (getEtherChecksum() == 0);
     }*/
    return ok;
}

bool etherIsTcpAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (tcp->flags == 0x010)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsTcpSyn(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (tcp->flags == 0b00000010)
    {
        return true;
    }
    else
    {
        return false;
    }

}

bool etherIsTcpSynAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (tcp->flags == 0x012)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsTcpFinAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (tcp->flags == 0x011)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsTcpPshAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (tcp->flags == 0x018)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool isTopic(char str[])    // this will be used in iftt.
//it will check if it is a valid topic for which we are subscribing.
//so it checks in a array
{
    uint8_t i;
    for (i = 0; i < subcount; i++)
    {
        if (strcmp(str, subtopics[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isOutput(char str[])
{
    if (strcmp(str, output2) == 0)
    {
        return true;
    }
    else if (strcmp(str, output2) == 0)
    {
        return true;
    }
    else if (strcmp(str, output3) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool isInput(char str[])
{
    if (strcmp(str, input1) == 0)
    {
        return true;
    }
    else if (strcmp(str, input2) == 0)
    {
        return true;
    }
    else if (strcmp(str, input3) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void sendEtherTcpMessage(uint8_t packet[], uint16_t srcp, uint16_t destp,
                         uint8_t srcip[4], uint8_t destip[4], uint8_t srcmac[6],
                         uint8_t destmac[6], uint8_t flag)
// this is our tcp message
{
    uint8_t tmpip[4];
    uint8_t tmpmac[6];
    uint16_t tmp16;
    uint16_t tmpp;
    uint16_t tcplength;
    uint16_t tcpPayloadSize;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    if (flag == 0x002 || flag == 0x011)
    {
        ip->revSize = 0x45;
    }
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    if (flag == 0x010) //TCP ACK
    {
        tcplength = htons(ip->length) - 20;
        tcpPayloadSize = tcplength - (tcp->off * 4);
        if (tcpPayloadSize == 0)
        {
            expack = htonl(tcp->seq) + 1;
        }
        else
        {
            expack = htonl(tcp->seq) + tcpPayloadSize;
        }
        //updateAck(data);
        tcp->seq = htonl(runningseq);
        tcp->flags = 0x010;
        tmpp = tcp->destp;
        tcp->destp = tcp->srcp;
        tcp->srcp = tmpp;
        memcpy(&tmpip, &ip->sourceIp, 4);
        memcpy(&ip->sourceIp, &ip->destIp, 4);
        memcpy(&ip->destIp, &tmpip, 4);
        memcpy(&tmpmac, &ether->sourceAddress, 6);
        memcpy(&ether->sourceAddress, &ether->destAddress, 6);
        memcpy(&ether->destAddress, &tmpmac, 6);
    }
    if (flag == 0x002 || flag == 0x011)
    {
        tcp->srcp = htons(srcp);
        tcp->destp = htons(destp);
        tcp->flags = flag;
        tcp->seq = htonl(runningseq++);
    }
    tcp->ack = htonl(expack);
    tcp->checksum = 0;
    tcp->off = 5;
    tcp->res = 0;
    tcp->winsize = htons(64240);
    tcp->urgptr = 0;
    if (flag == 0x002 || flag == 0x011)
    {
        memcpy(ip->sourceIp, srcip, 4);
        memcpy(ip->destIp, destip, 4);
        memcpy(ether->sourceAddress, macAddress, 6);
        memcpy(ether->destAddress, destmac, 6);
        ether->frameType = htons(0x0800);
        ip->protocol = 6;
        ip->ttl = 64;
        ip->flagsAndOffset = htons(0);
        ip->typeOfService = 0x00;
    }
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20;
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));

}

void sendMqttUnsubscribe(uint8_t packet[], uint16_t subid, char* topic,
                         uint8_t qos)
{
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 }; // mac address
    uint16_t tcplength;
    uint16_t tmp16;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttUnsubscribeFrame* mqttunsub = (mqttUnsubscribeFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    mqttunsub->hdrflag = 0b10100010;
    mqttunsub->msgid = htons(subid);
    mqttunsub->topicnamelen = htons(strlen(topic));
    memcpy(&mqttunsub->topicname, topic, strlen(topic));
    mqttunsub->msglen = strlen(topic) + 4;
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqttunsub->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqttunsub->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqttunsub->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));
}

void sendMqttDisconnect(uint8_t packet[], uint8_t qos)
{
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };     //dest mac
    uint16_t tcplength;
    uint16_t tmp16;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttDisconnectFrame* mqttdis = (mqttDisconnectFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    mqttdis->hdrflag = 0xe0;
    mqttdis->msglen = 0;
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqttdis->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqttdis->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqttdis->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));
}

void sendMqttSubscribe(uint8_t packet[], char topic[], uint8_t qos)
{
    //char topic[4] = "test";
    uint8_t qs = 0x00;
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint16_t tcplength;
    uint16_t tmp16;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttSubscribeFrame* mqttsub = (mqttSubscribeFrame*) &tcp->data;
    if (qos == 0)
    {
        qs = 0x00;
    }
    else if (qos == 1)
    {
        qs = 0x01;
    }
    else
    {
        qs = 0x02;
    }
    memcpy(dstip, ipGwAddress, 4);
    mqttsub->hdrflag = 0x82;
    mqttsub->topicnamelen = htons(strlen(topic));
    mqttsub->msgid = htons(subcount);
    memcpy(&mqttsub->topicname, topic, strlen(topic));
    memcpy(&mqttsub->topicname + strlen(topic), &qs, 1);
    mqttsub->msglen = strlen(topic) + 5;
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqttsub->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqttsub->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800); //to flip 16 bit 
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqttsub->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));
}

void sendMqttPubAck(uint8_t packet[], uint16_t msgid)
{
    uint8_t dstip[4];
    uint8_t tmpip[4];
    uint8_t tmpmac[6];
    uint16_t tmpp;
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint16_t tcplength;
    uint16_t tmp16;
    uint16_t tcpPayloadSize;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttPubAckFrame* mqttpub = (mqttPubAckFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    mqttpub->hdrflag = 0x40;
    mqttpub->msglen = 2;
    mqttpub->msgidentifier = htons(msgid);
    tcplength = htons(ip->length) - 20;
    tcpPayloadSize = tcplength - (tcp->off * 4);
    if (tcpPayloadSize == 0)
    {
        expack = htonl(tcp->seq) + 1;
    }
    else
    {
        expack = htonl(tcp->seq) + tcpPayloadSize;
    }
    tcp->seq = htonl(runningseq);
    tcp->flags = 0x010;
    tmpp = tcp->destp;
    tcp->destp = tcp->srcp;
    tcp->srcp = tmpp;
    memcpy(&tmpip, &ip->sourceIp, 4);
    memcpy(&ip->sourceIp, &ip->destIp, 4);
    memcpy(&ip->destIp, &tmpip, 4);
    memcpy(&tmpmac, &ether->sourceAddress, 6);
    memcpy(&ether->sourceAddress, &ether->destAddress, 6);
    memcpy(&ether->destAddress, &tmpmac, 6);
    tcp->ack = htonl(expack);   // to flip 32 bit data
    tcp->checksum = 0;
    tcp->off = 5;
    tcp->res = 0;
    tcp->winsize = htons(64240);
    tcp->urgptr = 0;
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20;
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));
}

void sendMqttPublish(uint8_t packet[], char topic[], char message[],
                     uint8_t qos)
{
    //char message[26] = "hello world from red board";
    //char topic[4] = "test";
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint16_t tcplength;
    uint16_t tmp16;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttPublishFrame* mqttpub = (mqttPublishFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    if (qos == 0)
    {
        mqttpub->hdrflag = 0b00110000;
    }
    else if (qos == 1)
    {
        mqttpub->hdrflag = 0b00110010;
    }
    else
    {
        mqttpub->hdrflag = 0b00110100;
    }
    mqttpub->topicnamelen = htons(strlen(topic));
    memcpy(&mqttpub->topicname, topic, strlen(topic));
    memcpy(&mqttpub->topicname + strlen(topic) + 2, message, strlen(message));
    if (qos == 1 || qos == 2)
    {
        memcpy(&mqttpub->topicname + strlen(topic), &pubcount, 2);
        pubcount = pubcount + 1;
    }
    if (qos == 1 || qos == 2)
    {
        mqttpub->msglen = strlen(topic) + strlen(message) + 4;
    }
    else
    {
        mqttpub->msglen = strlen(topic) + strlen(message) + 2;
    }
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqttpub->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqttpub->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqttpub->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));
}

void sendMqttConnect(uint8_t packet[], uint8_t qos)
{
    char prname[4] = "MQTT";
    char clientid[9] = "Red-Board";
    uint16_t tmp16;
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint16_t tcplength;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttFrame* mqtt = (mqttFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    mqtt->hdrflag = 0x10;
    mqtt->protocolnamelen = htons(4);
    memcpy(mqtt->protocolname, prname, 4);
    mqtt->version = 0x04;
    mqtt->connflag = 0b00000010;
    mqtt->keepalive = htons(60);
    memcpy(&mqtt->clid, &clientid, sizeof(clientid));
    mqtt->clidlen = htons(sizeof(clientid));
    mqtt->msglen = 21;
    //tcp->data = "Hello World";
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqtt->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqtt->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqtt->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));

}

void sendEtherMqttPing(uint8_t packet[])
{
    uint16_t tmp16;
    uint8_t dstip[4];
    uint8_t dstmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint16_t tcplength;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    ip->revSize = 0x45;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    mqttPingFrame* mqttping = (mqttPingFrame*) &tcp->data;
    memcpy(dstip, ipGwAddress, 4);
    mqttping->hdrflag = 0b11000000;
    mqttping->msglen = 0;
    tcp->srcp = htons(sourceport);
    tcp->destp = htons(destport);
    tcp->seq = htonl(runningseq);
    runningseq = runningseq + 2 + mqttping->msglen;
    tcp->flags = 0x018;
    tcp->checksum = 0;
    tcp->winsize = htons(512);
    tcp->ack = htonl(expack);
    tcp->off = 5;
    tcp->res = 0;
    tcp->urgptr = 0;
    memcpy(ip->sourceIp, ipAddress, 4);
    memcpy(ip->destIp, dstip, 4);
    ip->headerChecksum = 0;
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20 + mqttping->msglen + 2);
    ip->flagsAndOffset = htons(0);
    ip->ttl = 128;
    ip->typeOfService = 0x00;
    ip->protocol = 6;
    memcpy(ether->sourceAddress, macAddress, 6);
    memcpy(ether->destAddress, dstmac, 6);
    ether->frameType = htons(0x0800);
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20 + mqttping->msglen + 2;
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, htons(tcplength));
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + htons(tcplength));

}

void sendEtherTcpSynAck(uint8_t packet[])
{
    uint16_t tmp16;
    uint8_t tmpip[4];
    uint8_t tmpmac[6];
    uint16_t tcplength;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);
    tmp16 = tcp->destp;
    tcp->destp = tcp->srcp;
    tcp->srcp = tmp16;
    tcp->ack = htonl(htonl(tcp->seq) + 1);
    tcp->seq = htonl(runningseq);
    tcp->checksum = 0;
    tcp->off = 5;
    tcp->res = 0;
    tcp->flags = 0b00010010;
    tcp->winsize = htons(1280);
    tcp->urgptr = 0;
    memcpy(&tmpip, &ip->sourceIp, 4);
    memcpy(&ip->sourceIp, &ip->destIp, 4);
    memcpy(&ip->destIp, &tmpip, 4);
    ip->headerChecksum = 0;
    memcpy(&tmpmac, &ether->sourceAddress, 6);
    memcpy(&ether->sourceAddress, &ether->destAddress, 6);
    memcpy(&ether->destAddress, &tmpmac, 6);
    ip->length = htons(((ip->revSize & 0xF) * 4) + 20);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcplength = 20;
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    tcplength = htons(tcplength);
    etherSumWords(&tcplength, 2);
    etherSumWords(tcp, 20);
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 20);

}

bool etherIsDhcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    if (ip->protocol == 17 && htons(udp->destPort) == 68
            && dhcpsm.xid == dhcp->xid)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsDhcpOffer(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    if (dhcp->options[2] == 2)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsDhcpAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    if (dhcp->options[2] == 5)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqtt(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    if (htons(tcp->srcp) == 1883)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttConnectAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttFrame* mqtt = (mqttFrame*) &tcp->data;
    if (mqtt->hdrflag == 0x20)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttSubscribeAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttFrame* mqtt = (mqttFrame*) &tcp->data;
    if (mqtt->hdrflag == 0x90)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttPublish(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttFrame* mqtt = (mqttFrame*) &tcp->data;
    if (mqtt->hdrflag == 0x30)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttUnsubscribeAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttUnsubscribeFrame* mqtt = (mqttUnsubscribeFrame*) &tcp->data;
    if (mqtt->hdrflag == 0b10110000)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttPubAck(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttUnsubscribeFrame* mqtt = (mqttUnsubscribeFrame*) &tcp->data;
    if (mqtt->hdrflag == 0b01000000)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool etherIsMqttPing(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttPingFrame* mqtt = (mqttPingFrame*) &tcp->data;
    if (mqtt->hdrflag == 0b11010000)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void setDhcpAddresses(uint8_t packet[], uint8_t type)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    uint16_t siaddrval = searchDhcpOptions(packet, 54);
    uint16_t giaddrval = searchDhcpOptions(packet, 3);
    if (siaddrval != 450 && giaddrval != 450)
    {
        memcpy(&dhcpsm.siaddr, &dhcp->options[siaddrval + 2], 4);
        memcpy(&dhcpsm.giadder, &dhcp->options[giaddrval + 2], 4);
        memcpy(&dhcpsm.yiaddr, &dhcp->yiaddr, 4);
        if (type == 5)
        {
            memcpy(&dhcpsm.ciaddr, &dhcp->yiaddr, 4);
            etherSetIpAddress(dhcp->yiaddr[0], dhcp->yiaddr[1], dhcp->yiaddr[2],
                              dhcp->yiaddr[3]);
        }
    }
    else
    {
        //Error in recieving
        setPinValue(RED_LED, 1);
        waitMicrosecond(1000000);
        setPinValue(RED_LED, 0);
        waitMicrosecond(100000);
    }
}

void etherSetDhcpTimers(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    uint16_t t1ind = searchDhcpOptions(packet, 58);
    dhcpsm.t1 = (dhcp->options[t1ind + dhcp->options[t1ind + 1] + 1]) / 2;
    dhcpsm.t2 = 7 * (dhcp->options[t1ind + dhcp->options[t1ind + 1] + 1]) / 8;
}

// our struct  will have topic name
// topic name length,  topice name etc 
// we ddnt creat seperate thing for message. we can created a empt pointer.
// when compiler compiles, then we see sequentialy. as we write offset.
// so that we will know from where to start

void printPublishMessage(uint8_t packet[])
{
    uint8_t msglen;
    char msg[20];
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttPublishFrame* mqtt = (mqttPublishFrame*) &tcp->data;
    msglen = mqtt->msglen - htons(mqtt->topicnamelen) - 2;
    memcpy(msg, (char *) (&mqtt->topicname + htons(mqtt->topicnamelen)),    //offset calculation
           msglen);
    msg[msglen] = '\0';
    putsUart0("\n\rMessage recieved :-\n\r");
    putsUart0(msg);
    putsUart0("\n\r");
}

void getPublishTopic(uint8_t packet[], char* topic)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttPublishFrame* mqtt = (mqttPublishFrame*) &tcp->data;
    memcpy(topic, (char *) &mqtt->topicname, htons(mqtt->topicnamelen)); // skip topic name and its length 
    // then use the data
    topic[htons(mqtt->topicnamelen)] = '\0';
}

void getPublishMessage(uint8_t packet[], char* msg)
{
    uint8_t msglen;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttPublishFrame* mqtt = (mqttPublishFrame*) &tcp->data;
    msglen = mqtt->msglen - htons(mqtt->topicnamelen) - 2;
    memcpy(msg, (char *) (&mqtt->topicname + htons(mqtt->topicnamelen)),
           msglen);
    msg[msglen] = '\0';
}

uint16_t getPublishMessageId(uint8_t packet[])
{
    uint16_t retit;
    char* msg;
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    mqttPublishFrame* mqtt = (mqttPublishFrame*) &tcp->data;
    memcpy(msg, (char *) (&mqtt->topicname + htons(mqtt->topicnamelen)), 2);
    msg[2] = '\0';
    retit = atoi(msg);
    return retit;
}

void processPublish(uint8_t packet[])
{
    char* rtopic1;
    char* rmsg1;
    if ((pubflag1 == 1 && msg1[0] == '\0') && pubflag3 == 1)
    {
        //check topic then publish topic3 with msg3
        getPublishTopic(packet, rtopic1);
        if (strcmp(rtopic1, topic1) == 0)
        {
            sendMqttPublish(data2, topic3, msg3, qos);
        }
    }
    if ((pubflag1 == 1 && msg1[0] == '\0') && opflag == 1)
    {
        //check topic then output opcmd with msg3
        getPublishTopic(packet, rtopic1);
        if (strcmp(rtopic1, topic1) == 0)
        {
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
                putsUart0("\n\rPerforming Action Uart..\n\r");
                putsUart0(msg3);
                putsUart0("\n\r");
            }
            if (strcmp(opcmd, "udp") == 0)
            {
                sendUdpPacket(data2, msg3);
            }
        }
    }
    if ((pubflag1 == 1 && msg1[0] != '\0' && pubflag3 == 1))
    {
        //check topic1 and msg1 then publish topic3 with msg3
        getPublishTopic(packet, rtopic1);
        getPublishMessage(packet, rmsg1);
        if (strcmp(topic1, rtopic1) == 0 && strcmp(msg1, rmsg1) == 0)
        {
            sendMqttPublish(data2, topic3, msg3, qos);
        }
    }
    if ((pubflag1 == 1 && msg1[0] != '\0') && opflag == 1)
    {
        //check topic1 and msg1 then output opcmd with msg3
        getPublishTopic(packet, rtopic1);
        getPublishMessage(packet, rmsg1);
        if (strcmp(topic1, rtopic1) == 0 && strcmp(msg1, rmsg1) == 0)
        {
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
                putsUart0("\n\rPerforming Action Uart..\n\r");
                putsUart0(msg3);
                putsUart0("\n\r");
            }
            if (strcmp(opcmd, "udp") == 0)
            {
                sendUdpPacket(data2, msg3);
            }
        }
    }
    if ((pubflag1 == 1 && msg1[0] != '\0') && (pubflag2 == 1 && msg2[0] != '\0')
            && pubflag3 == 1)
    {
        //check topic1 and msg1 and topic2 and msg2 then publish topic3 with msg3
    }
    if ((pubflag1 == 1 && msg1[0] != '\0') && (pubflag2 == 1 && msg2[0] != '\0')
            && opflag == 1)
    {
        //check topic1 and msg1 and topic2 and msg2 then output opcmd with msg3
    }
    if ((pbtn == 1 || uartflag == 1 || udpflag == 1)
            && (pubflag2 == 1 && msg2[0] != '\0') && pubflag3 == 1)
    {
        //check input and topic2 and msg2 then publish topic3 with msg3
    }
    if ((pbtn == 1 || uartflag == 1 || udpflag == 1)
            && (pubflag2 == 1 && msg2[0] != '\0') && opflag == 1)
    {
        //check input and topic2 and msg2 then output opcmd with msg3
    }
}

uint8_t isTopicPresent(char* topic)
{
    uint8_t i;
    for (i = 0; i < 5; i++)
    {
        if (strcmp(topic, subtopics[i]) == 0)
        {
            return i;
        }
    }
    return 99;
}

void main()

{
    uint8_t destmac[6] = { 0x6c, 0xc2, 0x17, 0x78, 0xe5, 0x17 };
    uint8_t* udpData;
    //uint8_t arp[MAX_PACKET_SIZE];
    USER_DATA usrdata;
    uint32_t ip = 0x728A8304;
    uint32_t rip;
    // uint32_t num[4] = {0,0,0,0};
    //char* token;
    //uint8_t v[6];
    //uint8_t cnt = 0;
    // Init controller
    initHw();
    initTimer();
    initEeprom();

    if (readEeprom(0x0) != 0xFFFFFFFF)
        writeEeprom(0x0, 0xFFFFFFFF);
    writeEeprom(0x1, ip);
    rip = readEeprom(0x1);

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("Starting eth0\n\n\r");
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    etherSetMacAddress(2, 3, 4, 5, 6, 132);
    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 1, 132);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    waitMicrosecond(100000);
    displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);
    //dhcpsm.state = INIT;
    //startPeriodicTimer(discoverMessage, 15);
    //sendDhcpMessage(data, 1);
    //sourceport = 48777;
    //sendEtherTcpMessage(data, sourceport, 1883, ipAddress, ipGwAddress,
    //                  macAddress, destmac, 0x002);
    while (true)
    {
        if (kbhitUart0())
        {
            getsUart0(&usrdata);
            parseFields(&usrdata);
            if (isCommand(&usrdata, "ifconfig", 0))
            {
                displayConnectionInfo();
            }
            if (isCommand(&usrdata, "set", 2))
            {
                char* str = getFieldString(&usrdata, 2);
                if (strcmp(str, "mqtt") == 0)
                {
                    uint8_t i;
                    for (i = 0; i < 4; i++)
                    {
                        ipGwAddress[i] = getFieldInteger(&usrdata, (3 + i));
                    }
                    displayConnectionInfo();
                }
                if (strcmp(str, "qos") == 0)
                {
                    uint8_t tmp;
                    tmp = getFieldInteger(&usrdata, 3);
                    if (tmp == 0 || tmp == 1)
                    {
                        qos = tmp;
                    }
                    else
                    {
                        putsUart0(
                                "\n\rInvalid QOS Number! Please enter 0, 1 or 2.\n\r");
                    }
                }
            }
            if (isCommand(&usrdata, "if", 4))
            {
                iftttParser(usrdata);
            }
            if ((isCommand(&usrdata, msg1, 0) == true) && uartflag == 1)
            {
                if (opflag == 1)
                {
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
                        putsUart0("\n\rPerforming action uart..\n\r");
                        putsUart0(msg3);
                        putsUart0("\n\r");
                    }
                    if (strcmp(opcmd, "udp") == 0)
                    {
                        sendUdpPacket(data2, msg3);
                    }
                }
                if (pubflag3 == 1)
                    sendMqttPublish(data2, topic3, msg3, qos);
            }
            if (isCommand(&usrdata, "connect", 0))
            {
                sourceport = 45702;
                runningseq = 0;
                expack = 0;
                sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                    ipGwAddress, macAddress, destmac, 0x002);
            }
            if (isCommand(&usrdata, "publish", 2))
            {
                char* topic;
                char topic2[20];
                char* msg;
                char msg2[20];
                topic = getFieldString(&usrdata, 2);
                strcpy(topic2, topic);
                msg = getFieldString(&usrdata, 3);
                strcpy(msg2, msg);
                if (mqttstate == 1 && tcpstate == 1)
                {
                    sendMqttPublish(data2, topic2, msg2, qos);
                }
            }
            if (isCommand(&usrdata, "subscribe", 1))
            {
                if (mqttstate == 1 && tcpstate == 1)
                {
                    char* topic;
                    topic = getFieldString(&usrdata, 2);
                    strcpy(subtopics[subcount], topic);
                    subid[subcount] = subcount + 1;
                    subcount = subcount + 1;
                    sendMqttSubscribe(data2, topic, qos);
                }
            }
            if (isCommand(&usrdata, "unsubscribe", 1))
            {
                char* topic;
                uint8_t off = 0;
                topic = getFieldString(&usrdata, 2);
                off = isTopicPresent(topic);
                if (off != 99)
                {
                    if (mqttstate == 1 && tcpstate == 1)
                    {
                        sendMqttUnsubscribe(data2, subid[off], subtopics[off],
                                            qos);
                    }
                }
                else
                {
                    putsUart0("\n\rTopic not subscribed\n\r");
                }
            }
            if (isCommand(&usrdata, "disconnect", 0))
            {
                if (mqttstate == 1 && tcpstate == 1)
                {
                    sendMqttDisconnect(data2, qos);
                }
            }
            if (isCommand(&usrdata, "help", 1))
            {
                if (strcmp(getFieldString(&usrdata, 2), "subs") == 0)
                {
                    uint8_t i;
                    putsUart0("\n\rSubscribed Topics :-\n\r");
                    for (i = 0; i < subcount; i++)
                    {
                        putsUart0(subtopics[i]);
                        putsUart0("\n\r");
                    }
                }
                if (strcmp(getFieldString(&usrdata, 2), "inputs") == 0)
                {
                    putsUart0("\n\rPushbutton\n\r");
                    putsUart0("UART\n\r");
                    putsUart0("UDP Port\n\r");
                }
                if (strcmp(getFieldString(&usrdata, 2), "outputs") == 0)
                {
                    putsUart0("\n\rLED\n\r");
                    putsUart0("UART\n\r");
                    putsUart0("UDP Port\n\r");
                }
            }
        }

        if (etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            etherGetPacket(data, MAX_PACKET_SIZE);
            if (etherIsArpRequest(data))
            {
                etherSendArpResponse(data);
            }
            if (etherIsIp(data))
            {
                if (etherIsIpUnicast(data))
                {
                    // handle icmp ping request
                    if (etherIsPingRequest(data))
                    {
                        etherSendPingResponse(data);
                    }
                    if (etherIsUdp(data))
                    {
                        udpData = etherGetUdpData(data);
                        if (strcmp((char*) udpData, msg1) == 0 && udpflag == 1)
                            if (opflag == 1)
                            {
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
                        if (pubflag3 == 1)
                            sendMqttPublish(data2, topic3, msg3, qos);
                    }
                }

            }
            /*
             if (etherIsDhcp(data))
             {
             if (etherIsDhcpOffer(data))
             {
             stopTimer(discoverMessage);
             //waitMicrosecond(400000);
             dhcpsm.state = SELECTING;
             setPinValue(BLUE_LED, 1);
             waitMicrosecond(100000);
             setPinValue(BLUE_LED, 0);
             setDhcpAddresses(data, 1);
             sendDhcpMessage(data, 3);
             dhcpsm.state = REQUESTING;
             }
             if (etherIsDhcpAck(data))
             {
             dhcpsm.state = BOUND;
             setPinValue(GREEN_LED, 1);
             waitMicrosecond(100000);
             setPinValue(GREEN_LED, 0);
             //startPeriodicTimer(renewState, dhcpsm.t1);
             //startPeriodicTimer(rebindState, dhcpsm.t2);
             setDhcpAddresses(data, 5);
             // etherSendArpRequest(arp, dhcpsm.siaddr);
             }
             }*/
            if (etherIsTcp(data))
            {
                if (etherIsTcpSyn(data))
                {
                    sendEtherTcpSynAck(data);
                }
                if (etherIsTcpFinAck(data))
                {
                    sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                        ipGwAddress, macAddress, destmac,
                                        0x010);
                    mqttstate = 0;
                    tcpstate = 0;
                    stopTimer(mqttPing);
                    sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                        ipGwAddress, macAddress, destmac,
                                        0x011);
                }
                if (etherIsTcpSynAck(data))
                {
                    sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                        ipGwAddress, macAddress, destmac,
                                        0x010);
                    tcpstate = 1;
                    sendMqttConnect(data2, qos);
                }
                if (etherIsMqtt(data))
                {
                    if (etherIsMqttConnectAck(data))
                    {
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                        mqttstate = 1;
                        putsUart0("\n\rMQTT Client Connected\n\r");
                        startPeriodicTimer(mqttPing, 50);
                        //sendMqttPublish(data2);
                        //sendMqttSubscribe(data2);
                    }
                    if (etherIsMqttSubscribeAck(data))
                    {
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                    }
                    if (etherIsMqttUnsubscribeAck(data))
                    {
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                    }
                    if (etherIsMqttPubAck(data))
                    {
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                    }
                    if (etherIsMqttPublish(data))
                    {
                        //setPinValue(BLUE_LED, 1);
                        //waitMicrosecond(100000);
                        //setPinValue(BLUE_LED, 0);
                        printPublishMessage(data);
                        if (qos == 1)
                        {
                            sendMqttPubAck(data, getPublishMessageId(data));
                        }
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                        //processPublish(data);
                        //sendMqttDisconnect(data2);
                    }
                    if (etherIsMqttPing(data))
                    {
                        sendEtherTcpMessage(data, sourceport, 1883, ipAddress,
                                            ipGwAddress, macAddress, destmac,
                                            0x010);
                    }
                }
            }
        }
    }
}
