/* notes:
 * this implementation is the momentary catfeeder.
 * it uses a feather huzzah.
 * todo: all of the differences in the code
*/ 

/* nomenclature */
#define AP_NAME        "Feeder"    //gets used for access point name on wifi configuration and mDNS
#define ALEXA_DEVICE_1 "feeder"   //gets used for Alexa discovery and commands (must be lowercase)
#define AP_DESC        "The cat sitting stool" //used for dash.
#define AP_VERSION     "1.1m"

/* catfeeder uses gfx display, so add screen dependencies */
#include <Wire.h>                    // required for screen
#include <Adafruit_GFX.h>            // required for screen
#include <Adafruit_SSD1306.h>        // required for screen
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_RESET); // initialises screen

/* parameters for this implementation */
#define BLUE_LED        BUILTIN_LED          // pin for wemos D1 MINI PRO's onboard blue led
#define LED_OFF         HIGH                  // let's me flip the values if required, as the huzzah onboard LEDs are reversed.
#define LED_ON          LOW                 // as line above.
#define MOSFET_PIN      12
#define EVENT_COUNT     1 + 4                // zero based array. Option 0 is reserved.


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
} progLogic;

progLogic thisDevice = {false, true, false, false,"momentary",false,"Powered on"};

typedef struct {
  byte h;
  byte m;
  bool enacted;
  char label[14];
  byte powered;           
} event_time;             // event_time is my custom data type.

event_time dailyEvents[EVENT_COUNT] = {
  { 6, 30, false, "breakfast",     1},
  {11,  0, false, "elevenses",     1},
  {13, 30, false, "lunch",         1},
  {16, 00, false, "afternoon tea", 1},
  {20, 30, false, "supper",        1}
};                       // this is my array of dailyEvents.

void doMomentary(byte portionCount){
  Serial.print(F("Dispensing portions: ")); 
  Serial.println(portionCount);
  
  digitalWrite(BLUE_LED, LED_ON);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(2000 * portionCount);  //I'm using 2 seconds, as I have a 5RPM motor and a 60 degree serving.
  
  digitalWrite(BLUE_LED, LED_OFF);
  digitalWrite(MOSFET_PIN, LOW);
  Serial.println(F("Feed complete."));

  thisDevice.powered = false;
}

void doEvent(byte eventIndex = 0) {
  byte portionCount = dailyEvents[eventIndex].powered;
  
  Serial.print(F("\n      called doEvent for scheduled item "));
  Serial.println(dailyEvents[eventIndex].label);
  doMomentary(portionCount); 
}

void displaySplash() {
  /* Purpose: Shows version information on OLED at startup */

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(AP_NAME);
  display.display();
}

void showTimeOnOLED(int minute = 0) {
  /* Purpose: shows the time on the OLED screen
              uses fillRect to draw the colon so that space is saved.
  */
  Serial.print(F("    Showing time for minute "));
  Serial.print(minute);
  Serial.print(" - ");
  display.setTextColor(WHITE);
  display.setTextSize(4);
  display.setCursor(0, 0);

  /* print the hour */
  int h = (minute / 60);
  if (h >= 24) {
    h = h % 24;
  }
  if (h < 10) {
    display.print("0");
    Serial.print("0");
  }
  display.print(h);
  Serial.print(h);

  /* add dots for the colon */
  display.fillRect(46,  7, 4, 4, WHITE);
  display.fillRect(46, 20, 4, 4, WHITE);
  Serial.print(":");

  /* print the minute */
  display.setCursor(52, 0);
  int m = (minute % 60);
  if (m < 10) {
    display.print("0");
    Serial.print("0");
  }
  display.print(m);
  Serial.println(m);
}
void updateOLEDDisplay() {
  /* Purpose: sends info to the OLED screen.
              bigTime, feeds, the wifi status, etc.
  */

  Serial.println(F("\n  Updating OLED Display"));
  display.clearDisplay(); //starts from scratch

  /* show the time */
  Serial.print(F("\n    Current minute of day is "));
  Serial.print(currentMinuteOfDay);

  Serial.print(F("\n    DST is "));
  Serial.print(thisDevice.dst ? "ON" : "OFF");
  showTimeOnOLED(currentMinuteOfDay);

  /* work out how long until the next feed */
  int totalMinsToEvent = minsToNextEvent(currentMinuteOfDay);
  int hoursToEvent = totalMinsToEvent / 60;
  int minsToEvent = totalMinsToEvent % 60;
   
  int x; int y; int w; int h;
  int origin_x; int origin_y;
  
  origin_x = 102;
  origin_y = 0;
  w = 3; h=3;
  for(int i=0; i<hoursToEvent; i++){
      x = origin_x + ((i%5)*(w+1));
      y = origin_y + ((i/5) * (h+1));
      //printf("hour %d [%d,%d] ",i+1,x,y); 
      display.fillRect(x, y, w,h, WHITE);
  }
  //printf("\n\nCurrently at %d %d\n", x,y);
  origin_y = (y+(h+1));
      
  w = 1;  h=1;
  for(int i=0; i< minsToEvent; i++){
    x = origin_x + ((i%10)*(w+1));
    y = origin_y + ((i/10) * (h+1));
    //printf("min %d [%d,%d] ",i+1,x,y); 
    display.fillRect(x, y, w,h, WHITE);
  } //printf("\n");

  display.display();
}

void RunImplementationSetup(){
  
  pinMode(BLUE_LED, OUTPUT);
  pinMode(MOSFET_PIN, OUTPUT);
  
  digitalWrite(BLUE_LED, LED_OFF);

  display.begin(SSD1306_SWITCHCAPVCC);
  displaySplash();

  display.print(WiFi.localIP()); display.display();

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
     updateOLEDDisplay();

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
     updateOLEDDisplay();
     minuteOfLastCheck = minute(t);
  }
}

void FirstDeviceOn() {
    Serial.print("Switch 1 turn on ...");
    thisDevice.lastAction = "Powered on by Alexa at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());

    doMomentary(1);
}

void FirstDeviceOff() {
    Serial.print(F("Switch 1 turn off ..."));
    //no requirement for momentary.
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
    thisDevice.lastAction = "Requested from web UI at " + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second());
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

  if((thisDevice).powered){
    doMomentary(1);
  }
  Serial.print(F("Master:"));  Serial.println(thisDevice.powered ? "Powered" : "Off");
  Serial.print(F("Timer:"));   Serial.println(thisDevice.usingTimer ? "Enabled" : "Disabled");
  Serial.print(F("Skipping:"));Serial.println(thisDevice.skippingNext ? "Yes" : "No");
}
void handleFeatures(){
  Serial.println(F("Features request"));
  bool usingCallback = (httpServer.hasArg("callback"));
  String events = "";
  String features = 
                  "{\"address\":\"" + WiFi.localIP().toString() + "\""
                + ",\"app_name\":\"" + (String) AP_NAME + "\""
                + ",\"app_version\":\"" + (String) AP_VERSION + "\""
                + ",\"app_desc\":\"" + (String) AP_DESC + "\""  
                + ",\"time_of_day\":\"" + padDigit(hour()) + ":" + padDigit(minute()) + ":" + padDigit(second()) + "\""
                + ",\"mode\":\"" + (thisDevice.mode) + "\"" 
                + ",\"is_powered\":" + (thisDevice.powered ? "true" : "false") 
                + ",\"is_dst\":"         + (thisDevice.dst ? "true" : "false") 
                + ",\"is_using_timer\":"    + (thisDevice.usingTimer   ? "true" : "false")   
                + ",\"next_event_due\":" + minsToNextEvent(currentMinuteOfDay) 
                + ",\"is_skipping_next\":"  + (thisDevice.skippingNext ? "true" : "false")   
                + ",\"last_action\":\""  + thisDevice.lastAction + "\""
                + ",\"request\":{\"base_url\":\"action.php\",\"master_param\":\"master\"}"
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

