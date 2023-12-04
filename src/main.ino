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
// #define SCK_PIN         17
// #define MISO_PIN        15
// #define MOSI_PIN        16
// #define SS_PIN          18  // aka SDA
// #define RST_PIN         5

#define SCK_PIN         10
#define MISO_PIN        11
#define MOSI_PIN        48
#define SS_PIN          7  // aka SDa / CS
#define RST_PIN         47

// LCD 
// #define LCD_SDA_PIN     42
// #define LCD_SCL_PIN     41

#define LCD_SDA_PIN     5
#define LCD_SCL_PIN     4

#define LCD_ADDY        0x27
#define LCD_CHARS       16
#define LCD_ROWS        2

// RELAY
// #define RELAY_PORT      37

#define RELAY_PORT      36

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

// gets current component sent by camera
String getComponent() {
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }

  if (Firebase.ready()) {
    // Firebase stringData
    if (Firebase.getString(fbdo, "current/component")) {
      const char *val = fbdo.to<const char *>();
      Serial.println("read component from camera");
      Serial.println(String(val));
      return String(val);
    } else {
      return ""; 
    }
  }
}

// deletes old component sent by camera
void deleteComponent() {
  if (Firebase.isTokenExpired()){
    Firebase.refreshToken(&config);
    Serial.println("Refresh token");
  }

  if (Firebase.ready()) {
    Firebase.deleteNode(fbdo, "current/component");
    return;
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

// get current date + time for firebase database
String getDate() {
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  String currentTime = timeClient.getFormattedTime();
  return currentDate + " " + currentTime;
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

  // initialize relay
  pinMode(RELAY_PORT, OUTPUT);
  digitalWrite(RELAY_PORT, LOW);     // lock box

  // initialize wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);    // COMMENT FOR APT
  // WiFi.begin("IllinoisNet_Guest");    // COMMENT FOR SCHOOL
  // WiFi.begin("gateway", "makaha95");    // COMMENT FOR HOME
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

  echoLCD("WIFI", "connected!");
  delay(2000);

  // initialize ESP-NOW communication
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

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

  // spin if no new card present on reader
  echoLCD("Tap iCard!", "");
  while (! mfrc522.PICC_IsNewCardPresent());

	// select one card
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		return;
	}

  // set camera data to empty
  deleteComponent();

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

  // check user authorization from database
  std::string uid = std::string(uidValue.c_str());
  UserInfo& userInfo = authorizedUID[uid];
  if (checkAuthorization(String((userInfo.name).c_str()))) {
    Serial.println("authorized!");
    echoLCD("Hello,", String((userInfo.name).c_str()));
    delay(2000);
    echoLCD("Box unlocked,", "scan component!");
    digitalWrite(RELAY_PORT, HIGH);     // unlock box
  } else {
    Serial.println("unauthorized access");
    echoLCD(String((userInfo.name).c_str()), "Not Authorized!");
    delay(3000);
    return;   // terminate sequence if user not authorized
  }

  // get camera component from Firebase
  while (getComponent().isEmpty()) {
    Serial.println("waiting for component");
  }
  Serial.println("component seen!");

  // push to firebase
  updateFirebase(String((userInfo.name).c_str()), getComponent(), getDate());
  delay(5000);
  digitalWrite(RELAY_PORT, LOW);     // lock box
  lcd.clear();
}
