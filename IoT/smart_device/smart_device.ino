#include "DHT.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "time.h"

//VARIABLES TO OBTAIN TIME
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

//HARDCODED VALUES
#define SAMPLE_FREQUENCY_ERROR 500//if some error in configuration retrieval occures the sample frequency will be set to this value
#define GPS_Lat 44.490
#define GPS_Long 11.313
#define ID 1

//PIN USED FOR DHT-11 and MQ-2
#define DHT_PIN 14
#define DHT_TYPE DHT11
#define MQ2_PIN 35

//WIFI INFO
#define WIFI_SSID "****" //Put your WiFi SSID here
#define WIFI_PASS "****" //Put your WiFi password here

//MQTT Variables
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char* topic = "esp32/IoT/sensor_values";

//SERVER CONFS
const char* server_conf = "http://192.168.8.110:1025/get_config"; //conf retrieval via HTTP
const char* server_values = "http://192.168.8.110:1025/values"; //conf retrieval via HTTP

DHT dht(DHT_PIN, DHT_TYPE);
WiFiMulti WiFiMulti;
String conf;

int MAX_GAS_VALUE = -1;
int MIN_GAS_VALUE = -1;
int SAMPLE_FREQUENCY = -1;
int TECHNOLOGY = -1;

//AQI variables
int cont = 0;
int AQI_value = -1;
float gas_measure[4];


void setup() {
  Serial.begin(115200);
  delay(10);

  dht.begin();
  Serial.println("Connecting to WiFi...");
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASS);

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(500);

  Serial.println(WiFi.localIP());
}


void loop() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  char time_second[3];
  char time_minute[3];
  char time_hour[3];
  strftime(time_second,3, "%S", &timeinfo);
  strftime(time_minute,3, "%M", &timeinfo);
  strftime(time_hour,3, "%H", &timeinfo);


  
  //Sensor value measurements***********************************************
  float h = dht.readHumidity(); //reads the umidity from dht-11
  float t = dht.readTemperature(); //reads the temperature from dht-11
  float g = analogRead(MQ2_PIN); //reads the gas from MQ-2
  //*********************************************************************

  StaticJsonDocument<192> out;
  StaticJsonDocument<192> conf;
  if (WiFi.status() == WL_CONNECTED) {

    //Configuration retrieval via HTTP GET Request *************************
    String conf_req = httpGETRequest(server_conf);
    DeserializationError error = deserializeJson(conf, conf_req);
    //error during configuration retrieval
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    //configuration setting
    SAMPLE_FREQUENCY = conf["SAMPLE_FREQUENCY"];
    MIN_GAS_VALUE = conf["MIN_GAS_VALUE"];
    MAX_GAS_VALUE = conf["MAX_GAS_VALUE"];
    TECHNOLOGY = conf["TECHNOLOGY"];
    //*********************************************************************

    //AQI value computation (Simple Moving Average)************************
    if (AQI_value > -1 || cont == 5) {
      int avg = (gas_measure[0] + gas_measure[1] + gas_measure[2] + gas_measure[3] + gas_measure[4]) / 5 ;
      cont = cont % 5;
      gas_measure[cont % 5] = g;

      if (avg >= MAX_GAS_VALUE) {
        AQI_value = 0;
      }
      else if ( avg < MIN_GAS_VALUE) {
        AQI_value = 2;
      }
      else {
        AQI_value = 1;
      }
      cont++;
    }
    else {
      //measuring the first 5 values
      gas_measure[cont % 5] = g;
      cont++;
    }
    //*********************************************************************

    //OUTPUT SENSOR VALUES AND INFORMATIONS *******************************
    out["humidity"] = h;
    out["temperature"] = t;
    out["gas"] = g;
    out["wifi"] = WiFi.RSSI();
    out["AQI"] = AQI_value;
    out["ID"] = ID;
    out["GPS_Latitude"] = GPS_Lat;
    out["GPS_Longitude"] = GPS_Long;
    out["second"] = time_second;
    out["minute"] = time_minute;
    out["hour"] = time_hour;
    //Sending via HTTP Post the measured values
    String json_out;
    serializeJson(out, json_out);
    //*********************************************************************
    if (TECHNOLOGY == -1) {
      Serial.println("Error in retrieving technology to send values");
      return;
    }
    else if (TECHNOLOGY == 0) {
      Serial.println("Using HTTP to send values !");
      httpPOSTRequest(server_values, json_out);
    } else {
      Serial.println("Using MQTT to send values !");
      mqtt_Handler(mqttServer, json_out);
    }


    //DELAY****************************************************************
    if (SAMPLE_FREQUENCY <= 0) {
      delay(SAMPLE_FREQUENCY_ERROR); //if no configuration was retrieved delay by default value (1m)
      Serial.println("Something went wrong while getting configuration values !");
      return;
    }
    else {
      delay(SAMPLE_FREQUENCY); //Delay of acquisition of sensor values
    }
  }
  //*********************************************************************
}





//HTTP GET REQUEST IMPLEMENTATION
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // Send HTTP GET request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

//HTTP POST REQUEST IMPLEMENTATION
void httpPOSTRequest(const char* serverName, String json_out) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  //NB: If I do not add the header then it won't send the json
  http.addHeader("Content-Type", "application/json");
  // Send HTTP POST request
  int httpResponseCode = http.POST(json_out);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

//MQTT CONNECTION SETUP
void mqtt_Handler(const char* server, String json_out) {
  WiFiClient espClient;
  PubSubClient mqttClient(espClient);
  mqttClient.setServer(mqttServer, mqttPort);

  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");

    if (mqttClient.connect("ESP32Client")) { //, mqttUser, mqttPassword if necessary to login

      Serial.println("connected");

    } else {

      Serial.print("failed with state ");
      Serial.print(mqttClient.state());
    }
  }

  if (mqttClient.publish(topic, toCharArray(json_out)) == true) {
    Serial.println("Success sending message");
  } else {
    Serial.println("Error sending message");
  }
  Serial.println("-------------");
}

char* toCharArray(String str) {
  return &str[0];
}
