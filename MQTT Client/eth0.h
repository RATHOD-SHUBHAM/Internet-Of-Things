// ETH0 Library
// Guide : Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef ETH0_H_
#define ETH0_H_

#include <stdint.h>
#include <stdbool.h>

#define ETHER_UNICAST        0x80
#define ETHER_BROADCAST      0x01
#define ETHER_MULTICAST      0x02
#define ETHER_HASHTABLE      0x04
#define ETHER_MAGICPACKET    0x08
#define ETHER_PATTERNMATCH   0x10
#define ETHER_CHECKCRC       0x20

typedef struct _enc28j60Frame // 4-bytes
{
  uint16_t size;
  uint16_t status;
  uint8_t data;
} enc28j60Frame;

typedef struct _etherFrame // 14-bytes
{
  uint8_t destAddress[6];
  uint8_t sourceAddress[6];
  uint16_t frameType;
  uint8_t data;
} etherFrame;

typedef struct _ipFrame // minimum 20 bytes
{
  uint8_t revSize;
  uint8_t typeOfService;
  uint16_t length;
  uint16_t id;
  uint16_t flagsAndOffset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t headerChecksum;
  uint8_t sourceIp[4];
  uint8_t destIp[4];
} ipFrame;

typedef struct _icmpFrame
{
  uint8_t type;
  uint8_t code;
  uint16_t check;
  uint16_t id;
  uint16_t seq_no;
  uint8_t data;
} icmpFrame;

typedef struct _arpFrame
{
  uint16_t hardwareType;
  uint16_t protocolType;
  uint8_t hardwareSize;
  uint8_t protocolSize;
  uint16_t op;
  uint8_t sourceAddress[6];
  uint8_t sourceIp[4];
  uint8_t destAddress[6];
  uint8_t destIp[4];
} arpFrame;

typedef struct _udpFrame // 8 bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint16_t length;
  uint16_t check;
  uint8_t  data;
} udpFrame;

// dhcp frame
typedef struct _dhcpFrame
{
uint8_t op;
uint8_t htype;
uint8_t hlen;
uint8_t hops;
uint32_t xid;
uint16_t secs;
uint16_t flags;
uint8_t ciaddr[4];
uint8_t yiaddr[4];
uint8_t siaddr[4];
uint8_t giaddr[4];
uint8_t chaddr[16];
uint8_t data[192];
uint32_t magicCookie;
uint8_t options[0];
} dhcpFrame;

//tcp frame
typedef struct _tcpframe
{
    uint16_t srcp;
    uint16_t destp;
    uint32_t seq;
    uint32_t ack;
    unsigned res:4;
    unsigned off:4;
    uint8_t flags;
    uint16_t winsize;
    uint16_t checksum;
    uint16_t urgptr;
    uint8_t data;
    //uint8_t option[0];
} tcpFrame;

typedef struct _mqttframe
{
    uint8_t hdrflag;
    uint8_t msglen;
    uint16_t protocolnamelen;
    char protocolname[4];
    uint8_t version;
    uint8_t connflag;
    uint16_t keepalive;
    uint16_t clidlen;
    char clid[0];
} mqttFrame;

typedef struct _mqttpublishframe
{
    uint8_t hdrflag;
    uint8_t msglen;
    uint16_t topicnamelen;
    uint8_t topicname;
    //uint8_t payload;
} mqttPublishFrame;

typedef struct _mqttpubackframe
{
    uint8_t hdrflag;
    uint8_t msglen;
    uint16_t msgidentifier;
} mqttPubAckFrame;

typedef struct _mqttsubscribeframe
{
    uint8_t hdrflag;
    uint8_t msglen;
    uint16_t msgid;
    uint16_t topicnamelen;
    uint8_t topicname;
    //uint8_t qos;
} mqttSubscribeFrame;

typedef struct _mqttdisconnectframe
{
    uint8_t hdrflag;
    uint8_t msglen;
} mqttDisconnectFrame;

typedef struct _mqttunsubscribeframe
{
    uint8_t hdrflag;
    uint8_t msglen;
    uint16_t msgid;
    uint16_t topicnamelen;
    uint8_t topicname;
} mqttUnsubscribeFrame;

typedef struct _mqttping
{
    uint8_t hdrflag;
    uint8_t msglen;
} mqttPingFrame;

#define ETHER_HALFDUPLEX     0x00
#define ETHER_FULLDUPLEX     0x100

#define LOBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) >> 8) & 0xFF)

extern uint32_t sum;
extern uint8_t ipAddress[4];
extern uint8_t ipGwAddress[4];
extern uint8_t ipSubnetMask[4];
extern uint8_t macAddress[6];
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void etherInit(uint16_t mode);
bool etherIsLinkUp();

bool etherIsDataAvailable();
bool etherIsOverflow();
uint16_t etherGetPacket(uint8_t packet[], uint16_t maxSize);
bool etherPutPacket(uint8_t packet[], uint16_t size);

bool etherIsIp(uint8_t packet[]);
bool etherIsIpUnicast(uint8_t packet[]);

bool etherIsPingRequest(uint8_t packet[]);
void etherSendPingResponse(uint8_t packet[]);

bool etherIsArpRequest(uint8_t packet[]);
void etherSendArpResponse(uint8_t packet[]);
void etherSendArpRequest(uint8_t packet[], uint8_t ip[]);
void etherSumWords(void* data, uint16_t sizeInBytes);
uint16_t getEtherChecksum();

bool etherIsUdp(uint8_t packet[]);
uint8_t* etherGetUdpData(uint8_t packet[]);
void etherSendUdpResponse(uint8_t packet[], uint8_t* udpData, uint8_t udpSize);
void sendUdpPacket(uint8_t packet[], char* data);

void etherEnableDhcpMode();
void etherDisableDhcpMode();
bool etherIsDhcpEnabled();
bool etherIsIpValid();
void etherSetIpAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpAddress(uint8_t ip[4]);
void etherSetIpGatewayAddress(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void etherGetIpGatewayAddress(uint8_t ip[4]);
void etherSetIpSubnetMask(uint8_t mask0, uint8_t mask1, uint8_t mask2, uint8_t mask3);
void etherGetIpSubnetMask(uint8_t mask[4]);
void etherSetMacAddress(uint8_t mac0, uint8_t mac1, uint8_t mac2, uint8_t mac3, uint8_t mac4, uint8_t mac5);
void etherGetMacAddress(uint8_t mac[6]);
void sendDhcpMessage(uint8_t packet[], uint8_t type);

uint16_t htons(uint16_t value);
uint32_t htonl(uint32_t value);
#define ntohs htons

#endif
