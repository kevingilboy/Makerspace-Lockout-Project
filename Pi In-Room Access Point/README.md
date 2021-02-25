# Pi In-Room Access Point

The In-Room access point serves as the Makerspace IoT orchestrator. It hosts a local and isolated WLAN for all devices in the Makerspace, receives MQTT messages about IDs being placed on card readers, it asks the remote server if users are authenticated for those tools and receives the response, and it sends MQTT messages to PowerSwitches to enable the relays connected to tools. It also sends emails in case of a tool timeout and can enforce the buddy system.

The NodeJS server app that runs on the Pi can be found in the `site` directory.

The Pi can either be created by flashing an image file or from scratch. The steps for starting from scratch are as follows:

## 1. Flash Raspbian to an SD card
I used [Raspbian Jessie](https://downloads.raspberrypi.org/raspbian/images/raspbian-2017-07-05/) since it was the most recent at the time and I am most familiar with it. I used Win32DiskImager to do the flashing.

## 2. Connect the Pi to Pitt internet over ethernet
I did this manually by modifying `/etc/network/interfaces` to assign the eth0 interface a static IP that the IT department provided for the given ethernet port. I provide the configuration file [here](https://github.com/kevingilboy/Makerspace-Lockout-Project/blob/main/Pi%20In-Room%20Access%20Point/etc/network/interfaces).

## 3. Create the local WLAN
1.  Install the needed packages\
`sudo apt-get install dnsmasq hostapd`
2. End the running instances of those packages\
`sudo systemctl stop hostapd`\
`sudo systemctl stop dnsmasq`\
`sudo service dhcpcd restart`
3. Edit `/etc/dnsmasq.conf`\
I provide the configuration file [here](https://github.com/kevingilboy/Makerspace-Lockout-Project/blob/main/Pi%20In-Room%20Access%20Point/etc/hostapd/hostapd.conf).
4. Edit `/etc/hostapd/hostapd.conf`
I provide the configuration file [here](https://github.com/kevingilboy/Makerspace-Lockout-Project/blob/main/Pi%20In-Room%20Access%20Point/etc/dnsmasq.conf)\
In this file, make sure to set the network SSID, password, and if the network should be hidden
5. Restart the services and interfaces\
`sudo service dhcp restart`\
`sudo ifdown wlan0`\
`sudo ifup wlan0`\
`sudo service hostapd start`\
`sudo service dnsmasq start`

## 4. Install the Mosquitto MQTT Broker
I followed [these](https://www.instructables.com/Installing-MQTT-BrokerMosquitto-on-Raspberry-Pi/) steps which consist of:\
`wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key`\
`sudo apt-key add mosquitto-repo.gpg.key`\
`cd /etc/apt/sources.list.d`\
`wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key`\
`sudo apt-get update`\
`sudo apt-get install mosquitto mosquitto-clients`

## 5. Ensure npm, nodejs, and nodemon are installed
