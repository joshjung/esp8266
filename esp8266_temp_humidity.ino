#include <SimpleDHT.h>

#include "ESP8266WiFi.h"
#include "ESP8266HTTPClient.h"
#include "WiFiClient.h"
#include <ArduinoJson.h>

const char DHT11_PIN = 5; // D1 (GPIO5) pin

const int FREQUENCY_MS = 60000; // Delay between sends / reads

//---------------------------------------------------------
// Important! Use 2.4ghz with WPA Personal and TKIP+AES
//---------------------------------------------------------
const char* ssid = "[your wifi ssid here]";
const char* password = "[wifi password here]";

String API_HOST = "[strapi API url]";
String API_CONNECT_URL = "[connect endpoint]";
String API_DEVICE_EVENTS_URL = "[device event endpoint]";
String apiSessionToken = "[unset]";
bool apiConnected = false;

SimpleDHT11 dht11;

WiFiClientSecure client;
HTTPClient http;

void connectWIFI(const char* ssid, const char* password) {
  Serial.println("Connecting to wifi...");

  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.printf("Still connecting... WiFi.status(): %d\n", WiFi.status());
  }

  Serial.print("Connected to WiFi as ");
  Serial.println(WiFi.localIP());
}

void connectAPI() {
  if (WiFi.status() == WL_CONNECTED) {
    String macAddress = WiFi.macAddress();

    String message = String("Connecting as mac ") + macAddress + String(" to API...");
    Serial.println(message);

    // SSL (ignore for now)
    client.setInsecure();
    client.connect(API_HOST, 443);

    http.begin(client, API_CONNECT_URL.c_str());
    http.addHeader("Content-Type", "application/json");
    
    String formData = String("{\"mac_address\":\"") 
      + macAddress 
      + String("\",\"requested_job_codes\": [\"thermometer\", \"hygrometer\"]}");

    int responseStatusCode = http.POST(formData);
    StaticJsonDocument<512> responseJsonDoc;
    String responseJsonStr = http.getString();

    Serial.println("POST device/connect status: ");
    Serial.println(responseStatusCode);
    Serial.println(responseJsonStr);
    
    DeserializationError error = deserializeJson(responseJsonDoc, responseJsonStr);

    if (error) {
      Serial.print("Deserialize response error:");
      Serial.println(error.f_str());
      return;
    }

    if (responseStatusCode == 200) {
      apiSessionToken = String(responseJsonDoc["session"]["token"]);
      Serial.println("----------------------------------");
      Serial.println(String("Token: ") + apiSessionToken);
      Serial.println("----------------------------------");
      apiConnected = true;
    }
  } else {
    Serial.println("Fatal error: wifi issues after connection.");
  }
}

void setup() {
  Serial.begin(115200);

  Serial.println("Connecting to wifi...");

  connectWIFI(ssid, password);
  connectAPI();

  Serial.println("WIFI and API connect successfully.");
  
  pinMode(DHT11_PIN, INPUT);

  Serial.println("Listening on digital pin for DHT...");
}

void sendInteger(const char *eventType, const char *field, int value) {
  if (WiFi.status() == WL_CONNECTED && apiConnected) {
    http.begin(client, API_DEVICE_EVENTS_URL.c_str());
    http.addHeader("Content-Type", "application/json");

    String jsonData = String("{\"session_token\":\"") // { session_token: "
      + apiSessionToken                               //      asdf-asdf-asdf-asdf
      + String("\",\"type\":\"")                      //   ", "type": "
      + String(eventType)                             //      [eventType]
      + String("\",\"data\":{\"")                     //   ", "data": { "
      + String(field)                                 //      [field]
      + String("\":")                                 //   ":
      + value                                         //      [value]
      + String("}}");                                 // }}

    Serial.println(jsonData.c_str());
    http.POST(jsonData);
    String responseStr = http.getString();

    Serial.println(String(eventType) + " response: ");
    Serial.println(responseStr);
  }
}

void loop() {
  byte temperature = 0;
  byte humidity = 0;
  byte data[40] = {0};

  if (dht11.read(DHT11_PIN, &temperature, &humidity, data)) {
    Serial.println("Read DHT11 Failed");
    delay(FREQUENCY_MS);
    return;
  }

  Serial.print((int)temperature); Serial.print(" C, ");
  Serial.print((int)humidity); Serial.print(" %\n");

  sendInteger("temperature", "temperature", (int)temperature);
  sendInteger("humidity", "humidity", (int)humidity);

  delay(FREQUENCY_MS);
}
