#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Arduino_JSON.h> 
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Firebase authenticated user
#define USER_EMAIL "krishnnaik9@gmail.com"
#define USER_PASSWORD "Bayernmunich1!"

// Firebase url
#define DATABASE_URL "server-d0dcc-default-rtdb.firebaseio.com" 
#define API_KEY "AIzaSyCYG8xkf0IHb74zdDSXqXgrm7CQvXO1s4I"

// Wifi settings
#define WIFI_SSID "TP-Link_39DE"
#define WIFI_PASSWORD "74958306"

// instantiate firebase object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// for ESP32 CAM
void pushComponent(String component) {
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }

  FirebaseJson jsonData;
  jsonData.set("component", component);

  if (Firebase.ready()) {
    Firebase.updateNode(fbdo, "current", jsonData);
    Serial.println("updated firebase");
    return;
  }
} 

void setup() {
  // initialize serial communications with the PC
	Serial.begin(115200);		
	while (!Serial);
  delay(200);

  // initialize wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);    // COMMENT FOR APT
  // WiFi.begin("IllinoisNet_Guest");    // COMMENT FOR SCHOOL
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
      Serial.print(".");
      delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // initialize database
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  Firebase.setMaxRetry(fbdo, 3);
  Firebase.setMaxErrorQueue(fbdo, 30); 
  Firebase.enableClassicRequest(fbdo, true);
  fbdo.setBSSLBufferSize(4096, 1024);

  pushComponent("krish");
}

void loop() {

}
