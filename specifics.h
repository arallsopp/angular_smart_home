/* notes:
 * this is the astronomy branch.
 * implementation: blinds
*/ 

/* nomenclature */
#define AP_NAME        "Blind"      //gets used for access point name on wifi configuration and mDNS
#define ALEXA_DEVICE_1 "blind"      //gets used for Alexa discovery and commands (must be lowercase)
#define AP_DESC        "The kitchen blinds"     //used for dash.
#define AP_VERSION     "1.0a"

/* parameters for this implementation */
#define BLUE_LED        BUILTIN_LED          // pin for wemos D1 MINI PRO's onboard blue led
#define LED_OFF         HIGH                  // let's me flip the values if required, as the huzzah onboard LEDs are reversed.
#define LED_ON          LOW                 // as line above.
#define SWITCH_PIN      D2                   // pin connected to PUSH TO CLOSE switch.
#define FADE_PIN        D1
#define EVENT_COUNT     2 +1                // zero based array. Option 0 is reserved.

#define BUTTON_PUSHED          1
#define BUTTON_RELEASED        0  

int lastDayFromWeb = 0;    //I use this to detect a new date has occurred.
int currentMinuteOfDay = 0;

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

progLogic thisDevice = {false, true, false, false,"percentage",0,"Brightness",false,"Powered on"};

typedef struct {
  byte h;
  byte m;
  bool enacted;
  char label[20];
  int target_percentage;
  int transitionDurationInSecs;
} event_time;             // event_time is my custom data type.

event_time dailyEvents[EVENT_COUNT] = {
  {  0, 0, false, "reserved",            0,       0},
  { 18, 0, false, "evening light up",  100,   30*60},
  { 23, 0, false, "night fade out",      0,   60*60}
};                       // this is my array of dailyEvents.

struct fade {
  bool active = false;
  unsigned long startTime = 0L;
  long duration = 1 ;
  int startBrightness = 0;
  int endBrightness = 0;
};
struct fade thisFade; //I can populate this from the actions, and us it to update the brightness within the loop.
//as you're only changing LED on fade, alexa and buttons should set thisFade.

void doEvent(byte eventIndex = 0) {

  Serial.print(F("\n      called doEvent for scheduled item "));
  Serial.println(dailyEvents[eventIndex].label);
  dailyEvents[eventIndex].enacted = true;
  //to fix thisDevice.powered = dailyEvents[eventIndex].powered;
  thisFade.active = true;
  thisFade.startTime = millis();
  thisFade.duration = dailyEvents[eventIndex].transitionDurationInSecs * 1000;
  thisFade.startBrightness = thisDevice.percentage;
  thisFade.endBrightness = dailyEvents[eventIndex].target_percentage;
 
  Serial.println(thisDevice.powered ? "-> Powered" : "-> Off");
}

void updateLEDBrightness() {
  static int lastBrightnessLevel = 0;

  if (thisFade.active) {
    //calculate new brightness level
    long timeElapsed = millis() - thisFade.startTime;
    float timeAsFraction = (timeElapsed / float(thisFade.duration));

    //now work out the difference between start and end brightness level.
    int fadeTotalTravel = thisFade.endBrightness - thisFade.startBrightness;
    float newBrightnessLevel = thisFade.startBrightness + (fadeTotalTravel * timeAsFraction);

    //now convert new brightness level to the operable range.
    thisDevice.percentage = (int) newBrightnessLevel;

    if (timeAsFraction >= 1) {
      Serial.println("\nFade target reached. Turning off thisFade.active");
      thisDevice.percentage = thisFade.endBrightness;
      thisFade.active = false;
    } else {
       Serial.printf("\n%f percent of transition from %d to %d over %dms. At level %d (%d PWM)", (timeAsFraction * 100), thisFade.startBrightness, thisFade.endBrightness, thisFade.duration, thisDevice.percentage, newBrightnessLevel * 10.23);
    }
     thisDevice.powered = newBrightnessLevel > 0;


    int powerLevel = (newBrightnessLevel * 10.23) * (thisDevice.powered ? 1 : 0);
    if (powerLevel != lastBrightnessLevel) {
      analogWrite(FADE_PIN, powerLevel);
      analogWrite(BUILTIN_LED, 1023 - powerLevel);
      lastBrightnessLevel = powerLevel;
    }
  }
}

void handleLocalSwitch(){
  static bool lastValue;
  static unsigned long ignoreUntil = 0UL;

  bool buttonState = (digitalRead(SWITCH_PIN) == HIGH);
  if (buttonState != lastValue) {
    //check the debounce
    if (millis() > ignoreUntil) {
      lastValue = buttonState;
      ignoreUntil = millis() + 100UL;
      Serial.println(F("Enacting, and setting debounce for 1/10th sec"));

      //now toggle the brightness
      if(thisDevice.percentage > 50){
        dailyEvents[0].target_percentage = 0;
      }else{
        dailyEvents[0].target_percentage = 100;
      }
    
      dailyEvents[0].transitionDurationInSecs = 3;
      doEvent(0);
    } else {
      Serial.println(F("Debouncing"));
    }
  }
}


void RunImplementationSetup(){
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(FADE_PIN, OUTPUT);
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
  updateLEDBrightness(); 
}

void FirstDeviceOn() {
    Serial.print("Switch 1 turn on ...");
    thisDevice.lastAction = "Powered on by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
    
    dailyEvents[0].target_percentage = 100;
    dailyEvents[0].transitionDurationInSecs = 3;
    doEvent(0);
}

void FirstDeviceOff() {
    Serial.print(F("Switch 1 turn off ..."));
    thisDevice.lastAction = "Powered off by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());

    dailyEvents[0].target_percentage = 0;
    dailyEvents[0].transitionDurationInSecs = 3;
    doEvent(0);

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
  }else if(settingStart){
    dailyEvents[0].target_percentage = httpServer.arg(0).toInt();
    dailyEvents[0].transitionDurationInSecs = 3;
    doEvent(0);
       actionResult = "{\"message\":\"Set brightness\"}";    
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


