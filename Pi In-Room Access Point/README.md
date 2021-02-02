# Pi In-Room Access Point

The In-Room access point serves as the Makerspace IoT orchestrator. It hosts a local and isolated WLAN for all devices in the Makerspace, receives MQTT messages about IDs being placed on card readers, it asks the remote server if users are authenticated for those tools and receives the response, and it sends MQTT messages to PowerSwitches to enable the relays connected to tools. It also sends emails in case of a tool timeout and can enforce the buddy system.

I have an ISO of the original MS1 access point Pi that I will upload soon. Steps for replicating the Pi include:
1. Flash Raspbian Jessie to an SD card
2. Connect the Pi to Pitt internet over ethernet
3. Create the local WLAN using guides like these: ([link1](https://www.raspberrypi.org/documentation/configuration/wireless/access-point-routed.md))([link2](https://thepi.io/how-to-use-your-raspberry-pi-as-a-wireless-access-point/))
4. Install the Mosquitto MQTT Broker ([link](https://www.instructables.com/Installing-MQTT-BrokerMosquitto-on-Raspberry-Pi/))
5. Install nodejs

I have a NodeJS app that orchestrates all devices in the room through MQTT callbacks. I will upload that soon.
