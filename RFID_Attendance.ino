#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> // Install author: tzapu

#define SS_PIN D4  // SDA / SS pin
#define RST_PIN D1  // RST pin
#define BUZZER_PIN D8 // Buzzer pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const int flashButtonPin = 0; 
const char* serverUrl = "http://192.168.1.21/rfiddemo/getUID.php";

// Onboard LED
#define ON_Board_LED 2  // Onboard LED

void setup() {  
  Serial.begin(115200);
  pinMode(flashButtonPin, INPUT);
  pinMode(ON_Board_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  SPI.begin();  // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 card
  Serial.println("MFRC522 Initialized");

  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("Wifi Re-Configure")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.reset();
    delay(1000);
  }
  Serial.println("Connected to Wi-Fi!");

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(ON_Board_LED, LOW);
    delay(250);
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
  }
  digitalWrite(ON_Board_LED, HIGH); // Turn off onboard LED when connected
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Please tag a card or keychain to see the UID!");
  Serial.println("");
}

void loop() {
  if (digitalRead(flashButtonPin) == LOW) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.reset();
    Serial.println("WiFi settings reset");
    delay(1000); 
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Check for RFID card
    if (!mfrc522.PICC_IsNewCardPresent()) {
      // Serial.println("No card present"); // Debugging line
      return;
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Card present but unable to read serial"); // Debugging line
      return;
    }

    // Read card UID
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidStr += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      uidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    // Print UID to Serial Monitor
    Serial.print("Card UID: ");
    Serial.println(uidStr);

    // Trigger buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

    // Send UID to server
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverUrl);  // Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("uid=" + uidStr);  // Send the request with the uid parameter

    // Check the returning code
    if (httpCode > 0) {
      String payload = http.getString();  // Get the response payload
      Serial.println("HTTP Response code: " + String(httpCode));
      Serial.println("Response payload: " + payload);
    } else {
      Serial.println("Error on HTTP request");
    }

    http.end();  // Close connection

    delay(1000);  // Delay before next read
  } else {
    Serial.println("WiFi not connected");
  }
}