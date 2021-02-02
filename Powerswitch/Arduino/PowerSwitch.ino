// Kevin Gilboy 2018
// Pitt Makerspace

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Unique identifier of this tool. Must be the same name used by the
// Card_Reader_For_Tools.ino code uploaded to the card-reading arduino.
#define TOOLNAME "ms1_drill_press"

// If we do not have a current sensor on hand, set this to false
#define CHECK_CURRENT true

// Some tweakable defined parameters
#define TOOL_TIMEOUT 30000
#define CURRENT_THRESHOLD 200
#define DELAY_BEFORE_FIRST_READING 7500
#define DELAY_BETWEEN_READINGS 6667
#define NUMBER_OF_READINGS 4
#define NUMBER_OF_BUZZES 3
#define PITCH_OF_BUZZES 64

// Define the pins for I/O
#define LED_PIN 0
#define BUZER_PIN 14
#define RELAY_PIN 16
#define CURRENT_PIN A0

// Store the wifi credentials and mqtt server IP
const char* ssid = "ssid";
const char* password = "PASSWORD";
const char* mqtt_server = "192.168.0.1";

// Wifi/MQTT comms variables
WiFiClient espClient;
PubSubClient client(espClient);
char recmsg[127];

// State variables
bool sent_email = false;
int tool_on = true;
unsigned long last_time_off = millis();
bool card_in_reader = false;
int readings[NUMBER_OF_READINGS];

//
// Sets up wifi
//
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

// Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

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

    // The action we are interested in is in the first few characters. Either "off" or "on"
    if (strncmp(recmsg,"off",3)==0){
      // If msg says off (likely because a user removed their ID from the reader), turn off the tool.
      Serial.println("Turning off");
      card_in_reader = false;

      //If user removed ID without turning off tool, buzz at 'em
      if (CHECK_CURRENT) {
        if (current_detected()) {
          buzz();
        }
      }

      //Turn off powerswitch
      turn_off();
      tool_on = false;
    } else if (strncmp(recmsg,"on",2)==0) {
      // If msg says on (likely because an authenticated user put their ID on the reader), turn on the tool
      Serial.println("Turning on");
      card_in_reader = true;
      sent_email = false; // reset this flag

      //Turn on powerswitch
      turn_on();
      tool_on = true;

      if (CHECK_CURRENT) {
        //If tool was left in the on state (i.e. a current is detected immediately), buzz at 'em
        // Keep trying to turn enable the relay, switching it off if current is detected
        while (current_detected()) {
          turn_off();
          if(!card_in_reader) break;
          buzz();
          client.loop(); // Need to call this to not block any new messages sent by the server
          turn_on();
        }
      }
    }
}

//
// Reconnect to MQTT server
//
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(TOOLNAME"_powertail")) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("makermqtt/"TOOLNAME);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//
// Detect current from the current sensor
//
bool current_detected() {
  int zeros = 0;
  int below_noise_threshold = 0;

  delayMicroseconds(DELAY_BEFORE_FIRST_READING-DELAY_BETWEEN_READINGS);
  // Take multiple readings
  for (int i=0;i<NUMBER_OF_READINGS;i++) {
    delayMicroseconds(DELAY_BETWEEN_READINGS);
    readings[i] = analogRead(CURRENT_PIN);
    if(readings[i]>CURRENT_THRESHOLD) return true;
    if(readings[i]==0) zeros++;
    if(readings[i]<=32 && readings[i]>0) below_noise_threshold++;
  }

  // This is a tuned condition that determines if current has been detected. Based on power consumption of the tool, this may need tweaking for new tools
  if((zeros==NUMBER_OF_READINGS-1 && below_noise_threshold==1) || zeros==NUMBER_OF_READINGS) return true;
  return false;
}

//
// Buzz at em'
//
void buzz() {
  for(int i=0;i<NUMBER_OF_BUZZES;i++){
    analogWrite(BUZER_PIN,64);
    delay(250);
    analogWrite(BUZER_PIN,0);
    delay(250);
  }
}

//
// Turn off the tool relay and onboard LED
//
void turn_off(){
  digitalWrite(LED_PIN,LOW);
  digitalWrite(RELAY_PIN,LOW);
}

//
// Turn on the tool relay and onboard LED
//
void turn_on(){
  digitalWrite(LED_PIN,HIGH);
  digitalWrite(RELAY_PIN,HIGH);
}

void setup() {
  // Initialize the BUILTIN_LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Make sure the relay and buzzer start off
  turn_off();
  analogWrite(BUZER_PIN,0);

  // Begin Serial for debugging
  Serial.begin(115200);

  // Connect to Wifi and configure mqtt server/callback
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // Reconnect to MQTT if not connected
  if (!client.connected()) {
    reconnect();
  }

  // If current sensing is enabled
  if (CHECK_CURRENT) {
    // If an authenticated card is in the corresponding reader
    if (card_in_reader) {
      // If tool being used, keep delaying last_time_off so an email isn't sent
      if(current_detected()){
        last_time_off = millis();
        tool_on = true;
      } else if(tool_on){
        // If tool is not being used but is in the on state, save the time so that we know when to alert the user that they left their ID behind
        last_time_off = millis();
        tool_on = false;
      }

      //If it has been longer than a set amount of time, emit "timeout" signal to server
      // Since it is unsigned subtraction we do not have to worry about millis() resetting
      if((unsigned long)(millis()-last_time_off)>=TOOL_TIMEOUT && !sent_email){
        client.publish("makermqtt/"TOOLNAME,"timeout");
        sent_email = true; // set this flag so that we do not send emails on every loop
      }
    }
  }

  delay(2000);
  client.loop();
}
