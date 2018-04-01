#include "DHT.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3

#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>

//-------- Customise these values -----------

const char* ssid_fritz = "";
const char* password_fritz = "";


#define ORG "ldgwxb"
#define DEVICE_TYPE "ESP8266"
#define DEVICE_ID "ESP8266-e12-1"
#define TOKEN <your iot org token>
#define DHTPIN 14             // digital pin for DHT sensor
#define SWITCHPIN 12
#define SERVOPIN 13
#define RAINPIN A0
#define LEDPIN 4
#define D_RAINPIN 13
#define DHTTYPE DHT11        // DHT 11 type of the DHT sensor


char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char topic[] = "iot-2/evt/status/fmt/json";
char topic_sub1[] = "iot-2/cmd/update/fmt/json";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

//the time we give the sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 20;
int ledStatus = LOW;
unsigned long now = millis();
unsigned long lastUpload = 0;

int ldrMin = 0;
int ldrMax = 1024;

int DOOR_OPEN = 0;
int DOOR_CLOSED = 90;

DHT dht(DHTPIN, DHTTYPE);

Servo myservo;
int pos = 0; 

const char* LIGHT_ACT = "light";
const char* DOOR_ACT = "door";
const char* WINDOW_ACT = "window";

bool isOnline = true;

ESP8266WiFiMulti wifiMulti;

WiFiClient wifiClient;
PubSubClient client(server, 1883, NULL, wifiClient);

/****************************
   Auxiliary Functions
 ****************************/
bool bluemixConnection()
{
  int timeout = 20;
  // Check connection to Bluemix
  if (!client.connected())
  {
    Serial.print("Reconnecting client to ");
    Serial.println(server);
    while (!client.connect(clientId, authMethod, token) && timeout > 0) {
      Serial.print(".");
      delay(1000);
      timeout--;
    }

    if (timeout == 0) {
      return false;
    } else {            
      return true;
    }
  }
}



String getTimeTag() {
  Serial.println("getTimeTag");
   HTTPClient http;
   http.begin("http://9.149.245.238/redir.html");
   int httpCode = http.GET();
   Serial.print("HTTP GET Response: "); Serial.println(httpCode); // HTTP code 200 means ok 
   String s = http.getString();
   http.end();

   Serial.print("s:"); Serial.println(s);

   int x = s.indexOf("au_pxytimetag value=");
   Serial.print("x:"); Serial.println(x);      
   if (x != -1) {
      x += 21;
      int y = s.indexOf("\">", x);
      String timetag = s.substring(x, y);
      Serial.print("timetag:"); Serial.println(timetag);      
      return timetag;
   }
   return "";
}

boolean wifiConnect() {
  int timeout = 4 * 10; // 10 secs
  while ((wifiMulti.run() != WL_CONNECTED) && timeout-- > 0) {
    delay(250);
  }
  if (timeout == 0) {
    return false;
  }

  Serial.print("Connected to: "); Serial.print(WiFi.SSID());
  Serial.print(", IP address: "); Serial.println(WiFi.localIP());
  return true;
}

void openDoor() {
  myservo.attach(SERVOPIN);
  delay(1000);
  for (pos = DOOR_CLOSED; pos >= DOOR_OPEN; pos-= 1) 
  {
    myservo.write(pos);
  }
  delay(2000);
  myservo.detach();
}

void closeDoor() {
  myservo.attach(SERVOPIN);
  delay(1000);
  for (pos = DOOR_OPEN; pos <= DOOR_CLOSED; pos+= 1) 
  {
    myservo.write(pos);
  }
  delay(2000);
  myservo.detach();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  char delimit[]= "{\":},";
  char *tokens[10]; 
  int i = 0;
  tokens[i] = strtok((char*)payload, delimit);
  while(tokens[i] != NULL) {
    Serial.println(tokens[i]);
    i++;
    tokens[i] = strtok(NULL, delimit);
  }
  Serial.println(tokens[3]);
  int val = atoi(tokens[3]);
  if (val == 1) {
    if (strcmp(tokens[1], LIGHT_ACT) == 0) {
      Serial.println("turning on light");
      digitalWrite(LEDPIN, HIGH); 
    }
    else if (strcmp(tokens[1], DOOR_ACT) == 0 || strcmp(tokens[1], WINDOW_ACT) == 0) {
      Serial.println("opening door");
      openDoor();
    }
  } else if (val == 0) {
    if (strcmp(tokens[1], LIGHT_ACT) == 0) {
      Serial.println("turning off light");
      digitalWrite(LEDPIN, LOW); 
    }
    else if (strcmp(tokens[1], DOOR_ACT) == 0 || strcmp(tokens[1], WINDOW_ACT) == 0) {
      Serial.println("closing door");
      closeDoor();
    }
  } else {
    for (int i = 0; i < length; i++) {
      digitalWrite(LEDPIN, HIGH);
      delay(100);
      digitalWrite(LEDPIN, LOW);
      delay(100);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(SWITCHPIN, INPUT);
  pinMode(RAINPIN, INPUT);
  pinMode(D_RAINPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  myservo.attach(SERVOPIN);
  dht.begin();

  // Connecting ESP to Wifi
  Serial.println("Connecting to WiFi...");
  wifiMulti.addAP(ssid_fritz, password_fritz);


  if (!!!wifiConnect()) {
    Serial.println("*** Failed to connect to WiFi");
    isOnline = false;
    return;
  }
  client.setCallback(callback);
  //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
  
  for (int i = 0; i < calibrationTime; i++) {
    Serial.print(".");
    digitalWrite(LEDPIN, ledStatus);
    ledStatus= ledStatus==HIGH ? LOW:HIGH;
    delay(1000);
  }
  Serial.println("");
  Serial.println("testing servo ");
  myservo.write(DOOR_OPEN);
  for (pos = 0; pos <= 180; pos+= 1) 
  {
    myservo.write(pos);
    delay(15);
    if (digitalRead(SWITCHPIN)==0){
      DOOR_CLOSED = pos;
      Serial.print("door closes at ");
      Serial.println(DOOR_CLOSED);
      myservo.detach();
      break;
    }
  }
   
  Serial.println(" done");
  Serial.println("SENSOR ACTIVE");
  digitalWrite(LEDPIN, LOW);
  delay(50);
}


/**
 * This method does the following,
 * 
 * 1. Listen for the command from the Gateway (raspberry Pi) and glow LED which is connected to the PIN 13
 * 2. Sends the sensors status to the Gateway in the following format,
 * 
 *      a. The name of the event - Required by IoT Foundation followed by space
 *      b. comma separated datapoints
 *      
 *      "status humidity:60%,temp:35.22,heat:20,rain:1,soil:1,pir:0,count:1"
 *      
 */

void loop() {

  if (!!!client.connected()) {
    Serial.print("Reconnecting client to ");
    Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("Client connected");
    if (client.subscribe(topic_sub1))
    {
      Serial.print("Subscription to ");
      Serial.print(topic_sub1);
      Serial.println(" successful");
    }
    else
    {
      Serial.print("Subscription to ");
      Serial.print(topic_sub1);
      Serial.println(" failed");
    }
    
    Serial.println();
  }
  if(!!!client.loop())
  {
    Serial.println("PubSubCLient loop failed");
  }

  now = millis();
  if(now-lastUpload < 2000)
  {
    return;
  }
  lastUpload=now;
  
  char val[10];

  // Read Humidity
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read rain sensor
  int rainsensorValue = 100 - map(analogRead(RAINPIN), ldrMin, ldrMax, 0, 99);
  int d_rainsensorValue = digitalRead(D_RAINPIN);
  
  // Read switch sensor
  int switchVal = 1 - digitalRead(SWITCHPIN);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(rainsensorValue) || isnan(switchVal) ){
    Serial.println("Failed to read from sensors!");
    delay(1000);
    return;
  }
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  String payload = "{\"d\":{\"myName\":\"";
  payload += DEVICE_ID;
  
  // Add humidity
  payload += "\",\"humidity\":";
  dtostrf(h,1,2, val);
  payload += val;
  
  // Add temperature
  payload += ",\"temp\":";
  dtostrf(t,1,2, val);
  payload += val;

  // Add heating index
  payload += ",\"heat\":";
  dtostrf(hic,1,2, val);
  payload += val;
 
  // Add rain sensor value
  payload += ",\"rain\":";
  itoa(rainsensorValue, val, 10);
  payload += val;
  
  // Add switch value
  payload += ",\"switch\":";
  itoa(switchVal, val, 10);
  payload += val;
  payload += "}}";

  Serial.print("Sending payload: ");
  Serial.println(payload);

  if (client.publish(topic, (char*) payload.c_str())) {
    Serial.println("Publish ok");
  } else {
    Serial.println("Publish failed");
  }
  //delay(2000);
}


