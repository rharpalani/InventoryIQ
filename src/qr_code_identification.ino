#include <SPI.h>

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiUdp.h>

#include <esp_now.h>

#include <FirebaseESP32.h>
#include <Arduino_JSON.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "quirc.h"

// creating a task handle
TaskHandle_t QRCodeReader_Task;


#define CAMERA_MODEL_AI_THINKER


/* ======================================== GPIO of camera models */
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22


// Firebase authenticated user
#define USER_EMAIL // deleted for now, fill in later
#define USER_PASSWORD // deleted for now, fill in later

// Firebase url
#define DATABASE_URL "server-d0dcc-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyCYG8xkf0IHb74zdDSXqXgrm7CQvXO1s4I"

/* ======================================== Variables declaration */
struct QRCodeData {
  bool valid;
  int dataType;
  uint8_t payload[1024];
  int payloadLen;
};

struct quirc *q = NULL;
uint8_t *image = NULL;
camera_fb_t *fb = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;
struct QRCodeData qrCodeData;
String QRCodeResult = "";

const char *ssid = "IllinoisNet_Guest";
// const char* ssid = "TP-Link_39DE";
const char *password = "74958306";


typedef struct struct_message {
  String QRCodeResultString;
} struct_message;

struct_message packet;

// instantiate firebase object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config_firebase;

// esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/* ======================================== */

// for ESP32 CAM
void pushComponent(String component) {
  if (Firebase.isTokenExpired()) {
    Firebase.refreshToken(&config_firebase);
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

/* ________________________________________________________________________________ VOID SETTUP() */
void setup() {
  // put your setup code here, to run once:

  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  /* ---------------------------------------- Init serial communication speed (baud rate). */
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  /* ---------------------------------------- */

  WiFi.begin(ssid);
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.println(WiFi.macAddress());

  /* ---------------------------------------- Camera configuration. */
  Serial.println("Start configuring and initializing the camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;


  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  Serial.println("Configure and initialize the camera successfully.");
  Serial.println();


  // pin tasks to core 0 for real-time tasks
  xTaskCreatePinnedToCore(
    QRCodeReader,        /* Task function. */
    "QRCodeReader_Task", /* name of task. */
    10000,               /* Stack size of task */
    NULL,                /* parameter of the task */
    1,                   /* priority of the task */
    &QRCodeReader_Task,  /* Task handle to keep track of created task */
    0);                  /* pin task to core 0 */
  

  // initialize database
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config_firebase.database_url = DATABASE_URL;
  config_firebase.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config_firebase.token_status_callback = tokenStatusCallback;
  config_firebase.max_token_generation_retry = 5;

  Firebase.begin(&config_firebase, &auth);
  Firebase.reconnectNetwork(true);
  Firebase.setMaxRetry(fbdo, 3);
  Firebase.setMaxErrorQueue(fbdo, 30);
  Firebase.enableClassicRequest(fbdo, true);
  fbdo.setBSSLBufferSize(4096, 1024);
}


void loop() {
  // put your main code here, to run repeatedly:
  delay(1);
}



void QRCodeReader(void *pvParameters) {

  Serial.println("QRCodeReader is ready.");
  Serial.print("QRCodeReader running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println();

  /* Loop to read QR Code in real time. */
  while (1) {
    q = quirc_new();
    if (q == NULL) {
      Serial.print("can't create quirc object\r\n");
      continue;
    }

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    quirc_resize(q, fb->width, fb->height);
    image = quirc_begin(q, NULL, NULL);
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);

    Serial.println("Identifying...");

    int count = quirc_count(q);
    if (count > 0) {
      quirc_extract(q, 0, &code);
      err = quirc_decode(&code, &data);

      if (err) {
        // Serial.println("Decoding FAILED");
        QRCodeResult = "Decoding FAILED";
      } else {
        Serial.printf("Decoding successful:\n");
        dumpData(&data);

        // Serial.printf("Const Char Payload: %s\n", (const char *)data.payload);

        packet.QRCodeResultString = String((const char *)data.payload);

        Serial.println("Part Identified: " + packet.QRCodeResultString);
        Serial.println("Updating Database!");

        pushComponent(packet.QRCodeResultString);

        delay(5000);
      }
    }

    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;
    quirc_destroy(q);
  }
}


void dumpData(const struct quirc_data *data) {
  QRCodeResult = (const char *)data->payload;
}
