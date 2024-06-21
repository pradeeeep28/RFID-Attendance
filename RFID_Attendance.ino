#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SD.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SS_PIN D4      // SDA / SS pin
#define RST_PIN D1     // RST pin
#define BUZZER_PIN D8  // Buzzer pin
#define SD_CS_PIN D2   // SD card CS pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

const int flashButtonPin = 0;
const char* serverUrl = "http://192.168.1.21/rfiddemo/getUID.php";

// Onboard LED
#define ON_Board_LED 2  // Onboard LED
#define LOG_FILE "/uid_log.txt" // Log file on SD card

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // NTP server, time offset (19800 for IST), update interval (60000 ms)

void setup() {
  Serial.begin(115200);
  pinMode(flashButtonPin, INPUT);
  pinMode(ON_Board_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize SPI and MFRC522
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("MFRC522 Initialized");

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Initialization of SD card failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Read Wi-Fi credentials from file
  File configFile = SD.open("/wifi_config.txt");
  if (!configFile) {
    Serial.println("Failed to open wifi_config.txt file!");
    return;
  }

  String ssid = configFile.readStringUntil('\n');
  ssid.trim();
  String password = configFile.readStringUntil('\n');
  password.trim();
  configFile.close();

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);

  // Connect to Wi-Fi
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(ON_Board_LED, LOW);
    delay(250);
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
  }
  digitalWrite(ON_Board_LED, HIGH);  // Turn off onboard LED when connected
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize NTP client
  timeClient.begin();
  Serial.println("NTP client initialized.");

  // Fetch the current time
  while (!timeClient.update()) {
    Serial.println("Fetching NTP time...");
    delay(1000); // Retry delay
  }
  Serial.println("NTP time fetched successfully.");

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
    timeClient.update(); // Update the NTP client to get the latest time

    // Check for RFID card
    if (!mfrc522.PICC_IsNewCardPresent()) {
      return;
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Card present but unable to read serial");  // Debugging line
      return;
    }

    // Read card UID
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uidStr += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      uidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    // Get current timestamp
    unsigned long epochTime = timeClient.getEpochTime();
    String formattedTime = timeClient.getFormattedTime(); // Format: HH:MM:SS
    // Convert epoch time to date
    int year = 1970 + epochTime / 31556926;
    int month = (epochTime % 31556926) / 2629743 + 1;
    int day = ((epochTime % 31556926) % 2629743) / 86400 + 1;
    String formattedDate = String(year) + "-" + String(month < 10 ? "0" : "") + String(month) + "-" + String(day < 10 ? "0" : "") + String(day);
    String timestamp = formattedDate + " " + formattedTime; // Format: YYYY-MM-DD HH:MM:SS

    // Print UID and timestamp to Serial Monitor
    Serial.print("Card UID: ");
    Serial.println(uidStr);
    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    // Store UID and timestamp to SD card
    File logFile = SD.open(LOG_FILE, FILE_WRITE);
    if (logFile) {
      logFile.print("UID: ");
      logFile.print(uidStr);
      logFile.print(" Timestamp: ");
      logFile.println(timestamp);
      logFile.close();
      Serial.println("UID and timestamp stored to SD card");
    } else {
      Serial.println("Error opening log file on SD card");
    }

    // Trigger buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

    // Send UID and timestamp to server
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverUrl);  // Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("uid=" + uidStr + "&timestamp=" + timestamp);  // Send the request with the uid and timestamp parameters

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