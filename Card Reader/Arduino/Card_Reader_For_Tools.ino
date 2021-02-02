// Kevin Gilboy 2018
// Pitt Makerspace

#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Unique identifier of this tool. Must be the same name used by the
// PowerSwitch.ino code uploaded to the power-controlling arduino.
#define TOOLNAME  "ms1_drill_press"

// If the tool requires an admin to be in the room, set to 1
#define REQ_ADMIN 0

// Define some colors and which LED we will be using
#define RED    pixels.Color(63,0,0)
#define BLUE   pixels.Color(0,0,63)
#define GREEN  pixels.Color(0,63,0)
#define YELLOW pixels.Color(63,31,0)
#define LED 0

// Define the pins for the RFID reader
#define RST_PIN   4     // Configurable
#define SS_PIN    5    // Configurable

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create neopixel instance
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, 15);

// Store the wifi credentials and mqtt server IP
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mqtt_server = "192.168.0.1";

// Wifi/MQTT comms variables
WiFiClient espClient;
PubSubClient client(espClient);
char recmsg[127];
char sendmsg[127];
char uid[9];

// State variables
bool card_in_slot = false;
int powerswitch_status = 0; // this could be a bool but its easier to keep as an int since its sent to the server in a string

//
// Sets LED color
//
void setLED(uint32_t c){
  pixels.setPixelColor(0,c);
  pixels.show();
}

//
// Sets up wifi
//
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Show blue while we are connecting
  setLED(BLUE);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Once connected, show red, which means the tool is disabled and we are waiting on an ID to be inserted
  setLED(RED);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//
// Callback for any received MQTT message
//
void callback(char* topic, byte* payload, unsigned int length) {
  // Copy the message into a char buffer
  for (int i=0; i<length; i++) {
    recmsg[i] = (char)payload[i];
  }

  // The action we are interested in is in the first few characters. Either "off", "on", or "timeout"
  if (strncmp(recmsg,"off",3)==0) {
    // If msg says off, then the server just told the powerswitch to turn off. We should remember that here.
    // Cases like invalid IDs end up here too. We set the LED to red to signal the tool is disabled.
    powerswitch_status = 0;
    setLED(RED);
  } else if(strncmp(recmsg,"on",2)==0) {
    // If msg says on, then the server just told the powerswitch to turn on. We should remember that here.
    // Set the LED to green to signal the tool is enabled.
    powerswitch_status = 1;
    setLED(GREEN);
  } else if(strncmp(recmsg,"timeout",7)==0) {
    // If the msg says timeout, then the powerswitch just told the server that it has been enabled for too long without usage.
    // We send a message to the server to let them know the card UID so it can email the student about this bad behavior.
    // The server will also respond with a "off" message to the tool so we do not have to setLED or change status here.
    sprintf(sendmsg,"{\"toolname\":\"%s\",\"uid\":\"%s\",\"req_admin\":%d,\"tool_on\":%d}",TOOLNAME,uid,REQ_ADMIN,powerswitch_status);
    client.publish("makermqtt/rfid/card_timeout",sendmsg);
  }
}

//
// Reconnect to MQTT server
//
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Show blue while reconnecting
    setLED(BLUE);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(TOOLNAME"_rfid")) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("makermqtt/"TOOLNAME);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }

  // Once reconnected, show red
  setLED(RED);
}

void setup() {
  // Initialize the BUILTIN_LED pin as an output
  pinMode(LED, OUTPUT);
  digitalWrite(LED,HIGH);
  pixels.begin();

  // Begin Serial for debugging
  Serial.begin(115200);

  // Connect to Wifi and configure mqtt server/callback
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Init the RFID reader
  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 card
  mfrc522.PCD_SetAntennaGain(112); //Increase antenna gain
}

void loop() {
  // Reconnect to MQTT if not connected
  if (!client.connected()) {
    reconnect();
  }

  // If a new card has been detected on the reader
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Make sure a card is not in the slot already (this should always be satisfied)
    if (!card_in_slot) {
      card_in_slot = true;

      // Show yellow as we attempt to authenticate the user and if auth, then the server will tell the tool to power on.
      setLED(YELLOW);
      Serial.println("Card detected");

      // Send server a message with all necessary info to authenticate user. The servers response is handled in the callback function above
      sprintf(uid,"%02X%02X%02X%02X\0",mfrc522.uid.uidByte[0],mfrc522.uid.uidByte[1],mfrc522.uid.uidByte[2],mfrc522.uid.uidByte[3]);
      sprintf(sendmsg,"{\"toolname\":\"%s\",\"uid\":\"%s\",\"req_admin\":%d,\"tool_on\":%d}",TOOLNAME,uid,REQ_ADMIN,powerswitch_status);
      client.publish("makermqtt/rfid/tool_card_detected",sendmsg);
      delay(500);
    }
  }
  // Else, if a card has been removed from the reader
  else if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    // Make sure a card was on the slot already (this should always be satisfied)
    if (card_in_slot) {
      card_in_slot = false;

      Serial.println("Card removed");

      // Send server a message with all necessary info to terminate the user's session on the tool. The servers response is handled in the callback function above
      sprintf(sendmsg,"{\"toolname\":\"%s\",\"uid\":\"%s\",\"req_admin\":%d,\"tool_on\":%d}",TOOLNAME,uid,REQ_ADMIN,powerswitch_status);
      client.publish("makermqtt/rfid/tool_card_removed",sendmsg);

      //Show yellow as we attempt to get the server to tell the tool to power off.
      setLED(YELLOW);
      delay(500);
    }
  }

  delay(100);
  client.loop();
}
