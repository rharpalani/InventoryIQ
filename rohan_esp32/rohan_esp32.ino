#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Arduino_JSON.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <unordered_map>
#include <esp_now.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Arduino Pin Mappings
// LCD: 5V, GND, SDA=A4, SCL=A5
// RFID: 3V3, GND, RST=9, MISO=12, MOSI=11, SCK=13, SDA=10

// ESP32 Pin Mappings
#define BAUD            115200    // 9600 for Arduino

// RFID 
#define SCK_PIN         17
#define MISO_PIN        15
#define MOSI_PIN        16
#define SS_PIN          18  // aka SDA
#define RST_PIN         5

// LCD 
#define LCD_SDA_PIN     42
#define LCD_SCL_PIN     41
#define LCD_ADDY        0x27
#define LCD_CHARS       16
#define LCD_ROWS        2

// Wifi settings
#define WIFI_SSID "TP-Link_39DE"
#define WIFI_PASSWORD "74958306"

// Firebase url
#define DATABASE_URL "server-d0dcc-default-rtdb.firebaseio.com" 
#define API_KEY "AIzaSyCYG8xkf0IHb74zdDSXqXgrm7CQvXO1s4I"

// Firebase authenticated user
#define USER_EMAIL "rohanhh2@illinois.edu"
#define USER_PASSWORD "ece445"

// instantiate firebase object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// instantiate RFID
MFRC522 mfrc522(SS_PIN, RST_PIN); 

// instantiate LCD
LiquidCrystal_I2C lcd(LCD_ADDY, LCD_CHARS, LCD_ROWS); // set the LCD address to 0x27 for a 16 chars and 2 line display

// instantiate NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// user structures
struct UserInfo {
    std::string name;
};

std::unordered_map<std::string, UserInfo> authorizedUID = {
  {"dddb812d", {"Rohan"}},
  {"a28aff51", {"Rohan"}},
  {"44d473a237381", {"Rushil"}},
  {"467613a237381", {"Krish"}},
};

// updates database, takes current time and uid as parameters
// void pushFirebase(String currentTime, String uidValue) {
//   // refresh token
//   if (Firebase.isTokenExpired()){
//     Firebase.refreshToken(&config);
//     Serial.println("Refresh token");
//   }

//   if (Firebase.ready()) {
//     String path = "/esp32-test/test1";
//     FirebaseJson jsonData;
//     jsonData.set(currentTime, uidValue);
//     if (Firebase.pushJSON(fbdo, path, jsonData, 0)) {
//       Serial.println("database push successful");
//     } else {
//       Serial.println("database push failed");
//       Serial.println(fbdo.errorReason().c_str());
//     }
//   } else {
//     Serial.println("database not ready");
//     Serial.println(fbdo.errorReason().c_str());
//   }
// }

// updates database via borrowing or returning
void updateFirebase(String user, String component, String currentTime) {
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }

  if (Firebase.ready()) {
    // check if component on in list
    if (Firebase.get(fbdo, "inventory/in/" + component)) {
        // component in box, so student borrowing
        Serial.println("Initiate borrow");

        // checkout entry {component : true}
        FirebaseJson checkoutData;
        checkoutData.set(component, currentTime);

        // inventory entry {component : true}
        FirebaseJson jsonData;
        jsonData.set(component, true);

        // update student checkout record
        Firebase.updateNode(fbdo, "checkouts/" + user, checkoutData);

        // remove part from in record
        Firebase.deleteNode(fbdo, "inventory/in/" + component);

        // add part to out record
        Firebase.updateNode(fbdo, "inventory/out", jsonData);

        echoLCD(component, "Checked out!");

    } else if (Firebase.get(fbdo, "inventory/out/" + component)) {

      if (!(Firebase.get(fbdo, "checkouts/" + user + "/" + component))) {
        // not a component checked out to this user
        Serial.println("component checked out to another user");
        echoLCD("Checked out to", "another student!");
        return;
      }

      // component not in box, so student returning
      Serial.println("Initiate return");

      // database entry {component : true}
      FirebaseJson jsonData;
      jsonData.set(component, true);

      // update student checkout record
      Firebase.deleteNode(fbdo, "checkouts/" + user + "/" + component);

      // add part to in record
      Firebase.updateNode(fbdo, "inventory/in", jsonData);

      // remove part from out record
      Firebase.deleteNode(fbdo, "inventory/out/" + component);

      echoLCD(component, "Returned!");
    
    } else {
      Serial.println("component not recognized");
      echoLCD("Not recognized,", "scan card again!");
      return;
    }
  }
}

// use Firebase /users/authorized and /users/unauthorized to check if users can unlock box
bool checkAuthorization(String user) {
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }

  if (Firebase.ready()) {
    if (Firebase.get(fbdo, "users/authorized/" + user)) {
      // user is in authorized list
      return true;
    } else {
      // user is not in authorized list OR does not exist
      return false;
    }
  }
}

// echoes 2 lines to the LCD
void echoLCD(String line1, String line2) {
  lcd.clear();  // Clear the LCD
  lcd.setCursor(0, 0);  // Set cursor to the first row
  lcd.print(line1);  // display line1
  lcd.setCursor(0, 1);  // Set cursor to the first row
  lcd.print(line2);  // display line2
}

// esp32 peer communication
uint8_t broadcastAddress[] = {0x08, 0xD1, 0xF9, 0x97, 0xA9, 0xA0};  // 08:D1:F9:97:A9:A0 -- camera MAC address
esp_now_peer_info_t peerInfo;

// message structure
typedef struct struct_message {
  String text;
} struct_message;
struct_message receiving_data;

// callback (interrupt) function executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receiving_data, incomingData, sizeof(receiving_data));
  Serial.print("Data received: ");
  Serial.println(len);
  Serial.print("String Value: ");
  Serial.println(receiving_data.text);
  Serial.println();
}

void setup() {
  // initialize serial communications with the PC
	Serial.begin(BAUD);		
	while (!Serial);
  delay(200);

  // initialize LCD
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN); // SDA, SCL
  lcd.init(); //initialize the lcd
  lcd.backlight(); //open the backlight 

  // initialize RFID module
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN); // Initialize SPI bus with specific pins
	mfrc522.PCD_Init();		// Init MFRC522

  // initialize wifi
  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);    // COMMENT FOR APT
  WiFi.begin("IllinoisNet_Guest");    // COMMENT FOR SCHOOL
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

  // get MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // initialize ESP-NOW communication
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // initialize esp-now callback function
  esp_now_register_recv_cb(OnDataRecv);

  // initialize time client
  timeClient.begin();
  timeClient.setTimeOffset(-21600);   // CST is GMT -5

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

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
}

void loop() {

  echoLCD("Tap iCard!", "");
  // spin if no new card present on reader
  while (! mfrc522.PICC_IsNewCardPresent());
  // if ( ! mfrc522.PICC_IsNewCardPresent()) {
	// 	return;
	// }

	// select one card
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		return;
	}

  // set camera data to empty
  receiving_data.text = "";

  // extract UID from struct
  String uidValue = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidValue += String(mfrc522.uid.uidByte[i], HEX);
  }
  mfrc522.PICC_HaltA();

  // print UID to serial terminal
  Serial.print("UID: ");
  Serial.println(uidValue);
  Serial.println();

  // check if UID is authorized
  // std::string uid = std::string(uidValue.c_str());
  // if (authorizedUID.find(uid) != authorizedUID.end()) {
  //   UserInfo& userInfo = authorizedUID[uid];
    
  //   if (userInfo.authorized) {
  //     echoLCD(String((userInfo.name).c_str()), "Scan Component!");
  //     delay(200);
  //   } else {
  //     echoLCD(String((userInfo.name).c_str()), "Not Authorized!");
  //     delay(200);
  //     return;   // terminate sequence if user not authorized
  //   }
  // } else {
  //   Serial.println("user not found");
  //   echoLCD("User not found!", "");
  //   return;
  // }

  // check user authorization from database
  std::string uid = std::string(uidValue.c_str());
  UserInfo& userInfo = authorizedUID[uid];
  if (checkAuthorization(String((userInfo.name).c_str()))) {
    Serial.println("authorized!");
    // UNLOCK BOX HERE
    echoLCD(String((userInfo.name).c_str()), "Scan Component!");
    delay(500);
  } else {
    Serial.println("unauthorized access");
    echoLCD(String((userInfo.name).c_str()), "Not Authorized!");
    delay(5000);
    return;   // terminate sequence if user not authorized
  }

  receiving_data.text = "9V battery";   // TEST VAL
  // spin until component data received
  while (receiving_data.text.isEmpty()) {
    Serial.println("waiting for component");
    delay(10000);
  }
  Serial.println("received data");

  // get current time
  timeClient.update();
  String currentTime = timeClient.getFormattedTime();

  // push to firebase
  // pushFirebase(currentTime, String((userInfo.name).c_str())); // or uidValue
  updateFirebase(String((userInfo.name).c_str()), receiving_data.text, currentTime);
  delay(5000);
  lcd.clear();
}
