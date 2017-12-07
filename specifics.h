/* notes:
 * blinds and catfeeder are using feather huzzah.
 * bedroom lamp and teddy night light 
 * todo: keep next event last event somewhere visible.
 * todo: individual refresh.
 * todo: serial print can go quiet, should I remove?
 * think about adding days of week to schedule.
 * think about allowing someone to choose the schedule via web ui.
 * html has an error. it doesn't poll the local devices for updates.
*/ 

/* nomenclature */
#define AP_NAME        "Bedroom"    //gets used for access point name on wifi configuration and mDNS
#define ALEXA_DEVICE_1 "bedroom"   //gets used for Alexa discovery and commands (must be lowercase)
#define AP_DESC        "Modded Lamp from The Range" //used for dash.
#define AP_VERSION     "1.1t"

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
  String mode;
  bool skippingNext;
  String lastAction;

  /* unused members */
  byte percentage;
  String perc_label;
  
} progLogic;

progLogic thisDevice = {false, true, false, true,"toggle",false,"Powered on",
                        0,""};

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

void doEvent(byte eventIndex = 0) {

  Serial.print(F("\n      called doEvent for scheduled item "));
  Serial.println(dailyEvents[eventIndex].label);
  dailyEvents[eventIndex].enacted = true;
  thisDevice.powered = dailyEvents[eventIndex].powered;
 
  Serial.println(thisDevice.powered ? "-> Powered" : "-> Off");
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
      Serial.println(F("bouncing"));
    }
  }
}


void RunImplementationSetup(){
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
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
  digitalWrite(RELAY_PIN, thisDevice.powered);
  digitalWrite(BLUE_LED, (thisDevice.powered ? LED_ON : LED_OFF)); 
}

void FirstDeviceOn() {
    Serial.print("Switch 1 turn on ...");
    thisDevice.lastAction = "Powered on by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    thisDevice.powered = true;
}

void FirstDeviceOff() {
    Serial.print(F("Switch 1 turn off ..."));
    thisDevice.lastAction = "Powered off by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    thisDevice.powered = false;
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

