This project lays out a framework for a home automation ecosystem, running on various ESP8266 chips and/or TPLink smart plugs.

##The following features are included:

###Setup
- Each device supports NTP time lookup for automatic clock synchronisation.
- Each device offers a WiFi manager. If it cannot connect to your wifi with known creds on boot, it will set itself up as an access point. Connect to the access point by joining **its** WiFi, and provide the SSID and pass for your own. After that, it will reboot and load these credentials by default.
- Each device can identify itself to Amazon alexa scans. Just ask Alexa to 'discover devices'.

###Operation

- Each device can specify its own schedule of events to run automatically.
- Each device can respond to Amazon alexa on/off commands for manual override.
- Each device can establish its own mDNS entry to simplify finding it on a network.
- Each device can serve an HTML5 dashboard that leverages angular to provide control over its own status.
- Each device can describe its features and status to other dashboards.
- Each dashboard can consume the features of other devices and allow control of those too.
- Each dashboard can access a TP-Link account and control any smart plugs that are registered.

As such, there are 4 ways that each device can be controlled

1. Via the 'events' array of timed scheduled items (eg. Turn off at 4pm).
2. Via on/off voice commands issued to Amazon Alexa.
3. Via a web browser aimed at the device's mDNS or IP entry.
4. Via a web browser aimed at any device running this ecosystem, within the local network.
 

##Types of device used:

My home projects use Adafruit Feather Huzzahs, and Wemos D1 Mini Pros.

##Branches

The code is arranged so that only specifics.h changes between implementations. All branches are able to consume and control the features of all other branches.

- The **master** branch uses a specifics.h to support toggle devices - those with a sticky on/off state. Example use would be a lamp (as seen here: https://youtu.be/4WTB1Uiu2DI). Toggle devices show in the HTML5 dash as slide toggle buttons.
- The **momentary** branch uses a specifics.h that supports push-button devices - those where the device turns back off automatically after its task is completed. Examples in my house include the catfeeder, which responds to an 'on' command by dispensing food for 2 seconds then turns off. Momentary devices show in the HTML5 dash as a push button.
- The **percentage** branch uses a specifics.h that supports variable control devices - those where the device can be on/off or at multiple states between. Examples in my house include dimmable LED strings. Percentage devices show in the HTML5 dash as a range slider.
- The **astronomy** branch uses a specifics.h that supports percentage based devices, but additionally adds the position of the sun as a timer input. Examples in my house include the blinds, which allow a range of values between open and closed, but also set themselves to various positions upon noon/sunrise/sunset.

For an overview of the implementation and code, see: https://youtu.be/4WTB1Uiu2DI