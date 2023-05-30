#include <Arduino.h>
extern "C" { 
  #include "user_interface.h"
}

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "config.h"
#include <ESPSocket.h>
#include <string>

#define REQUEST_FORMAT "api_key=%s&sensor=%s&temperature=%.2f&humidity=%.2f&pressure=%.2f&wind_speed=%.2f"

// Debugging object
struct Debug {
    std::string otaStart = "";
    std::string otaProgress = "";
    std::string otaEnd = "";
    std::string request = "";
    std::string responsePayload = "";
    const char* otaError = "";
    int anemometerRevs = 0;
};

/* GLOBAL VARIABLES */
AsyncWebServer GLOBAL_WEB_SERVER(80); // create web server on port 80
Adafruit_BME280 GLOBAL_BME_SENSOR; // create BME280 sensor object
Debug GLOBAL_DEBUG_OUTPUT; // create debug object
os_timer_t GLOBAL_WIND_SPEED; // create timer for wind speed
std::string GLOBAL_REQUEST_BODY = ""; // create request body
unsigned long GLOBAL_LAST_TIME = 0; // create last time variable for timer delay 
bool GLOBAL_RESTART_FLAG = false; // create restart flag
int GLOBAL_ANEMOMETER_REVOLUTIONS = 0; // revolutions
float GLOBAL_FINAL_WIND_SPEED = 0.0; // in m/s
int GLOBAL_ANEMOMETER_RPS; // revolutions per second
unsigned long GLOBAL_LAST_TIME_TWO = 0; // create last time variable for timer delay
float GLOBAL_ROTARY_TRAVEL = (CONFIG_ANEMOMETER_RADIUS * 2) * 3.1414; // rotary travel

/* GLOBAL BME VARIABLES */
float GLOBAL_TEMPERATURE, GLOBAL_HUMIDITY, GLOBAL_PRESSURE;

/**
 * @brief ISR for anemometer
 * @return ICACHE_RAM_ATTR 
 */
IRAM_ATTR void Anemometer_ISR()
{
  ETS_INTR_LOCK();
  GLOBAL_ANEMOMETER_REVOLUTIONS++;
  GLOBAL_ANEMOMETER_RPS = GLOBAL_ANEMOMETER_REVOLUTIONS;
  ETS_INTR_UNLOCK();
}

/**
 * @brief Callback function for anemometer
 * @param pArg 
 * @return void
 */
void calculateWS(void *pArg)
{
  int localAnemometerRevs = GLOBAL_ANEMOMETER_REVOLUTIONS;
  GLOBAL_ANEMOMETER_REVOLUTIONS = 0;

  // Use the local copy for calculations
  int localAnemometerRps = localAnemometerRevs / 5;
  GLOBAL_FINAL_WIND_SPEED = localAnemometerRps * GLOBAL_ROTARY_TRAVEL;
}

/**
 * @brief Five second timer for reading wind speed
 * @return void
 */
void user_init()
{
  os_timer_setfn(&GLOBAL_WIND_SPEED, calculateWS, NULL);
  os_timer_arm(&GLOBAL_WIND_SPEED, 5000, 1);
}

/**
 * @brief check if restart is required based on failed sensor readings
 * @param body 
 * @return bool true if restart is required
 */
bool checkIfRestartRequired(const std::string& body)
{
  char request_body[128]; // Buffer size depends on expected size of string
  float noReading = 0;
  snprintf(request_body, 
    sizeof(request_body),
    REQUEST_FORMAT,
    CONFIG_API_KEY.c_str(),
    CONFIG_SENSOR_NAME,
    noReading,
    noReading,
    noReading,
    GLOBAL_FINAL_WIND_SPEED
  );

  if (body == request_body) {
    return true;
  } 
  return false;
}

/**
 * @brief print the request string for debugging over the websocket
 * @return void 
 */
void printRequestString() 
{
  ESPSocket.print("\nPOST "); ESPSocket.print(CONFIG_URI); ESPSocket.println(" HTTP/1.1");
  ESPSocket.print("Host: "); ESPSocket.println(CONFIG_HOST);
  ESPSocket.print("Content-Type: "); ESPSocket.println(CONFIG_CONTENT_TYPE);
  ESPSocket.print("Content-Length: "); ESPSocket.println(GLOBAL_REQUEST_BODY.length());
  ESPSocket.println();
  ESPSocket.println(GLOBAL_REQUEST_BODY.c_str());
  ESPSocket.println("Connection: close");
  ESPSocket.println();
}

void onMsgHandler(uint8_t *data, size_t len) {
  ESPSocket.print("[");
  ESPSocket.print((int)millis());
  ESPSocket.print("] ");

  std::string action = "";
  for (size_t i = 0; i < len; i++) {
    action += char(data[i]);
  }

  // ESPSocket.println(action.c_str());

  if (action == CONFIG_ESP_REBOOT) {
    ESPSocket.println("Rebooting in 5 seconds, this page will reload...");
    ESPSocket.println("[:RELOAD:]");
    GLOBAL_RESTART_FLAG = true;
    return;
  }

  if (action == CONFIG_ESP_UPDATE) {
    ESPSocket.println("Processing update signal...");
    // update();
    return;
  }
}

/**
 * @brief Setup function, runs once on boot
 * @return void
 */
void setup()
{
  Serial.begin(CONFIG_BAUD_RATE);
  pinMode (2, OUTPUT);
  digitalWrite (2, HIGH);
  pinMode (CONFIG_REED_SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CONFIG_REED_SWITCH_PIN), Anemometer_ISR, RISING);
  user_init();

  // TODO - add error handling around calling begin on BME280 if not true then failed to init
  GLOBAL_BME_SENSOR.begin(0x76);
  GLOBAL_TEMPERATURE = GLOBAL_BME_SENSOR.readTemperature();
  GLOBAL_HUMIDITY = GLOBAL_BME_SENSOR.readHumidity();
  GLOBAL_PRESSURE = GLOBAL_BME_SENSOR.readPressure() / 100.0F;

  WiFi.begin(CONFIG_SSID, CONFIG_PASSWORD);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(3000);
    ESP.restart();
  }

  Serial.print("Connected to "); Serial.println(WiFi.localIP());
  ESPSocket.msgCallback(onMsgHandler);
  ESPSocket.begin(&GLOBAL_WEB_SERVER);
  GLOBAL_WEB_SERVER.begin();

  ArduinoOTA.setHostname(CONFIG_OTA_HOSTNAME);
  ArduinoOTA.setPassword(CONFIG_OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    GLOBAL_DEBUG_OUTPUT.otaStart.append("Starting to update " + std::string(type.c_str()));
  });

  ArduinoOTA.onEnd([]() {
    GLOBAL_DEBUG_OUTPUT.otaEnd = "\nEnd";
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    GLOBAL_DEBUG_OUTPUT.otaProgress = std::string(String(progress / (total / 100)).c_str());
  });

  ArduinoOTA.onError([](ota_error_t error) {
    ESPSocket.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      GLOBAL_DEBUG_OUTPUT.otaError = "Auth Failed";
    } else if (error == OTA_BEGIN_ERROR) {
      GLOBAL_DEBUG_OUTPUT.otaError = "Begin Failed";
    } else if (error == OTA_CONNECT_ERROR) {
      GLOBAL_DEBUG_OUTPUT.otaError = "Connect Failed";
    } else if (error == OTA_RECEIVE_ERROR) {
      GLOBAL_DEBUG_OUTPUT.otaError = "Receive Failed";
    } else if (error == OTA_END_ERROR) {
      GLOBAL_DEBUG_OUTPUT.otaError = "End Failed";
    }
  });
  ArduinoOTA.begin();
}

/**
 * @brief Main loop function, runs repeatedly
 * @return void
 */
void loop()
{
  ArduinoOTA.handle();
  GLOBAL_DEBUG_OUTPUT.anemometerRevs = GLOBAL_ANEMOMETER_REVOLUTIONS;
  yield();
  
  if (GLOBAL_PRESSURE == 0) {
    digitalWrite (2, LOW);
    if ((millis() - GLOBAL_LAST_TIME_TWO) > CONFIG_TIMER_DELAY_TWO) {
      digitalWrite (2, HIGH);
    }
    GLOBAL_LAST_TIME_TWO = millis();
  }
  
  if ((millis() - GLOBAL_LAST_TIME) > CONFIG_TIME_DELAY) { 
    GLOBAL_LAST_TIME = millis();
    WiFiClientSecure httpsClient; 
    
    if (httpsClient.setFingerprint(CONFIG_SERVER_FINGERPRINT)) {
      ESPSocket.println("Fingerprint set");
    }

    httpsClient.setTimeout(15000);
    delay(500);
    int retries = 0;

    while ((!httpsClient.connect(CONFIG_HOST, CONFIG_PORT)) && (retries < 30)) {
        delay(100);
        retries++;
    }

    if (retries == 30) {
      ESPSocket.println("Connection failed");
    } else {
      ESPSocket.println("Connected to " + String(CONFIG_HOST));  

    char request_body[128]; // Buffer size depends on expected size of string

    snprintf(request_body, 
      sizeof(request_body),
      REQUEST_FORMAT,
      CONFIG_API_KEY.c_str(),
      CONFIG_SENSOR_NAME,
      GLOBAL_BME_SENSOR.readTemperature(),
      GLOBAL_BME_SENSOR.readHumidity(),
      GLOBAL_BME_SENSOR.readPressure() / 100.0F,
      GLOBAL_FINAL_WIND_SPEED
    );

    GLOBAL_DEBUG_OUTPUT.request = GLOBAL_REQUEST_BODY = request_body;

    httpsClient.print("POST "); httpsClient.print(CONFIG_URI); httpsClient.println(" HTTP/1.1");
    httpsClient.print("Host: "); httpsClient.println(CONFIG_HOST);
    httpsClient.print("Content-Type: "); httpsClient.println(CONFIG_CONTENT_TYPE);
    httpsClient.print("Content-Length: "); httpsClient.println(GLOBAL_REQUEST_BODY.length());
    httpsClient.println();
    httpsClient.println(GLOBAL_REQUEST_BODY.c_str());
    httpsClient.println();

    if (CONFIG_DEBUG) {
      printRequestString();
      ESPSocket.println("request sent");
      ESPSocket.print("reply: ");
    }

      std::string line;
      while (httpsClient.available()) {        
        line.append(httpsClient.readStringUntil('\n').c_str());
        ESPSocket.println(line.c_str());
      }
      GLOBAL_DEBUG_OUTPUT.responsePayload = line;

      ESPSocket.println("closing connection");
      
     if (checkIfRestartRequired(GLOBAL_REQUEST_BODY)) {
       ESP.restart();
     }
    }
  }
}
