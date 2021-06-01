# Internet-Of-Things
## Find My IOT Related Code here.

#### Requirements : 
             #### 1 : TM4C123G LaunchPad.
             #### 2 : Ethernet Board.
-------------------------------------------------------------------------------------------------------------------------------------------------------------
### implementing an MQTT client.
>> The goal of this project is add IoT support

* The solution must be implemented on a TM4C123GXL board using an ENC28J60 ethernet interface. The solution will be able to publish topics and subscribe to topics on an MQTT broker, such as Mosquitto. As the intent of this project is to understand the details of these simple elements, your code solution can be based only on provided class code and code you write. You should not be incorporating more than a few lines of code from other sources. 2 Command-line Interface Requirements

* The solution must provide these additional command-line interface using UART0 and configuring the device and reading out the status. The command-line interface should support the following commands additional (to Project 1) commands at a minimum: set MQTT w.x.y.z This command sets the IP address of the MQTT broker and stores this values persistently in EEPROM. publish TOPIC DATA This command publishes a topic and associated data to the MQTT broker. subscribe TOPIC This command subscribes to a topic and then prints the data to the screen when a topic is received later. unsubscribe TOPIC This command unsubscribes from a topic. connect This command send a connect message to the MQTT broker. disconnect The command disconnects from the MQTT broker.

* Project provided me with a better understanding of how the IOT devices Work.

-------------------------------------------------------------------------------------------------------------------------------------------------------------
The goal of this project is build the essential components of a basic embedded Ethernet appliance. Building on a simple Ethernet framework,

an DHCP client and a TCP application

will beadded. The solution must be implemented on a TM4C123GXL board using an ENC28J60 ethernet interface. As the intent of this project is to understand the details of these simple elements, your code solution can be based only on provided class code and code you write. You should not be incorporating more than a few lines of code from other sources.

The solution must provide a command-line interface using UART0 and configuring the device and reading out the status. The command-line interface should support the following commands at a minimum: dhcp ON | OFF This command enables and disables DHCP mode and stores the mode persistently in EEPROM. dhcp dhcp REFRESH | RELEASE This command refreshes a current IP address (if in DHCP mode) or releases the current IP address (if in DHCP mode). set IP | GW | DNS | SN w.x.y.z This command sets the IP address, gateway address, DNS address, and subnet mask (used when DHCP is disabled) and stores these values persistently in EEPROM. ifconfig This command at a minimum dumps the current IP address, DHCP mode, subnet mask, gateway address, and DNS address. reboot: This command restarts the microcontroller.

When DHCP is enabled, the solution must implement an DHCP client. The solution must implement a TCP state machine that implements server functionality (will be in passive open state) with support for at least one socket.


this project helped me understand the behaviour of the Packets.
