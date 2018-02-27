#include <WiFi.h>

#include <DNSServer.h>
#include <WebServer.h>
#include "WiFiManager.h"
#include <PubSubClient.h>

#include "SSD1306.h"
#include <wire.h>
#include "DHT.h"
#include <BH1750.h>

#include <NTPtimeESP.h>

// Time
NTPtime NTPde("de.pool.ntp.org");
strDateTime dateTime;

// OLED
SSD1306 display(0x3c, 5, 4);

// Temperature and humidity
#define DHT_TYPE DHT22
#define DHT_PIN 15
float t_c;
float h;

DHT dht(DHT_PIN, DHT_TYPE);

// Light meter
// !! MUST make bugfix in BH1750_LIB: Wire.Reset(); if NOT two bytes
// are available yet, for read (e.g. while still starting the sensor)
// => without this bugfix, reading the value might tear down I2C forever
// which even blocks the OLED from updates (this uses I2C, too).
#define BH1750_DEBUG;
BH1750 lightMeter;
uint16_t lux = 0;

// MQTT

const char *mqtt_server = "192.168.1.111";
const int mqtt_port = 1883;
const char *mqtt_user = "pi";
const char *mqtt_password = "password";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void displayStatusLine(String message)
{
    Serial.println(message);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, message);
    display.display();
}

/* Wifi */

void configModeCallback(WiFiManager *myWiFiManager)
{
    String ip = WiFi.softAPIP().toString();
    String ssid = String(myWiFiManager->getConfigPortalSSID());
    Serial.println("Entered configuration mode");
    Serial.println(ip);
    Serial.println(ssid);
    Serial.println("Config portal is active");

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "!! NOT CONNECTED !!");
    display.drawString(0, 20, ssid);
    display.drawString(0, 40, ip);
    display.display();
}

void setupWifi()
{
    //Local initialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset settings - for testing
    //wifiManager.resetSettings();

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    // Timout of 5 minutes for config mode
    wifiManager.setConfigPortalTimeout(300);

    displayStatusLine("Search WIFI..");

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect())
    {
        displayStatusLine("NO WIFI!");
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(1000);
    }

    //if you get here you have connected to the WiFi
    displayStatusLine("WIFI connected");
}

void setupMQTT()
{
    Serial.println("Connecting MQTT server " + String(mqtt_server) + ":" + String(mqtt_port) + "...");
    mqttClient.setServer(mqtt_server, mqtt_port);
    if (mqttClient.connect("ESP32_THL_Sensor"))
    {
        Serial.println("MQTT server connected!");
    }
    else
    {
        Serial.println("Cannot connect MQTT server: " + mqttClient.state());
    }
}

void setupSensors()
{
    dht.begin();
    
    if (!lightMeter.begin())
    {
        displayStatusLine("GY-30 Init ERR");
    }
}

void setupClock()
{
    Serial.println("Setup clock...");
    int numRetries = 0;
retry:
    if (numRetries < 3)
    {
        // first parm TimeZone (UTC+1 = germany), second parm daylightSaving
        dateTime = NTPde.getNTPtime(1.0, 1);
        if (dateTime.valid)
        {
            NTPde.printDateTime(dateTime);
        }
        else
        {
            delay(1000);
            numRetries++;
            goto retry;
        }
    }
    else
    {
        Serial.println("Could not get time.");
    }
}

void setup()
{
    Serial.begin(115200);
    display.init();

    setupWifi();
    setupMQTT();
    setupClock();
    setupSensors();
}

void readValues()
{
    h = dht.readHumidity();
    t_c = dht.readTemperature();
    
    if (isnan(h) || isnan(t_c))
    {
        Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
        Serial.print(t_c);
        Serial.print("°C ");
        Serial.print(h);
        Serial.println("%");
    }

    lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx ");
}

void displayValues()
{
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, WiFi.localIP().toString() + " (" + WiFi.RSSI() + " RSSI)");

    String t_h;
    if (!isnan(t_c))
    {
        t_h = String(t_c) + "°C";
    }
    else
    {
        t_h = "N/A °C";
    }
    t_h += " ";
    if (!isnan(h))
    {
        t_h += String(h) + "%";
    }
    else
    {
        t_h += "N/A %";
    }
    Serial.println(t_h);
    display.drawString(0, 20, t_h);
    display.drawString(0, 40, String(lux) + " Lux");
    display.display();
}

void publishValues()
{
    if (!mqttClient.connected()){
        Serial.println("Lost connection to MQTT-Server .. try to reconnect");
        setupMQTT();
        if (!mqttClient.connected()){
            Serial.println("Reconnect to MQTT-Server failed.");
            return;
        }
    }   
    if (!isnan(t_c))
    {
        mqttClient.publish("esp32/temperature", String(t_c).c_str());
    }
    if (!isnan(h)){
        mqttClient.publish("esp32/humidity", String(h).c_str());
    }
    
    if (lux < 10000){
        mqttClient.publish("esp32/lux", String(lux).c_str());
    }
   
    mqttClient.publish("esp32/wifi-rssi", String(WiFi.RSSI()).c_str());
    
    mqttClient.loop();
}

void checkI2CBus()
{
    byte error, address;
    int nDevices;
    Serial.println();
    Serial.println("I2C Scanning...");
    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        // The i2c scanner uses the return value of Write.endTransmisstion to see if a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            Serial.println(" !");
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("Unknown error at address 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
    {
        Serial.println("No I2C devices found\n");
    }
    else
    {
        Serial.println("Done.\n");
    }
}

void loop()
{
    checkI2CBus();

    readValues();
    displayValues();
    publishValues();
    delay(5000);
}
