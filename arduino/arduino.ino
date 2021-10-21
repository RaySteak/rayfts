#include <ArduinoWiFiServer.h>
#include <BearSSLHelpers.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiGratuitous.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiServer.h>
#include <WiFiServerSecure.h>
#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>
#include "commands.h"

using namespace arduino_constants;

#define LED 15

// your Wi-Fi credentials go here
const char* ssid = "";
const char* pass = "";

//WiFiServer server(PORT);
WiFiUDP server;

bool led_state = LOW;

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  Serial.begin(9600);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin(PORT);
  Serial.print("Server started on port ");
  Serial.println(PORT);
}

void loop() {
  int n = server.parsePacket();
  if(n)
  {
    String message = "";
    String reply = "";
    while(server.available())
    {
      char c;
      server.read(&c, 1);
      message += c;
    }
    Serial.println(message);
    if(message == TURN_OFF)
    {
      digitalWrite(LED, LOW);
      led_state = LOW;
    }
    else if(message == TURN_ON)
    {
      digitalWrite(LED, HIGH);
      led_state = HIGH;
    }
    else if(message == GET_STATE)
    {
      server.beginPacket(server.remoteIP(), server.remotePort());
      reply = String(led_state);
      server.write(reply.c_str());
      server.endPacket();
    }
  }
}
