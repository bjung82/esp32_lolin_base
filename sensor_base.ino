#include <WiFi.h>

#include <DNSServer.h>
#include <WebServer.h>
#include "WiFiManager.h"

#include "SSD1306.h"
#include <wire.h>

/* OLED */

SSD1306 display(0x3c, 5, 4);

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

void setup()
{
    Serial.begin(115200);
    display.init();

    setupWifi();
}

void loop()
{
}
