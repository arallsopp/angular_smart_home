/* notes:
 * blinds and catfeeder are using feather huzzah.
 * bedroom lamp and teddy night light 
 * todo: keep next event last event somewhere visible.
 * todo: individual refresh.
 *  todo: serial print can go quiet.
 * think about adding days of week to schedule.
 * think about allowing someone to choose the schedule via web ui.
 * html has an error. it doesn't poll the local devices for updates.
*/ 

/* nomenclature */
#define AP_NAME        "Bedroom"    //gets used for access point name on wifi configuration and mDNS
#define ALEXA_DEVICE_1 "bedroom"   //gets used for Alexa discovery and commands (must be lowercase)
#define AP_DESC        "Modded Lamp from The Range" //used for dash.
#define AP_VERSION     "1.0"

/* parameters for this implementation */
#define BLUE_LED        BUILTIN_LED          // pin for wemos D1 MINI PRO's onboard blue led
#define LED_OFF         HIGH                  // let's me flip the values if required, as the huzzah onboard LEDs are reversed.
#define LED_ON          LOW                 // as line above.
#define SWITCH_PIN      D2                   // pin connected to PUSH TO CLOSE switch.
#define RELAY_PIN       D1
#define SWITCH_WIRED_IN true
#define EVENT_COUNT     1 + 4                // zero based array. Option 0 is reserved.

#define BUTTON_PUSHED          1
#define BUTTON_RELEASED        0  

int lastDayFromWeb = 0;    //I use this to detect a new date has occurred.
int currentMinuteOfDay = 0;

typedef struct {
  bool online;
  bool usingTimer;
  bool dst;
  bool powered;
  bool skippingNext;
  String lastAction;
} progLogic;

progLogic thisDevice = {false, true, false, true,false,"Powered on"};

typedef struct {
  byte h;
  byte m;
  bool enacted;
  char label[14];
  byte powered;
} event_time;             // event_time is my custom data type.

event_time dailyEvents[EVENT_COUNT] = {
  { 0,  0, false, "reserved",      0},
  { 8,  0, false, "wakeup",        1},
  { 8, 20, false, "power-down",    0},
  {22, 00, false, "bed-time",      1},
  {23, 00, false, "good-night",    0}
};                       // this is my array of dailyEvents.

void setupForNewDay() {
  /* Purpose: Resets everything for the new day */

  Serial.println("  Its a new day!");
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

void doEvent(byte eventIndex = 0) {

  Serial.print(F("\n      called doEvent for scheduled item "));
  Serial.println(dailyEvents[eventIndex].label);
  dailyEvents[eventIndex].enacted = true;
  thisDevice.powered = dailyEvents[eventIndex].powered;
 
  Serial.println(thisDevice.powered ? "-> Powered" : "-> Off");
}

byte isEventTime(int currentMinuteOfDay) {

  /* purpose: return the index of the event which is due.
     method:  compare against my global struct for daily events.
  */

  bool eventDue = false;
  bool eventOverdue = false;
  byte retval = 0;
  
  Serial.print(F("\n  Checking the scheduled events for minute "));
  Serial.print(currentMinuteOfDay);
  byte maxdrift = 3;        // maxdrift is the number of minutes ago that it will check for missed events
  // you shouldn't really require this, but its possible that the connection stumbles
  // and misses a minute.


  for (byte i = 1; i < EVENT_COUNT; i++) { // iterate the daily events
    int minuteOfEvent = (dailyEvents[i].h * 60) + dailyEvents[i].m;

    Serial.print(F("\n    Event labelled "));
    Serial.print(dailyEvents[i].label);

    for (byte offset = 0; offset <= maxdrift; offset++) { // iterate each offset within maxdrift
      eventDue = dailyEvents[i].enacted == false
                &&
                (currentMinuteOfDay == (minuteOfEvent + offset));
      if (eventDue) {
        Serial.print(F(": Due "));
        Serial.print(offset);           //might be zero, might not.
        Serial.print(F(" minutes ago"));
        retval = i;
      } else if (dailyEvents[i].enacted) {
        Serial.print(F(": Already performed today."));
      } else {
        if ((minuteOfEvent - currentMinuteOfDay) > 0) {
          Serial.print(F(": Not due for "));
          Serial.print(minuteOfEvent - currentMinuteOfDay);
          Serial.print(F(" minute(s)."));
        } else {
          Serial.print(F(": Missed today. Next due tomorrow."));
        }
      }
    }
  }
  return retval;
}

int minsToNextEvent(int currentMinuteOfDay) {

  /* purpose: return the total number of minutes to go until the next event (could be over 60)
     method:  compare against my global struct for daily events.
  */

  Serial.print(F("\n  Checking interval for events at minute "));
  Serial.print(currentMinuteOfDay);
  
  int lowestFoundLag = 24*60; //start point. Assume no events today.

  for (byte i = 1; i < EVENT_COUNT; i++) { // iterate the daily events
    int minuteOfEvent = (dailyEvents[i].h * 60) + dailyEvents[i].m;
    Serial.print("Event ");Serial.print(dailyEvents[i].label);Serial.print(" is due at minute ");Serial.println(minuteOfEvent);
        
    if (minuteOfEvent < currentMinuteOfDay){
      minuteOfEvent = minuteOfEvent + (24 * 60); //look at tomorrow's instead.
      Serial.print("(using tomorrow's time instead, so ");Serial.println(minuteOfEvent);
    }

    int thisEventDueIn = (minuteOfEvent - currentMinuteOfDay);
    Serial.print("Event due in ");Serial.println(thisEventDueIn);
    
    if (thisEventDueIn < lowestFoundLag){
      Serial.println(" - this is the closest so far.");
      lowestFoundLag = thisEventDueIn;
    }    
  }

  Serial.print("Closest at end of loop is ");Serial.println(lowestFoundLag);

  return lowestFoundLag;
}


void handleLocalSwitch(){
  static int prevSwitchPosition = 0;
  static unsigned long nextTriggerAfter = 0;
  const long DEBOUNCE = 500;
  bool debounced = false;
  int thisReading = 0;

  //now read the switch and see if its changed
  if(SWITCH_WIRED_IN){
    thisReading  = digitalRead(SWITCH_PIN);
  }else{
    if (Serial.available()) {
      int incomingByte = Serial.read();
      thisReading = prevSwitchPosition +1;
    }
  }

  if (thisReading != prevSwitchPosition){
    //test for debounce
    debounced = ( (long) ( millis() - nextTriggerAfter ) >= 0);

    if(debounced){
     Serial.print(F("toggling power level: "));
     thisDevice.powered = !thisDevice.powered;
     Serial.println(thisDevice.powered ? "Powered" : "Off");
    
     prevSwitchPosition = thisReading;
     nextTriggerAfter = millis() + DEBOUNCE;
    }else{
      Serial.println("bouncing");
    }
  }
}


void RunImplementationSetup(){
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);
 
  digitalWrite(BLUE_LED, LED_OFF);
 
}

String padDigit(int digit){
  String padChar = "";
  if (digit<10){
      padChar = "0";
  }
  return padChar + digit; 
}

void RunImplementationLoop(){
  static int minuteOfLastCheck = -1;
  static int dayOfLastCheck = -1;
  byte activeEvent = 0;
  
  time_t t = now();
  thisDevice.online = (timeStatus()==timeSet);

  if(day(t) != dayOfLastCheck){
    setupForNewDay();
    dayOfLastCheck = day(t);
  }

  if (minute(t) != minuteOfLastCheck){

     currentMinuteOfDay = (hour(t) * 60) + (minute(t)); //already includes DST adjustment.

     //now use currentMinuteOfDay to work out whether we should perform an event.
     if(thisDevice.usingTimer){
        activeEvent =  (isEventTime(currentMinuteOfDay)); //activeEvent will be 0 if none, or the index+1 of the event struct.
        if (activeEvent > 0) {
          if(thisDevice.skippingNext){
            Serial.println("skipping this event");
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
  digitalWrite(RELAY_PIN, thisDevice.powered);
  digitalWrite(BLUE_LED, (thisDevice.powered ? LED_ON : LED_OFF)); 
}

void FirstDeviceOn() {
    Serial.print("Switch 1 turn on ...");
    thisDevice.lastAction = "Powered on by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    thisDevice.powered = true;
}

void FirstDeviceOff() {
    Serial.print("Switch 1 turn off ...");
    thisDevice.lastAction = "Powered off by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    thisDevice.powered = false;
}

void handleScript(){
  Serial.print(F("\nScript request"));
  httpServer.send_P ( 200, "application/javascript", SCRIPT_JS);
  Serial.println(F("...done."));
}

void handleAction(){
  Serial.println("action request");
  Serial.println(httpServer.args());  
  bool settingMaster = (httpServer.argName(0) == "master");
  bool settingSkip = (httpServer.argName(0) == "skip");
  bool settingTimer = (httpServer.argName(0) == "timer");
  bool usingCallback = (httpServer.hasArg("callback"));
  String actionResult = "";

  if(settingMaster){
    thisDevice.lastAction = "Set master from web UI at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    thisDevice.powered = (httpServer.arg(0)=="true"); 
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
  }else{
       actionResult = "{\"message\":\"Did not recognise action\"}";
  } 

  httpServer.sendHeader("Access-Control-Allow-Origin","*");
  
  if(usingCallback){
    httpServer.send(200, "text/javascript", httpServer.arg("callback") + "(" + actionResult + ");");
    Serial.println("Sending response with callback");
  }else{
      httpServer.sendHeader("Access-Control-Allow-Origin","*");
      httpServer.sendHeader("Server","ESP8266-AA");
      httpServer.send(200, "application/json", actionResult);
    Serial.println("Sending response without callback");
  }
  
  Serial.print("Master:");  Serial.println(thisDevice.powered ? "Powered" : "Off");
  Serial.print("Timer:");  Serial.println(thisDevice.usingTimer ? "Enabled" : "Disabled");
  Serial.print("Skipping:");  Serial.println(thisDevice.skippingNext ? "Yes" : "No");
}
void handleStatus(){
  
  Serial.println("status request");  
  String response = "{\"app_name\":\"" + (String) AP_NAME + "\""
                 + ",\"app_version\":\"" + AP_VERSION + "\""
                 + ",\"time_of_day\":\"" + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second()) + "\""
                 + ",\"is_powered\":" + (thisDevice.powered ? "true" : "false") 
                 + ",\"request\":{\"base_url\":\"action.php\",\"master_param\":\"master\"}"
                 + ",\"is_dst\":"         + (thisDevice.dst ? "true" : "false") 
                 + ",\"next_event_due\":" + minsToNextEvent(currentMinuteOfDay) 
                 + ",\"is_skipping_next\":"  + (thisDevice.skippingNext ? "true" : "false")   
                 + ",\"is_using_timer\":"    + (thisDevice.usingTimer   ? "true" : "false")   
                 + ",\"last_action\":\""  + thisDevice.lastAction + "\",\"events\":[";



  //attempt to iterate.
  String config = "";
  
  for (byte i = 1; i < EVENT_COUNT; i++) {
    if (config!= ""){ config += ",";}
    config += "{\"time\":\"" + (String) dailyEvents[i].h + ":" + padDigit(dailyEvents[i].m) + "\",\"label\":\"" + dailyEvents[i].label + "\",\"enacted\":" + (dailyEvents[i].enacted ? "true" : "false") + "}";  
  }
 
  response = response + config + "]}";

  httpServer.sendHeader("Access-Control-Allow-Origin","*");
  httpServer.send(200, "application/json",response); 
}
void handleFeatures(){
  Serial.print("features request for ");
  Serial.println(WiFi.localIP());
  httpServer.sendHeader("Access-Control-Allow-Origin","*");
  httpServer.sendHeader("Server","ESP8266-AA");
  httpServer.send(200,"text/javascript", httpServer.arg("callback") + (String) "({\"address\":\"" + WiFi.localIP().toString() + "\""
                + ",\"app_name\":\"" + (String) AP_NAME + "\""
                + ",\"app_desc\":\"" + (String) AP_DESC + "\""
                + ",\"is_powered\":" + (thisDevice.powered ? "true" : "false") 
                + ",\"on_url\":\"action.php?master=true\""
                + ",\"off_url\":\"action.php?master=false\""
                + ",\"request\": {\"base_url\":\"action.php\",\"master_param\":\"master\"}"
                + "});");
}
void handleRoot(){  
  Serial.print(F("\nHomepage request"));
  httpServer.send_P ( 200, "text/html", INDEX_HTML);
  Serial.println(F("...done."));
}

