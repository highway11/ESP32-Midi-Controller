  
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <analogWrite.h>


// Debounce buttons and switches, https://github.com/thomasfredericks/Bounce2/wiki
// Define the following here or in Bounce2.h. This make button change detection more responsive.
//#define BOUNCE_LOCK_OUT
#include <Bounce2.h>

#define BUTTON_PIN 0

// Instantiate a Bounce object
Bounce debouncer2 = Bounce();
Bounce debouncer4 = Bounce();
Bounce debouncer5 = Bounce();
Bounce debouncer13 = Bounce();
Bounce debouncer14 = Bounce();
Bounce debouncer15 = Bounce();
Bounce debouncer16 = Bounce();
Bounce debouncer17 = Bounce();
Bounce debouncer18 = Bounce();
Bounce debouncer19 = Bounce();

// Red, green, and blue pins for PWM control
const int redPin = 25;     // 13 corresponds to GPIO13
const int greenPin = 32;   // 12 corresponds to GPIO12
const int bluePin = 33;    // 14 corresponds to GPIO14
char* originalColor = "red";

// Setting PWM bit resolution
const int resolution = 256;




#define SerialMon Serial
#define APPLEMIDI_DEBUG SerialMon
#include <AppleMIDI.h>

// DEFINE HERE THE KNOWN NETWORKS
const char* KNOWN_SSID[] = {"YourWifi1", "YourWifi2", "YourWifi3"};
const char* KNOWN_PASSWORD[] = {"password1", "password2", "password3"};
const IPAddress KNOWN_STATICIP[] = {IPAddress(192,168,137,20), IPAddress(192,168,100,20), IPAddress(192,168,50,20)};
const IPAddress KNOWN_GATEWAY[] = {IPAddress(192,168,137,1), IPAddress(192,168,100,1), IPAddress(192,168,50,1)};
boolean wifiFound = false;
int i, n;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  setRGBColor(originalColor);
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  setRGBColor("red");
  
  Serial.println(info.disconnected.reason);
  //Serial.println("Trying to Reconnect");
  //WiFi.begin(ssid, password);
}

const int   KNOWN_SSID_COUNT = sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0]); // number of known networks

//char ssid[] = "TellMyWifiLover"; //  your network SSID (name)
char ssid[] = "LL"; //  your network SSID (name)
// Set your Static IP address
IPAddress staticIP(192, 168, 137, 20);
// Set your Gateway IP address
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

const char* deviceName = "pedalboard";
const int expPin = 34;
//Initialize variables to read expression pedal status
int newExpVal = 0;
int lastExpVal = 0;

char pass[] = "billow11";    // your network password (use for WPA, or use as key for WEP)

unsigned long t0 = millis();
int8_t isConnected = 0;

APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup()
{

  pinMode(2, INPUT_PULLUP);
  debouncer2.attach(2);
  debouncer2.interval(5); // interval in ms

  pinMode(4, INPUT_PULLUP);
  debouncer4.attach(4);
  debouncer4.interval(5); // interval in ms

  pinMode(5, INPUT_PULLUP);
  debouncer5.attach(5);
  debouncer5.interval(5); // interval in ms

  pinMode(13, INPUT_PULLUP);
  debouncer13.attach(13);
  debouncer13.interval(5); // interval in ms

  pinMode(14, INPUT_PULLUP);
  debouncer14.attach(14);
  debouncer14.interval(5); // interval in ms

  pinMode(15, INPUT_PULLUP);
  debouncer15.attach(15);
  debouncer15.interval(5); // interval in ms

  pinMode(16, INPUT_PULLUP);
  debouncer16.attach(16);
  debouncer16.interval(5); // interval in ms

  pinMode(17, INPUT_PULLUP);
  debouncer17.attach(17);
  debouncer17.interval(5); // interval in ms

  pinMode(18, INPUT_PULLUP);
  debouncer18.attach(18);
  debouncer18.interval(5); // interval in ms

  pinMode(19, INPUT_PULLUP);
  debouncer19.attach(19);
  debouncer19.interval(5); // interval in ms

  // configure LED PWM resolution/range and set pins to LOW
  analogWrite(redPin, 0);
  analogWrite(greenPin, 0);
  analogWrite(bluePin, 0);

  setRGBColor("red");

  

  DBG_SETUP(115200);
  DBG("Booting");

  //-------------------------------------------NEW WIFI CODE ------------------------------------------
    // ----------------------------------------------------------------
  // WiFi.scanNetworks will return the number of networks found
  // ----------------------------------------------------------------
  Serial.println(F("scan start"));
  int nbVisibleNetworks = WiFi.scanNetworks();
  Serial.println(F("scan done"));
  if (nbVisibleNetworks == 0) {
    Serial.println(F("no networks found. Reset to try again"));
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here at least some networks are visible
  // ----------------------------------------------------------------
  Serial.print(nbVisibleNetworks);
  Serial.println(" network(s) found");

  // ----------------------------------------------------------------
  // check if we recognize one by comparing the visible networks
  // one by one with our list of known networks
  // ----------------------------------------------------------------
  for (i = 0; i < nbVisibleNetworks; ++i) {
    Serial.println(WiFi.SSID(i)); // Print current SSID
    for (n = 0; n < KNOWN_SSID_COUNT; n++) { // walk through the list of known SSID and check for a match
      if (strcmp(KNOWN_SSID[n], WiFi.SSID(i).c_str())) {
        Serial.print(F("\tNot matching "));
        Serial.println(KNOWN_SSID[n]);
      } else { // we got a match
        wifiFound = true;
        break; // n is the network index we found
      }
    } // end for each known wifi SSID
    if (wifiFound) break; // break from the "for each visible network" loop
  } // end for each visible network

  if (!wifiFound) {
    Serial.println(F("no Known network identified. Reset to try again"));
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here you found 1 known SSID
  // ----------------------------------------------------------------
  Serial.print(F("\nConnecting to "));
  Serial.println(KNOWN_SSID[n]);
  

  // ----------------------------------------------------------------
  // We try to connect to the WiFi network we found
  // ----------------------------------------------------------------
  
  // Configures static IP address
  if (!WiFi.config(KNOWN_STATICIP[n], KNOWN_GATEWAY[n], subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static ip");
  }
  WiFi.begin(KNOWN_SSID[n], KNOWN_PASSWORD[n]);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  // ----------------------------------------------------------------
  // SUCCESS, you are connected to the known WiFi network
  // ----------------------------------------------------------------
  Serial.println(F("WiFi connected, your IP address is "));
  Serial.println(WiFi.localIP());
  
  if (strcmp(KNOWN_SSID[n],"LL")) {
    setRGBColor("green");
    originalColor = "green";
  } else {
    setRGBColor("blue");
    originalColor = "blue";
   
  }

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  //------------------------------------------END NEW WIFI CODE----------------------------------------

//  // Configures static IP address
//  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
//    Serial.println("Failed to configure static ip");
//  }
//
//  WiFi.begin(ssid, pass);
//  //WiFi.disconnect();  //Prevent connecting to wifi based on previous configuration
//  
//  //WiFi.hostname(deviceName);      // DHCP Hostname (useful for finding device for static lease)
//  //WiFi.config(staticIP, subnet, gateway, dns);
//  //WiFi.begin(ssid, pass);
//
//  //WiFi.mode(WIFI_STA); //WiFi mode station (connect to wifi router only
//
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    DBG("Establishing connection to WiFi..");
//  }
//  DBG("Connected to network");

  DBG(F("OK, now make sure you an rtpMIDI session that is Enabled"));
  DBG(F("Add device named Arduino with Host"), WiFi.localIP(), "Port", AppleMIDI.getPort(), "(Name", AppleMIDI.getName(), ")");
  DBG(F("Select and then press the Connect button"));
  DBG(F("Then open a MIDI listener and monitor incoming notes"));
  DBG(F("Listen to incoming MIDI commands"));

  MIDI.begin();

  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
    isConnected++;
    DBG(F("Connected to session"), ssrc, name);
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    isConnected--;
    DBG(F("Disconnected"), ssrc);
  });
  
  MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
    DBG(F("NoteOn"), note);
  });
  MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
    DBG(F("NoteOff"), note);
  });

  DBG(F("Sending NoteOn/Off of note 45, every second"));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{
   
   
  // Update the Bounce instance :
  
  debouncer2.update();
  debouncer4.update();
  debouncer5.update();
  
  debouncer13.update();
  debouncer14.update();

  debouncer15.update();
  debouncer16.update();
  debouncer17.update();
  debouncer18.update();
  debouncer19.update();
  
 
  

  

  
  byte velocity = 55;
  byte channel = 1;

  
  
  byte note2 = 21;  //1
  byte note4 = 22;  //2
  byte note5 = 23;  //3
  byte note13 = 24; //4
  byte note14 = 25; //5
  byte note15 = 26; //6
  byte note16 = 27; //7
  byte note17 = 28; //8
  byte note18 = 29; //9
  byte note19 = 30; //10
  

 
 
  //--------------------------------------------
  if (debouncer2.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note2, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 2 on"));
  }
  else if (debouncer2.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note2, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 2 off"));
  }

 

  //--------------------------------------------
  if (debouncer4.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note4, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 4 on"));
  }
  else if (debouncer4.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note4, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 4 off"));
  }

  //--------------------------------------------
  if (debouncer5.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note5, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 5 on"));
  }
  else if (debouncer5.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note5, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 5 off"));
  }

  
  //--------------------------------------------
  if (debouncer13.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note13, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 13 on"));
  }
  else if (debouncer13.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note13, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 13 off"));
  }

  //--------------------------------------------
  if (debouncer14.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note14, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 14 on"));
  }
  else if (debouncer14.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note14, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 14 off"));
  }

  //--------------------------------------------
  if (debouncer15.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note15, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 15 on"));
  }
  else if (debouncer15.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note15, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 15 off"));
  }

  //--------------------------------------------
  if (debouncer16.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note16, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 16 on"));
  }
  else if (debouncer16.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note16, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 16 off"));
  }

  //--------------------------------------------
  if (debouncer17.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note17, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 17 on"));
  }
  else if (debouncer17.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note17, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 17 off"));
  }

  //--------------------------------------------
  if (debouncer18.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note18, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 18 on"));
  }
  else if (debouncer18.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note18, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 18 off"));
  }

  //--------------------------------------------
  if (debouncer19.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note19, velocity, channel);
    setRGBColor("white");
    Serial.println(F("note 19 on"));
  }
  else if (debouncer19.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note19, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 19 off"));
  }

  //PROCESS EXPRESSION PEDAL...

  newExpVal = analogRead(expPin);
  newExpVal = map(newExpVal, 0, 4095, 0, 127);
  newExpVal = constrain(newExpVal, 0, 127);
  if (newExpVal != lastExpVal) {
      MIDI.sendNoteOn(31, newExpVal, channel);
      //Serial.println(newExpVal);
  }
  lastExpVal = newExpVal;
  delay(10);

   

 
  // Listen to incoming notes
  MIDI.read();

  
}

//Set Color of RGB Led
void setRGBColor(char* color) {
  if (color == "red") {
    analogWrite(redPin, 20);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  }

  if (color == "blue") {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 20);
  }

  if (color == "green") {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 20);
    analogWrite(bluePin, 0);
  }

  if (color == "white") {
    analogWrite(redPin, 20);
    analogWrite(greenPin, 20);
    analogWrite(bluePin, 20);
  }
}
