/*  .
 *  angular webhost with index, script.js and action.php
 *  supports JSONP if required.
 *  able to update via wifi and respond to alexa requests
 *  also serves a simple dash that lets you request actions.
 *  uses wifi manager to set up wifi connections.
 *  uses corrected isDST algorithm.
 *  does not use 3 strike attempt at time
*/

/* based on https://github.com/kakopappa/arduino-esp8266-alexa-multiple-wemo-switch */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <functional>
#include "switch.h" 
#include "UpnpBroadcastResponder.h"
#include "CallbackFunction.h"

#include <DNSServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

/* my pages */
#include "index.html.h"          // the angular container page
#include "script.js.h"          // my scripts.

/* create dummy functions, to be overwritten by specifics.h if needed. */
void RunImplementationSetup(); //repeat this in specifics.h if needed.
void RunImplementationLoop();  //repeat this in specifics.h if needed.
void handleFeatures();         //repeat this in specifics.h if needed.
void setupForNewDay();         //overwritten after specifics.h
byte updateEventTimes();       //overwritten after specifics.h
void handleScript();           //overwritten after specifics.h
void handleRoot();             //overwritten after specifics.h

int indexOfNextEvent;          //allows me to find the details of the next event due.
int minsToNextEvent;           //tracks when the next event will happen.


// prototypes
boolean connectWifi();


boolean wifiConnected = false;

UpnpBroadcastResponder upnpBroadcastResponder;

Switch *first_device = NULL;
/*Switch *second_device = NULL;*/

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

//time setup
static const char ntpServerName[] = "us.pool.ntp.org";
int timeZoneOffset = 0;     // GMT
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
bool isDst(int day, int month, int dow); //defined below

time_t prevDisplay = 0; // when the digital clock was displayed

String padDigit(int digit){
  String padChar = "";
  if (digit<10){
      padChar = "0";
  }
  return padChar + digit; 
}


#include "specifics.h"

void setupForNewDay() {
  /* Purpose: Resets everything for the new day */

  Serial.println(F("  Its a new day!"));
  lastDayFromWeb = day();

  //Now reset the Events' enacted flags to false, as they've not happened for this new day.
  for (byte i = 1; i < EVENT_COUNT; i++) {
    Serial.print(F("    Setting the active flag for "));
    Serial.print(dailyEvents[i].label);
    Serial.println(F(" to false"));
    dailyEvents[i].enacted = false;
  }

  //Check to see if we are in DST.
  thisDevice.dst = isDst(day(), month(), weekday()); //option base 0.
  Serial.print(F("    DST is "));
  Serial.println(thisDevice.dst ? "ON" : "OFF");
  
  timeZoneOffset = (thisDevice.dst ? 1 : 0);
}

byte isEventTime(int currentMinuteOfDay) {

  /* purpose: update indexOfNextEvent and minsToNextEvent, and return the index of the due event (if it exists.
     method:  compare against my global struct for daily events.
  */
  
  bool eventDue = false;
  bool eventOverdue = false;
  byte retval = 0;
  int lowestFoundLag = 24*60; //start point. Assume no events today.

  Serial.print(F("\n  Checking the scheduled events for minute "));
  Serial.print(currentMinuteOfDay);

  for (byte i = 1; i < EVENT_COUNT; i++) { // iterate the daily events
    int minuteOfEvent = (dailyEvents[i].h * 60) + dailyEvents[i].m;

    Serial.print(F("\n    Event labelled "));
    Serial.print(dailyEvents[i].label);

    eventDue = dailyEvents[i].enacted == false
               &&
               currentMinuteOfDay == minuteOfEvent;

    if (eventDue) {
      Serial.print(F(": Due "));
      indexOfNextEvent = i;          
      minsToNextEvent = 0;
      retval = i; //have this value to return later.
      
    } else {
      if ((minuteOfEvent - currentMinuteOfDay) > 0) {
        Serial.print(F(": Not due for "));
        Serial.print(minuteOfEvent - currentMinuteOfDay);
        Serial.print(F(" minute(s)."));
      } else {
        Serial.print(F(": Missed today. Next due tomorrow."));
        minuteOfEvent = minuteOfEvent + (24 * 60); //look at tomorrow's instead.
      }

      if ((minuteOfEvent - currentMinuteOfDay) < lowestFoundLag){
        lowestFoundLag = (minuteOfEvent - currentMinuteOfDay);
        indexOfNextEvent = i;
        minsToNextEvent = lowestFoundLag;
        
      }
    }

  }
  return retval;
}

void handleScript(){
  Serial.print(F("\nScript request"));
  httpServer.sendHeader("Cache-Control","max-age=2628000");
  httpServer.send_P ( 200, "application/javascript", SCRIPT_JS);
  Serial.println(F("...done."));
}

void handleRoot(){  
  Serial.print(F("\nHomepage request"));
  httpServer.sendHeader("Cache-Control","max-age=2628000");
  httpServer.send_P ( 200, "text/html", INDEX_HTML);
  Serial.println(F("...done."));
}

void handleFeatures(){
  Serial.println(F("Features request"));
  
  bool usingCallback = (httpServer.hasArg("callback"));
  String events = "";
  String features = 
                  "{\"address\":\""      + WiFi.localIP().toString() + "\""
                + ",\"app_name\":\""     + (String) AP_NAME + "\""
                + ",\"app_version\":\""  + (String) AP_VERSION + "\""
                + ",\"app_desc\":\""     + (String) AP_DESC + "\""  
                + ",\"time_of_day\":\""  + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second()) + "\""
                + ",\"mode\":\""         + (thisDevice.operating_mode) + "\"" 
                + ",\"is_powered\":"     + (thisDevice.powered ? "true" : "false") 
                + (thisDevice.operating_mode == "percentage" ? 
                   + ",\"percentage\":"    + (String) thisDevice.percentage
                   + ",\"perc_label\":\""  + (String) thisDevice.perc_label + "\"" 
                   : "")
                + ",\"is_dst\":"            + (thisDevice.dst ? "true" : "false") 
                + ",\"is_using_timer\":"    + (thisDevice.usingTimer   ? "true" : "false")   
                + ",\"next_event_due\":"    + minsToNextEvent
                + ",\"next_event_name\":\"" + dailyEvents[indexOfNextEvent].label + "\""
                + ",\"is_skipping_next\":"  + (thisDevice.skippingNext ? "true" : "false")   
                + ",\"last_action\":\""  + thisDevice.lastAction + "\""
                + ",\"request\":{\"base_url\":\"action.php\",\"master_param\":\"master\""
                + (thisDevice.operating_mode == "percentage" ? 
                    ",\"start_param\":\"start\",\"end_param\":\"end\",\"duration_param\":\"duration\""
                   : "")
                + "}"
                + (String) ",\"events\":[";
                 
                //attempt to iterate.   
                for (byte i = 1; i < EVENT_COUNT; i++) {
                  if (events!= ""){ events += ",";}
                  events += "{\"time\":\"" + (String) dailyEvents[i].h + ":" + padDigit(dailyEvents[i].m) + "\",\"label\":\"" + dailyEvents[i].label + "\",\"enacted\":" + (dailyEvents[i].enacted ? "true" : "false") + "}";  
                }
 
  features = features + events + "]}";

  if(usingCallback){
    httpServer.send(200, "text/javascript", httpServer.arg("callback") + "(" + features + ");");
    Serial.println(F("Sending response with callback"));
  }else{
      httpServer.sendHeader("Access-Control-Allow-Origin","*");
      httpServer.sendHeader("Server","ESP8266-AA");
      httpServer.send(200, "application/json", features);
    Serial.println(F("Sending response without callback"));
  }   
}


void setup(){
  Serial.begin(115200);

   
  // Initialise wifi connection
  WiFiManager wifiManager;

  wifiConnected = wifiManager.autoConnect(AP_NAME);
  //wifiManager.startConfigPortal(AP_NAME); //use to test wifi config capture.
  
  if(wifiConnected){
    upnpBroadcastResponder.beginUdpMulticast();
    
    // Define your switches here. Max 14
    // Format: Alexa invocation name, local port no, on callback, off callback
    first_device = new Switch(ALEXA_DEVICE_1, 81, FirstDeviceOn, FirstDeviceOff);
    /*second_device = new Switch(ALEXA_DEVICE_2, 82, SecondDeviceOn, SecondDeviceOff);*/

    //Serial.println("Adding switches upnp broadcast responder");
    upnpBroadcastResponder.addDevice(*first_device);
    /*upnpBroadcastResponder.addDevice(*second_device);*/

    Serial.println(F("Registering mDNS host")); 
    MDNS.begin(AP_NAME);

    httpUpdater.setup(&httpServer);
    httpServer.on("/", handleRoot);
    httpServer.on("/script.js", handleScript);
    httpServer.on("/sync", [](){ 
      while (timeStatus() == timeNotSet); //keep trying to set the time
      httpServer.send(200, "text/plain", "sync'd");
    });
    httpServer.on("/action.php", handleAction);
    httpServer.on("/features.json",handleFeatures);
    httpServer.begin();

    MDNS.addService("http", "tcp", 80);
    Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", AP_NAME);

    Serial.println(F("Looking for NTP time signal"));
    Udp.begin(localPort);
    Serial.print(F("Local port: "));
    Serial.println(Udp.localPort());
    Serial.println(F("waiting for sync"));
    setSyncProvider(getNtpTime);
    
    setSyncInterval(10 * 60);

    Serial.println(F("Running implementation setup"));
    RunImplementationSetup();

    Serial.println(F("Entering loop"));
  }
}

void loop(){

  yield(); //let the ESP8266 do its background tasks.
  
  //connect wifi if not connected
  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    
    return;
  }else{
      upnpBroadcastResponder.serverLoop();
      
      first_device->serverLoop();
      /*second_device->serverLoop();*/
      
      httpServer.handleClient();
  }
  RunImplementationLoop();
}


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime(){
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println(F("Transmit NTP Request"));
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return 1 + secsSince1900 - 2208988800UL + timeZoneOffset * SECS_PER_HOUR; //1 is the adjustment for lag.
    }
  }
  Serial.println(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
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

bool isDst(int day, int month, int dow) {
    /* Purpose: works out whether or not we are in UK daylight savings time SUMMER TIME.
     using day, month and dayOfWeek
     Expects: day (1-31), month(0-11), day of week (0-6)
     Returns: boolean true if DST
     */
    
    month++; //too tricky to do zero based.
    
    if (month < 3 || month > 10)  return false; //jan,feb, nov, dec are NOT DST. 
    if (month > 3 && month < 10)  return true;  //apr,may,june,july,august,september IS DST.

    //now the picky ones.
    int previousSunday = day - dow;
    
    if (month == 3) return previousSunday >= 25; //the most recent sunday was the last sunday in the month.
    if (month == 10) return previousSunday < 25; //the most recent sunday was not the last sunday in the month.

    return false; // something went wrong.
}

