#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <XTronical_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include "DHT.h"
#include <SoftwareSerial.h> 
#include "Arduino.h"
#define DHTPIN 4
#define DHTTYPE DHT22
#define GUVA_S12SD_PIN_OUT  A0

#define TFT_DC     D4       // register select (stands for Data Control perhaps!)
#define TFT_RST   D0         // Display reset pin, you can also connect this to the ESP8266 reset
                            // in which case, set this #define pin to -1!
#define TFT_CS   D1 
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);  
#define speakerPin D6
uint32_t x=0;
boolean isAlarm_temp;
boolean isAlarm_hum;
boolean isAlarm_UV;
/************ WiFi Access Point *********************************/

#define WLAN_SSID       "ASUS_X008D"
#define WLAN_PASS       "anca1997"


/************************* IBM Watson *********************************/
#define ORG "ogrt8q"
#define DEVICE_TYPE "arduino"
#define DEVICE_ID "arduino4321"
#define TOKEN "HTIRGonaRvQhlWj*PP"
/************************* IBM Watson some more*********************************/
char server[] =  "ogrt8q.messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;
/************************* Evenimente*********************************/
const char ev_t[] = "iot-2/evt/event_t/fmt/json";
const char ev_h[] = "iot-2/evt/event_h/fmt/json";
const char ev_u[] = "iot-2/evt/event_UV/fmt/json";
//const char cmdTopic[] = "iot-2/cmd/led/fmt/json";
float temp, umid;
float UV;
int publishInterval = 60000; 
long lastPublishMillis;

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  8883                   // use 8883 for SSL
#define AIO_USERNAME    
#define AIO_KEY         

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
//WiFiClient client;
// or... use WiFiFlientSecure for SSL
WiFiClientSecure wifiClient;
DHT dht(DHTPIN, DHTTYPE);

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&wifiClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/


// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Publish temperature = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Temperature");
Adafruit_MQTT_Publish humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Humidity");
Adafruit_MQTT_Publish UV_rate = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/UV_index");
Adafruit_MQTT_Subscribe alarm_temperature = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Alarm_temperature");
Adafruit_MQTT_Subscribe alarm_humidity = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Alarm_humidity");
Adafruit_MQTT_Subscribe alarm_UV = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Alarm_UV_rate");
/*************************** Sketch Code ************************************/
void callback(char* topic, byte* payload, unsigned int payloadLength) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < payloadLength; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
PubSubClient client (server, 8883, callback, wifiClient);


 
   
void setup() {
  Serial.begin(9600);
    pinMode(speakerPin, OUTPUT);
    pinMode(GUVA_S12SD_PIN_OUT, INPUT);
  delay(10);
 dht.begin();
  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&alarm_temperature);
  mqtt.subscribe(&alarm_humidity);
  mqtt.subscribe(&alarm_UV);
  delay(200);
   
    mqttConnect();
    MQTT_connect();
/************************* LCD-ul*********************************/
  
  tft.init();  
  tft.setRotation(2);
  tft.fillScreen(ST7735_BLACK);
  // initialize a ST7735S chip,
  tft.setTextColor(ST7735_BLUE);
  tft.setTextSize(3);
  tft.setCursor(2,2);
  tft.println("SMI");
  tft.println("IBM");
  tft.println("WATSON");
   tft.setRotation(2);
  
  delay(500);
}




void loop() {
  delay(2000);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float sensorValue = analogRead(GUVA_S12SD_PIN_OUT);
  //float sensorVoltage = sensorValue/4095*3.3;
  float sensorVoltage = ((sensorValue*3.3)/1020.4);
  float u = sensorVoltage;
  delay(200);
  if (isnan(h) || isnan(t)) {
    Serial.println("\nFailed to read from DHT sensor!");
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);
  delay(100);

mqttConnect();
   temp = t;
  publish_t();
  umid = h;
  publish_h();
  UV = u;
  publish_u();
  delay(500);
  client.disconnect();
  
  MQTT_connect();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(2000))) {
    yield();
    if (subscription == &alarm_temperature) {
     
      char *value = (char *)alarm_temperature.lastread;
      Serial.print(F("Got: "));
      Serial.println(value);
      String message = String(value);
      message.trim();
      if(message =="ON")
      {
        
        Serial.println("*Alarm Temperature Set: ON");
        isAlarm_temp = true;
      }
      else 
      {
      
        Serial.println("*Alarm Temperature Set: OFF");
        isAlarm_temp = false;
      }
     if(isAlarm_temp == true)
      {
        Serial.println("am gasit buton");
         yield();
        if(t >  20)
        { yield(); 
          Serial.println("acum cant");
          playTone(200, 1000);
         
        }
      }
    }
 if(subscription == &alarm_humidity)
    { yield();
      char *value2 = (char *) alarm_humidity.lastread;
      Serial.print(F("Got: "));
      Serial.println(value2);
      String message2 = String(value2);
      message2.trim();
      if(message2 == "ON")
      {
         
          Serial.println("*Alarm Humidity Set: ON");
          isAlarm_hum = true;
      }
      else 
      {
        
        Serial.println("*Alarm Humidity Set: OFF");
        isAlarm_hum = false;
      }
       if(isAlarm_hum == true)
      {
        yield();
        if(h > 50)
        {
          yield();
          playTone(100, 2000);
        }
      }
    }
 if(subscription == &alarm_UV)
    {
      yield();
      char *value3 = (char *) alarm_UV.lastread;
      Serial.print(F("Got: "));
      Serial.println(value3);
      String message3 = String(value3);
      message3.trim();
      if(message3 == "ON")
      {
        yield();
          Serial.println("*Alarm UV Set: ON");
          isAlarm_UV= true;
      }
      else 
      {
        yield();
        Serial.println("*Alarm UV Set: OFF");
        isAlarm_UV = false;
      }
       if(isAlarm_UV == true)
      {
        yield();
        if(u > 2)
        {
          yield();
          playTone(400, 2000);
        }
      }
    }
  }

 

  // Now we can publish stuff!
  Serial.print(F("\nSe incarca valoarea temperaturii pe ADAFRUIT --> "));
  Serial.print(t);
  if (! temperature.publish(t)) {
    Serial.println(F("\nFailed"));
  } else {
    Serial.println(F("\nOK!"));
  }

  Serial.print(F("\nSe incarca valoarea umiditatii pe ADAFRUIT --> "));
  Serial.print(h);
  if (! humidity.publish(h)) {
    Serial.println(F("\nFailed"));
  } else {
    Serial.println(F("\nOK!"));
  }

  Serial.print(F("\nSe incarca valoarea indicelui UV pe ADAFRUIT --> "));
  Serial.print(u);
  if (! UV_rate.publish(u)) {
    Serial.println(F("\nFailed"));
  } else {
    Serial.println(F("\nOK!"));
  }

  mqtt.disconnect();
  
/************ LCD-ul print******************/
  

  tft.setTextSize(2);
  tft.setTextWrap(true);
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(1,1);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_YELLOW);
  tft.print("Temp      ");
  tft.print(t);
  tft.print(" oC  ");
  tft.setTextColor(ST7735_WHITE);
  tft.println("Umiditate");
  tft.setTextColor(ST7735_RED);
  tft.print(h);
  tft.print("%    ");
  tft.setTextColor(ST7735_WHITE);
  tft.println("Lumina");
  tft.setTextColor(ST7735_MAGENTA);
  tft.println(u);
  }



// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
    return;
  }
  Serial.print("\nConnecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) = 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(50000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }      
  }
  Serial.println("MQTT Connected!");
}

void mqttConnect() {
  if(client.connected()) {
    return;
  }
    uint8_t dati = 3;
  if (!!!client.connected()) {
    Serial.print("Reconnecting MQTT client to "); 
    Serial.println(server);
    while(!!!client.connect(clientId, authMethod, token)) {
     Serial.println("Retrying MQTT connection to client in 5 seconds...");
      mqtt.disconnect();
       delay(50000);  // wait 5 seconds
       dati--;
     if (dati == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }      
  }
  Serial.println("MQTT Client Connected!");
}
}
 void playTone(int tone, int duration)
{
 for (long i = 0; i < duration * 1000L; i += tone * 2)
 {
 digitalWrite(speakerPin, HIGH);
 delayMicroseconds(tone);
 digitalWrite(speakerPin, LOW);
 delayMicroseconds(tone);

 }
}


void publish_t() {
  String payload = "{ \"temp\" : " ;
 payload += String(temp, DEC);
 payload += "}";
  Serial.print("Sending payload: "); Serial.println(payload);
  if (client.publish(ev_t, (char*) payload.c_str())) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
}

void publish_h() {
  String payload = "{ \"umid\" : ";
 payload += String(umid, DEC);
 payload += "}";
  Serial.print("Sending payload: "); Serial.println(payload);
  if (client.publish(ev_h, (char*) payload.c_str())) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
}

void publish_u() {
  String payload = "{ \"UV\" : " ;
  payload += String(UV, DEC);
  payload += "}";
  Serial.print("Sending payload: "); Serial.println(payload);
  if (client.publish(ev_u, (char*) payload.c_str())) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
}
