/* notes:
 * this is the astronomy branch.
 * implementation: blinds
 * using feather huzzah.
*/ 


#include <Servo.h>              // to move the servos.

/* nomenclature */
#define AP_NAME        "Blind"      //gets used for access point name on wifi configuration and mDNS
#define ALEXA_DEVICE_1 "blind"      //gets used for Alexa discovery and commands (must be lowercase)
#define AP_DESC        "The kitchen blinds"     //used for dash.
#define AP_VERSION     "1.0a"

/* parameters for this implementation */
#define BLUE_LED        BUILTIN_LED          // pin for wemos D1 MINI PRO's onboard blue led
#define LED_OFF         HIGH                  // let's me flip the values if required, as the huzzah onboard LEDs are reversed.
#define LED_ON          LOW                 // as line above.
#define SWITCH_PIN      16                   // pin connected to PUSH TO CLOSE switch.
#define EVENT_COUNT     4 +1                // zero based array. Option 0 is reserved.

#define BUTTON_PUSHED          1
#define BUTTON_RELEASED        0  

/* add some defs to make it easier to track the blind position */
#define POSITION_OPEN          104
#define POSITION_CLOSED          0

#define POSITION_MAX           180
#define POSITION_MIN             0


/* add some defs to make it easier to track the events index */
#define SUNRISE 1
#define MIDMORN 2
#define NOON    3
#define SUNSET  4

int lastDayFromWeb = 0;    //I use this to detect a new date has occurred.
int currentMinuteOfDay = 0;

Servo myservo;       //setup the servo

typedef struct {
  bool online;
  bool usingTimer;
  bool dst;
  bool powered;
  String operating_mode;
  byte percentage;
  String perc_label;  
  bool skippingNext;
  String lastAction;
} progLogic;

progLogic thisDevice = {false, true, false, false,"percentage",0,"Angle",false,"Powered on"};

typedef struct {
  byte h;
  byte m;
  bool enacted;
  char label[20];
  int target_percentage;
} event_time;             // event_time is my custom data type.

event_time dailyEvents[EVENT_COUNT] = {  
  { 0, 00, false, "reserved",             0},  /* 0 */
  { 6, 30, false, "sunrise",             16},  /* 1 */
  { 9, 30, false, "mid-morning",         40},  /* 2 */
  {12, 00, false, "solar noon",         104},  /* 3 */
  {18, 30, false, "sunset", POSITION_CLOSED}   /* 4 */
};                            // initialises my events to a reasonable set of defaults.


event_time extractAndStoreEventTime(String haystack, String needle, int eventIndex = -1) {
  /* purpose:  accepts json. Searches within json haystack for needle,
               finds the next hour and minute, and adds those to the events at specified index.
     returns:  an astronomy event.
  */

  event_time retval;
  Serial.print(F("\n    Looking up "));
  Serial.print(needle);
  Serial.print(F(" - "));

  int blockPosition = haystack.indexOf(needle);                   //tells me where the needle starts.

  int position = haystack.indexOf("hour\":\"", blockPosition);    //looks for the hour after it.
  if (position > -1) {                                            //something was found
    int found_hour = haystack.substring(position + 7).toInt();          //look ahead to find the integer value of hour.
 
    if (position > -1) {                                          //hour was found, look for minute
      int position = haystack.indexOf("minute\":\"", blockPosition);
      int found_minute  = haystack.substring(position + 9).toInt();     //look ahead to find the integer value of hour.
      Serial.print(found_hour);
      Serial.print(":");
      Serial.print(found_minute);
      
      
      if (eventIndex == -1) {
        //nothing to store;
      } else {
        dailyEvents[eventIndex].h = found_hour;                           //store the hour into the dailyEvent array at eventIndex.
        dailyEvents[eventIndex].m = found_minute;                         //store the minute into the dailyEvent array at eventIndex.
        Serial.print (F(" - Stored at position "));
        Serial.print (eventIndex);
      }
      retval.h = found_hour;
      retval.m = found_minute;
      needle.toCharArray(retval.label,20);
    }

  }
  return retval;
}

void getAstronomyData() {
  char* host = "api.wunderground.com";

  Serial.print(F("  Connecting to "));
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println(F("connection failed"));

    return;
  }

  // We now create a URI for the request
  String url = "/api/05328f28ceea6fba/astronomy/q/UK/London.json";
  {
    Serial.print(F("    Requesting URL: "));
    Serial.println(url);
  }

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(500);

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r'); //remember, this will include the headers.

    //one of the lines returned will be the json string that features the times.
    int blockPosition = line.indexOf("sunrise");       //see if its this one.
    if (blockPosition > -1) {
      extractAndStoreEventTime(line, "sunrise", SUNRISE);    //store this
      extractAndStoreEventTime(line, "sunset", SUNSET);     //store this

      if (timeStatus() == timeNotSet) {
        //you don't know the time, so may as well extract an approximation from the json.
        event_time temp = extractAndStoreEventTime(line, "current_time");
        Serial.println(F("  Setting time of day from json request"));
        setTime(temp.h, temp.m, second(), day(), month(), year());
      }

      //now work out the midpoints
      int sunriseAsMinuteOfDay = (dailyEvents[SUNRISE].h * 60) + (dailyEvents[SUNRISE].m);
      int sunsetAsMinuteOfDay = (dailyEvents[SUNSET].h * 60) + (dailyEvents[SUNSET].m);
      int lengthOfDayInMins = sunsetAsMinuteOfDay - sunriseAsMinuteOfDay;
      int midMornAsMinuteOfDay = sunriseAsMinuteOfDay + (lengthOfDayInMins / 4);
      int noonAsMinuteOfDay = sunriseAsMinuteOfDay + (lengthOfDayInMins / 2);

      dailyEvents[MIDMORN].h = floor(midMornAsMinuteOfDay / 60);
      dailyEvents[MIDMORN].m = (midMornAsMinuteOfDay % 60);

      dailyEvents[NOON].h = floor(noonAsMinuteOfDay / 60);
      dailyEvents[NOON].m = (noonAsMinuteOfDay % 60);

    }

  }


  {
    Serial.println();
    Serial.println(F("  Closing connection"));
  }
}

void positionBlindLouvres(int position) {
  
    Serial.print(F("Blind at "));
    Serial.println(position);

  myservo.write(position);
  yield(); //let the ESP8266 do its background tasks.
  delay(20);
  thisDevice.percentage = position;
  thisDevice.powered = thisDevice.percentage != POSITION_CLOSED;
}


void animateBlindLouvres(int position_target) {
  /* this should just step by 1 degree increments.*/
  int position_start = thisDevice.percentage;
  int distance = (position_target - position_start);

  if (distance) { //somewhere to go.
    while (thisDevice.percentage != position_target) {
      if (thisDevice.percentage > position_target) {
        thisDevice.percentage--;
      }else{
        thisDevice.percentage++;
      }
      positionBlindLouvres(thisDevice.percentage);
      
    }
    positionBlindLouvres(position_target);
  }
}


void doEvent(byte eventIndex = 0) {

  Serial.print(F("\n      called doEvent for scheduled item "));
  Serial.println(dailyEvents[eventIndex].label);
  dailyEvents[eventIndex].enacted = true;
  animateBlindLouvres(dailyEvents[eventIndex].target_percentage);    
  Serial.println(thisDevice.powered ? "-> Powered" : "-> Off");
}

void handleLocalSwitch(){
  static int increment = 1;
 
  bool buttonState = (digitalRead(SWITCH_PIN) == HIGH);
  if (buttonState) {
    if ((thisDevice.percentage + increment) > POSITION_MAX) {
      increment = -1;
    } else if ((thisDevice.percentage + increment) < POSITION_MIN) {
      increment = 1;
    }else{
      //you're somewhere in the middle and all is well.
    }
    positionBlindLouvres(thisDevice.percentage + increment);
  }
}


void RunImplementationSetup(){
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
 
  digitalWrite(BLUE_LED, LED_OFF);
 
}

void RunImplementationLoop(){
  static int minuteOfLastCheck = -1;
  static int dayOfLastCheck = -1;
  byte activeEvent = 0;
  
  time_t t = now();
  thisDevice.online = (timeStatus()==timeSet);

  if(day(t) != dayOfLastCheck){
    setupForNewDay();
    getAstronomyData();
    dayOfLastCheck = day(t);
  }

  if (minute(t) != minuteOfLastCheck){

     currentMinuteOfDay = (hour(t) * 60) + (minute(t)); //already includes DST adjustment.

     //now use currentMinuteOfDay to work out whether we should perform an event.
     if(thisDevice.usingTimer){
        activeEvent =  (isEventTime(currentMinuteOfDay)); //activeEvent will be 0 if none, or the index+1 of the event struct.
        if (activeEvent > 0) {
          if(thisDevice.skippingNext){
            Serial.println(F("skipping this event"));
            thisDevice.lastAction = "Skipped the scheduled event at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
            dailyEvents[activeEvent - 1].enacted = true; //makes the feather think the cat has been fed.
            thisDevice.skippingNext = false;
          }else{
            thisDevice.lastAction = "Set master from schedule at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
            doEvent(activeEvent);  //pass in the index of the active event, so that we can set it to enacted, and access the label.
          }
        }
     }
     minuteOfLastCheck = minute(t);
  }

  handleLocalSwitch();
}

void FirstDeviceOn() {
    Serial.print("Switch 1 turn on ...");
    thisDevice.lastAction = "Powered on by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());

    animateBlindLouvres(POSITION_OPEN);
}

void FirstDeviceOff() {
    Serial.print(F("Switch 1 turn off ..."));
    thisDevice.lastAction = "Powered off by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());

    animateBlindLouvres(POSITION_CLOSED);
    
}

void handleAction(){
  Serial.println("action request");
  Serial.println(httpServer.args());  
  bool settingMaster = (httpServer.argName(0) == "master");
  bool settingSkip = (httpServer.argName(0) == "skip");
  bool settingTimer = (httpServer.argName(0) == "timer");
  bool settingStart = (httpServer.argName(0) == "start");
  bool usingCallback = (httpServer.hasArg("callback"));
  String actionResult = "";

  if(settingMaster){
    thisDevice.lastAction = "Set master from web UI at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    if (httpServer.arg(0)=="true"){
          animateBlindLouvres(POSITION_OPEN); 
    }else{
          animateBlindLouvres(POSITION_CLOSED);    
    }
    if(thisDevice.powered){
       actionResult = "{\"message\":\"Master turned on manually\"}";
    }else{
       actionResult = "{\"message\":\"Master turned off manually\"}";
    }
  }else if(settingSkip){
    thisDevice.skippingNext = (httpServer.arg(0)=="true"); 
    if(thisDevice.skippingNext){
          actionResult = "{\"message\":\"Next event will be skipped\"}";
    }else{
          actionResult = "{\"message\":\"Skip event request cancelled\"}";   
    }
  }else if(settingTimer){
    thisDevice.usingTimer = (httpServer.arg(0)=="true"); 
    if(thisDevice.usingTimer){
          actionResult = "{\"message\":\"Timer has been enabled\"}";
    }else{
          actionResult = "{\"message\":\"Timer has been disabled\"}";   
    }
  }else if(settingStart){
    int targetPosition = httpServer.arg(0).toInt();

    animateBlindLouvres(map(targetPosition,0,100,POSITION_CLOSED,POSITION_OPEN));
    actionResult = (String) "{\"message\":\"Set position to " + targetPosition + "\"}";    
  }else{
       actionResult = "{\"message\":\"Did not recognise action\"}";
  } 

  httpServer.sendHeader("Access-Control-Allow-Origin","*");
  
  if(usingCallback){
    httpServer.send(200, "text/javascript", httpServer.arg("callback") + "(" + actionResult + ");");
    Serial.println(F("Sending response with callback"));
  }else{
      httpServer.sendHeader("Access-Control-Allow-Origin","*");
      httpServer.sendHeader("Server","ESP8266-AA");
      httpServer.send(200, "application/json", actionResult);
    Serial.println(F("Sending response without callback"));
  }
  
  Serial.print(F("Master:"));  Serial.println(thisDevice.powered ? "Powered" : "Off");
  Serial.print(F("Timer:"));   Serial.println(thisDevice.usingTimer ? "Enabled" : "Disabled");
  Serial.print(F("Skipping:"));Serial.println(thisDevice.skippingNext ? "Yes" : "No");
}


