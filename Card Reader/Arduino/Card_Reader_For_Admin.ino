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

// Unique identifier of this makerspace
#define TOOLNAME "ms1_volunteer"

// Define some colors and which LED we will be using
#define RED pixels.Color(63,0,0)
#define BLUE pixels.Color(0,0,63)
#define GREEN pixels.Color(0,63,0)
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
int card_in_slot = false;
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
// Callback for any received MQTT message (not used for admin card reader)
//
void callback(char* topic, byte* payload, unsigned int length) {

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
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
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
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(112); //Increase antenna gain
}

void loop() {
  // Reconnect to MQTT if not connected
  if (!client.connected()) {
    reconnect();
  }

  // If a new card has been detected on the reader
  if(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Make sure a card is not in the slot already (this should always be satisfied)
    if(!card_in_slot) {
      card_in_slot = true;
      Serial.println("Card detected");

      // Send server a message with all necessary info to authenticate user.
      sprintf(uid,"%02X%02X%02X%02X\0",mfrc522.uid.uidByte[0],mfrc522.uid.uidByte[1],mfrc522.uid.uidByte[2],mfrc522.uid.uidByte[3]);
      sprintf(sendmsg,"{\"toolname\":\"%s\",\"uid\":\"%s\"}",TOOLNAME,uid);
      client.publish("makermqtt/rfid/admin_card_detected",sendmsg);

      setLED(YELLOW);
      delay(500);
      setLED(GREEN);
    }
  }
  // Else, if a card has been removed from the reader
  else if(!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()){
    // Make sure a card was on the slot already (this should always be satisfied)
    if(card_in_slot){
      card_in_slot = false;

      Serial.println("Card removed");

      // Send server a message with all necessary info to terminate the volunteer's session
      sprintf(sendmsg,"{\"toolname\":\"%s\",\"uid\":\"%s\"}",TOOLNAME,uid);
      client.publish("makermqtt/rfid/admin_card_removed",sendmsg);

      setLED(YELLOW);
      delay(500);
      setLED(RED);
    }
  }

  delay(50);
  client.loop();
}
