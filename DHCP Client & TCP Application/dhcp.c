/*
 * dhcp.c
 *
 *  Created on: Mar 22, 2020
 *      Author: shubham
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "eth0.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "dhcp.h"
#include "timer.h"

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

#define MAX_PACKET_SIZE 1522

uint8_t data[MAX_PACKET_SIZE];

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
    putsUart0("IP: ");
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
    putsUart0("SN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (etherIsLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");

}

bool check_tcp(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    bool ok;
    uint16_t tmp16;
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

bool check_tcp_syn(uint8_t packet[])
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

void send_tcp_synack(uint8_t packet[])
{
    uint16_t tmp16;
    uint8_t tmp_ipaddr[4];
    uint8_t tmp_macaddr[6];
    uint16_t tcp_length;

    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    tcpFrame* tcp = (tcpFrame*) ((uint8_t*) ip + (ip->revSize & 0xF) * 4);

    tmp16 = tcp->dest_port;
    tcp->dest_port = tcp->src_port;
    tcp->src_port = tmp16;
    tcp->acknowledgment = htonl(htonl(tcp->sequence) + 1);
    tcp->sequence = 0;
    tcp->offset = 5;
    tcp->reserve = 0;
    tcp->flags = 0b00010010;
    tcp->window_size = htons(1280);
    tcp->urgent_ptr = 0;

    memcpy(&tmp_ipaddr, &ip->sourceIp, 4);
    memcpy(&ip->sourceIp, &ip->destIp, 4);
    memcpy(&ip->destIp, &tmp_ipaddr, 4);
    //ip->headerChecksum = 0;

    memcpy(&tmp_macaddr, &ether->sourceAddress, 6);
    memcpy(&ether->sourceAddress, &ether->destAddress, 6);
    memcpy(&ether->destAddress, &tmp_macaddr, 6);

    ip->length = htons(((ip->revSize & 0xF) * 4) + 20);
    // 32-bit sum over ip header
    sum = 0;
    etherSumWords(&ip->revSize, 10);
    etherSumWords(ip->sourceIp, ((ip->revSize & 0xF) * 4) - 12);
    ip->headerChecksum = getEtherChecksum();
    tcp_length = 20;
    // 32-bit sum over pseudo-header
    sum = 0;
    etherSumWords(ip->sourceIp, 8);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    etherSumWords(&tcp_length, 2);
    etherSumWords(tcp, 20);
    tcp->checksum = getEtherChecksum();
    etherPutPacket(ether, 14 + ((ip->revSize & 0xF) * 4) + 20);

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
    return 500;
}

bool checkDHCPpacket(uint8_t packet[])
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    if (ip->protocol == 17 && htons(udp->destPort) == 68
            && statemachine.xid == dhcp->xid)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool checkDHCPoffer(uint8_t packet[])
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

bool checkDHCPAcknowledgment(uint8_t packet[])
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

void setDhcpAddress(uint8_t packet[], uint8_t type)
{
    etherFrame* ether = (etherFrame*) packet;
    ipFrame* ip = (ipFrame*) &ether->data;
    udpFrame* udp = (udpFrame*) ((uint8_t*) ip + ((ip->revSize & 0xF) * 4));
    dhcpFrame* dhcp = (dhcpFrame*) &udp->data;
    uint16_t serveraddressvalue = searchDhcpOptions(packet, 54);
    uint16_t gatewayaddressvalue = searchDhcpOptions(packet, 3);

    if (serveraddressvalue != 500 && gatewayaddressvalue != 500)
    {
        memcpy(&statemachine.serveraddressvalue, &dhcp->options[serveraddressvalue + 2], 4);
        memcpy(&statemachine.gatewayaddressvalue, &dhcp->options[gatewayaddressvalue + 2], 4);
        memcpy(&statemachine.youripaddress, &dhcp->yiaddr, 4);
        if (type == 5)
        {
            memcpy(&statemachine.clientipaddress, &dhcp->yiaddr, 4);
            etherSetIpAddress(dhcp->yiaddr[0], dhcp->yiaddr[1], dhcp->yiaddr[2],dhcp->yiaddr[3]);
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

void main()

{
//    uint8_t arp[MAX_PACKET_SIZE];
    uint8_t info[MAX_PACKET_SIZE];
    uint8_t i;
    uint32_t test = 1;
    uint32_t num;
    uint8_t* udp_data;
    USER_DATA user_data;

    // Init controller
    initHw();
    initTimer();
    initEeprom();

    writeEeprom(0x0001, test);

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    statemachine.state = INIT;
    startPeriodicTimer(discoverMessage, 15);
 //   sendDhcpMessage(data, 1);
    while (true)
    {
        if (kbhitUart0())
        {
            getsUart0(&user_data);
            ParseFields(&user_data);
            if (strcmp(user_data.first, "ifconfig"))
            {
                displayConnectionInfo();
            }
            if (strcmp(user_data.first, "dhcp"))
            {
                if (strcmp(user_data.second, "release"))
                {
                    if (statemachine.state == BOUND)
                    {
                        //sendDhcpMessage(data, 7);
                    }
                }
                if (strcmp(user_data.second, "renew"))
                {
                    if (statemachine.state == BOUND)
                    {
                        //sendDhcpMessage(data, 3);
                    }
                }
                if (strcmp(user_data.second, "on"))
                {
                    etherEnableDhcpMode();
                }
                if (strcmp(user_data.second, "off"))
                {
                    etherDisableDhcpMode();
                }
            }
            if (strcmp(user_data.first, "setip"))
            {
                for (i = 0; user_data.second[i] != '\0'; i++)
                {
                    if (user_data.second[i] != ".")
                        ipAddress[i] = user_data.second[i];
                }
            }
            if (strcmp(user_data.first, "setgw"))
            {
                for (i = 0; user_data.second[i] != '\0'; i++)
                {
                    if (user_data.second[i] != ".")
                        ipGwAddress[i] = user_data.second[i];
                }
            }
            if (strcmp(user_data.first, "setsn"))
            {
                for (i = 0; user_data.second[i] != '\0'; i++)
                {
                    if (user_data.second[i] != ".")
                        ipSubnetMask[i] = user_data.second[i];
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
                }
            }

            if (checkDHCPpacket(data))
            {
                if (checkDHCPoffer(data))
                {
                    stopTimer(discoverMessage);
                    statemachine.state = SELECTING;
                    setPinValue(GREEN_LED, 1);
                    waitMicrosecond(100000);
                    setPinValue(GREEN_LED, 0);
                    setDhcpAddress(data, 1);
                    sendDhcpMessage(data, 3);
                    statemachine.state = REQUESTING;
                }

                if (checkDHCPAcknowledgment(data))
                {
                    statemachine.state = BOUND;
                    setPinValue(BLUE_LED, 1);
                    waitMicrosecond(100000);
                    setPinValue(BLUE_LED, 0);
                    //startPeriodicTimer(renewState, dhcpsm.t1);
                    //startPeriodicTimer(rebindState, dhcpsm.t2);
                    setDhcpAddress(data, 5);
                    // etherSendArpRequest(arp, dhcpsm.siaddr);
                }
            }

            if (check_tcp(data))
            {
                if (check_tcp_syn(data))
                {
                    send_tcp_synack(data);
                }
            }

        }
    }

}
