/*
 * dhcp.h
 *
 *  Created on: Mar 22, 2020
 *      Author: shubham
 */

#ifndef DHCP_H_
#define DHCP_H_
#define MAX_PACKET_SIZE 1522

extern uint8_t ipAddress[4];
extern uint8_t data[MAX_PACKET_SIZE];

typedef struct _dhcpstatemachine
{
    uint8_t state;
    uint8_t serveraddressvalue[4];
    uint8_t gatewayaddressvalue[4];
    uint8_t youripaddress[4];
    uint8_t clientipaddress[4];
    uint32_t temp1;
    uint32_t temp2;
    uint32_t xid;


}dhcpstatemachine;

extern dhcpstatemachine statemachine;
bool Dhcp_Ack(uint8_t packet[]);


#endif /* DHCP_H_ */
