#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <PageBuilder.h>
#include <Arduino.h>
#include <EEPROM.h>
// #include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

#include "plant.hpp"
#include "pages.hpp"

#define DEBUG 0
#define SSID "SSID"
#define PSK "PASSKEY"

#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer server;
#endif

/* TODO:
 *  - Send error when plants are full and one is added
 *  - Wait for time sync
 *  - turn LED on when plant needs water
 *  - send email when plant needs water
*/
String listPlants(PageArgument &args);
String getForm(PageArgument &args);
String addPlant(PageArgument &args);
String delPlant(PageArgument &args);
String waterPlant(PageArgument &args);
String redirectPage(PageArgument &args);
void clearEEPROM();
void readEEPROM();
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

// NTPClient timeClient(ntpUDP);
static const char ntpServerName[] = "europe.pool.ntp.org";
const int timeZone = 1; // Central European Time (GMT +1)
WiFiUDP Udp;
uint32_t localPort = 8888; // local port to listen for UDP packets
time_t currentTime, lastChecked;
String currentUri;

const uint8_t LEDpins[] = {D7, D6, D5, D4, D3, D2, D1, D0};

static const char root_element_2[] PROGMEM = {
"<html><body>"
"<h1>Planten</h1>"
"{{Plants}}"
"<h2>Voeg plant toe</h2>"
"<form action=\"./add\" target=\"_self\" method=\"post\">"
"<h3>Plant Naam:</h3>"
"<input type=\"text\" id=\"plantname\" name=\"plantname\" value=\"Jan\"><br><br>"
"<h3>Water Interval:</h3>"
"<input type=\"number\" id=\"intervalWeeks\" name=\"intervalWeeks\" value=\"0\"> weken <br>"
"<input type=\"number\" id=\"intervalDays\" name=\"intervalDays\" value=\"0\"> dagen <br>"
"<input type=\"number\" id=\"intervalHours\" name=\"intervalHours\" value=\"0\"> uren <br>"
"<input type=\"number\" id=\"intervalMinutes\" name=\"intervalMinutes\" value=\"0\"> minuten <br><br>"
"<input type=\"submit\" value=\"Verstuur\">"
"</form>"
"</body></html>"
};


PageElement root_elm("<html><head><style>html{text-align:center;background-image:url(\"https://i.imgur.com/FL0f78w.jpeg\");background-position:center;background-repeat:no-repeat;background-size:cover;}div{max-width:800;margin:auto;background:rgba(222,184,135,0.7);padding:20px}.inputs{text-align:left;margin:auto;max-width:250}h3{font-size:1.3em}h2{font-size:2em}body{font-size:1.1em;font-family:Verdana,sans-serif}input{width:151px}</style></head><body><div><div><h1>Planten</h1>{{Plants}}</div><br><div>{{Form}}</div></div></body></html>", {
    {"Plants", listPlants},
    {"Form", getForm}
});
PageBuilder root_page("/", {root_elm});

PageElement add_elm("{{ADD}}{{REDIRECT}}", {{"ADD", addPlant},
                                            {"REDIRECT", redirectPage}});
PageBuilder add_page("/add", {add_elm});

PageElement del_elm("{{DEL}}{{REDIRECT}}", {{"DEL", delPlant},
                                            {"REDIRECT", redirectPage}});
PageBuilder del_page("/delete", {del_elm});

PageElement water_elm("{{WATER}}{{REDIRECT}}", {{"WATER", waterPlant},
                                                {"REDIRECT", redirectPage}});
PageBuilder water_page("/water", {water_elm});

void setup()
{
    EEPROM.begin(MAX_PLANTS * EEPROM_STRUCT_WIDTH);
    delay(DEBUG * 1000);
    Serial.begin(115200);
    delay(DEBUG * 4000);
    Serial.println("System has started.");

    // clearEEPROM();

    // Plant::createPlant("Jan", 1637611741, 814210);
    // EEPROM.read(EEPROM_OFFSET_INTERVAL_WATERING);

    // Serial.println("Addr 0:" + String(EEPROM.read(0)));
    readEEPROM();

    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.print("Connecting");
    WiFi.begin(SSID, PSK);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.println("WiFi connected.");

    root_page.insert(server);
    add_page.insert(server);
    del_page.insert(server);
    water_page.insert(server);

    Plant::getPlantsFromEEPROM();

    // Plant::createPlant("Piet", 1637622750, 818210);
    Udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(300);

    server.begin();

    Serial.print("http server:");
    Serial.println(WiFi.localIP());

    for (uint8_t i = 0; i < sizeof(LEDpins) / sizeof(LEDpins[0]); i++)
    {
        pinMode(LEDpins[i], OUTPUT);
    }
}

void loop()
{
    server.handleClient();
    now();
    // check for all plants if they need water
    if (lastChecked + 10 < now())
    {
        lastChecked = now();

        for (int i = 0; i < MAX_PLANTS; i++)
        {
            Plant *plant = Plant::getPlant(i);
            if (plant != NULL)
            {
                if (plant->needsWatering())
                {
                    Serial.println(plant->getName() + " heeft water nodig!");
                    digitalWrite(LEDpins[i], HIGH);
                    // send email
                }
                else
                {
                    digitalWrite(LEDpins[i], LOW);
                }
            } 
            else {
                digitalWrite(LEDpins[i], LOW);
            }
        }
    }
}

/* List all plants in memory and return them as a HTML string. */
String listPlants(PageArgument &args)
{
    String result = "";
    for (int i = 0; i < MAX_PLANTS; i++)
    {
        if (Plant::getPlant(i) != NULL)
        {
            String plantName = Plant::getPlant(i)->getName();
            String lastWatered = Plant::getPlant(i)->getLastWatered();
            String nextWatering = Plant::getPlant(i)->getNextWatering();
            String intervalWatering = Plant::getPlant(i)->getIntervalWatering();
            result += "<h3> " + plantName + "</h3>";
            result += plantName + " heeft voor het laatst water gehad op " + lastWatered + ". "; 
            result += plantName + " moet weer water krijgen op " + nextWatering + ". ";
            result += plantName + " moet elke " + intervalWatering + " water krijgen anders gaat "+ plantName + " dood. <br>";
            result += " <a href=\"/water?plant=" + String(i) + String("\">Geef ") + plantName + " water</a><br>";
            result += " <a href=\"/delete?plant=" + String(i) + String("\">Verwijder ") + plantName + "</a><br>";
            result += "<br>";
        }
    }
    return result;
}

String getForm(PageArgument &args)
{
    String result = "";
    result += "<h2>Voeg plant toe</h2>";
    result += "<form action=\"./add\" target=\"_self\" method=\"post\">";
    result += "<h3>Plant Naam:</h3>";
    result += "<input type=\"text\" id=\"plantname\" name=\"plantname\" placeholder=\"Jan\"><br><br>";
    result += "<h3>Water Interval:</h3>";
    result += "<div class=\"inputs\">";
    result += "<input type=\"number\" id=\"intervalWeeks\" name=\"intervalWeeks\" value=\"0\"> weken <br>";
    result += "<input type=\"number\" id=\"intervalDays\" name=\"intervalDays\" value=\"0\"> dagen <br>";
    result += "<input type=\"number\" id=\"intervalHours\" name=\"intervalHours\" value=\"0\"> uren <br>";
    result += "<input type=\"number\" id=\"intervalMinutes\" name=\"intervalMinutes\" value=\"0\"> minuten <br><br>";
    result += "</div>";
    result += "<input type=\"submit\" value=\"Verstuur\">";
    result += "</form>";
    return result;
}

String addPlant(PageArgument &args)
{

    if (args.hasArg("plantname"))
    {
        time_t interval;
        String plantname = args.arg("plantname");
        interval = args.arg("intervalWeeks").toInt() * 7 * 24 * 60 * 60;
        interval += args.arg("intervalDays").toInt() * 24 * 60 * 60;
        interval += args.arg("intervalHours").toInt() * 60 * 60;
        interval += args.arg("intervalMinutes").toInt() * 60;

        Plant::createPlant(plantname, now(), interval);
    }
    return "";
}

String delPlant(PageArgument &args)
{
    if (args.hasArg("plant"))
    {
        int index = args.arg("plant").toInt();
        Plant::deletePlant(index);
        if(index + 1 <= MAX_PLANTS)
        {
            digitalWrite(LEDpins[index], LOW);
        }
    }
    return "";
}

String waterPlant(PageArgument &args)
{
    if (args.hasArg("plant"))
    {
        int index = args.arg("plant").toInt();
        Plant::getPlant(index)->water();
        Serial.println("Watering plant " + String(index));
        if(index + 1 <= MAX_PLANTS)
        {
            digitalWrite(LEDpins[index], LOW);
        }
    }
    return "";
}

String redirectPage(PageArgument &args)
{
    server.sendHeader("Location", "/", true);
    server.sendHeader("Connection", "keep-alive");
    server.send(303, "text/plain", "");
    server.client().stop();
    add_page.cancel();

    return "";
}

/* Read complete eeprom and print to console */
void readEEPROM()
{
    Serial.println("\n\nEEPROM:");
    for (size_t i = 0; i < MAX_PLANTS; i++)
    {
        Serial.println("\nPlant " + String(i) + ": ");
        for (size_t j = 0; j < EEPROM_STRUCT_WIDTH; j++)
        {
            Serial.print(EEPROM.read(i * EEPROM_STRUCT_WIDTH + j), HEX);
            Serial.print(" ");
        }
    }
    Serial.println("\n\n");
}

void clearEEPROM()
{
    for (size_t i = 0; i < MAX_PLANTS; i++)
    {
        for (size_t j = 0; j < EEPROM_STRUCT_WIDTH; j++)
        {
            EEPROM.write(i * EEPROM_STRUCT_WIDTH + j, 0);
        }
    }
    EEPROM.commit();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP server's ip address

    while (Udp.parsePacket() > 0)
        ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    Serial.println("No NTP Response :-(");
    return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
