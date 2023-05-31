# 🌦️ Advanced Weather Station using ESP8266 & BME280 🌦️
Welcome to our Advanced Weather Station's GitHub repository! This project leverages the power of the ESP8266 - a highly affordable Wi-Fi microchip with built-in TCP/IP stack and microcontroller capabilities - in combination with a BME280 sensor for environmental data and a reed switch anemometer for wind speed data. This real-time weather monitoring system delivers detailed meteorological data directly to your device.

## 🎯 Features
Real-time wind speed monitoring: The reed switch anemometer provides accurate wind speed data, with readings updated in real-time using interrupt-driven measurements.
```cpp
attachInterrupt(digitalPinToInterrupt(CONFIG_REED_SWITCH_PIN), countAnemometerRevolutions, FALLING);
```

Environmental data collection: BME280 sensor tracks barometric pressure, humidity, and temperature, providing a comprehensive picture of local weather conditions.

```cpp
bme.begin(CONFIG_BME_ADDRESS);
```

Over-The-Air (OTA) updates: Deploy firmware updates easily over the internet with the built-in OTA functionalities of the ESP8266.
WebSocket Debugging: Monitor the performance of your application in real-time with our debugging support through WebSocket.
Configurable: Tailor the system according to your needs with our easy-to-use config file.
## 🛠️ Installation & Setup
To get started, you will need the Arduino IDE installed on your computer to upload the sketch to the ESP8266.

Clone this repository to your local machine.
Open the .ino file in your Arduino IDE.
Set the IDE to the correct board (ESP8266) and port.
Customize the provided config.example.h file with your configurations, such as Wi-Fi and OTA details, and rename it to config.h.
```cpp
const char* CONFIG_SSID = "REPLACE_WITH_SSID";
const char* CONFIG_PASSWORD = "REPLACE_WITH_PASSWORD";
```
Compile and upload the code to your ESP8266.
Enjoy your advanced weather monitoring!
## 📈 Usage
Once uploaded and running, the ESP8266 will continuously collect wind speed and environmental data, sending it to your configured endpoint. A WebSocket is available for debugging, and you can also push firmware updates over the air.

The core functionalities are encapsulated in functions like calculateWS(), which measures the wind speed, and BME280 read methods for environmental data.

```cpp
void calculateWS(void *pArg)
{
  // Temporarily disable interrupts
  noInterrupts();
  int localAnemometerRevs = GLOBAL_ANEMOMETER_REVOLUTIONS;
  GLOBAL_ANEMOMETER_REVOLUTIONS = 0;
  // Re-enable interrupts
  interrupts();

  // Use the local copy for calculations
  int localAnemometerRps = localAnemometerRevs / 5;
  GLOBAL_FINAL_WIND_SPEED = localAnemometerRps * GLOBAL_ROTARY_TRAVEL;
}

// Reading BME280 data
float temperature = bme.readTemperature();
float humidity = bme.readHumidity();
float pressure = bme.readPressure() / 100.0F;
```
## 🤝 Contributing
We warmly welcome contributions! If you have a suggestion, bug report, or feature request, please open an issue on GitHub. If you wish to contribute code, please open a pull request and include a detailed description of your changes.

## 🙏 Acknowledgements
Our heartfelt thanks to the entire open-source community for making projects like this possible.

## 📄 License
This project is licensed under the MIT License - see the LICENSE file for details.

## Notes on OTA errors:

OTA_AUTH_ERROR: This error occurs when the authentication for OTA update fails. It means that the provided password for the OTA update is incorrect or missing. Make sure that you have the correct password configured on both the Arduino board and the client application attempting the OTA update.

OTA_BEGIN_ERROR: This error indicates a failure in starting the OTA update process. It can occur due to network connectivity issues or insufficient memory on the Arduino board. Ensure that your Arduino board is properly connected to the network and has enough memory available for the OTA update.

OTA_CONNECT_ERROR: This error occurs when the OTA update process fails to establish a connection with the client. It can happen due to network connectivity problems or if the client application is unable to reach the Arduino board. Check your network settings and ensure that the client and board are on the same network.

OTA_RECEIVE_ERROR: This error suggests that there was an issue with receiving the firmware during the OTA update process. It can occur due to network interruptions or problems with the firmware transfer. Verify that the network connection is stable and consider reattempting the OTA update.

OTA_END_ERROR: This error indicates a failure in completing the OTA update process. It can happen due to issues with finalizing the update or verifying the firmware on the Arduino board. Check the integrity of the uploaded firmware and consider reattempting the OTA update.