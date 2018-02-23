#include <WiFi.h>

#include <DNSServer.h>
#include <WebServer.h>
#include "WiFiManager.h"

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
BH1750 lightMeter;
uint16_t lux;

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
    setupClock();
    setupSensors();
}

void readValues()
{
    h = dht.readHumidity();
    t_c = dht.readTemperature();
    lux = lightMeter.readLightLevel();
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

    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx ");
}

void displayValues()
{
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, WiFi.localIP().toString() + " (" + WiFi.RSSI() + " dBm)");

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

void loop()
{
    readValues();
    displayValues();

    delay(5000);
}
