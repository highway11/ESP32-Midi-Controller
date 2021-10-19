  
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <analogWrite.h>

#include <HttpRequest.h>
#include <Preferences.h>

//Editing/Storing Text using web server
WiFiServer server(8888);
Preferences preferences;
HttpRequest httpReq;
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
// Variable to store the HTTP req  uest
String header;

//Store Time On Code -------------------------------
unsigned long startTime = millis();
unsigned long lastCheckTime = startTime;
unsigned long storedOnTime = 0;
const long checkOnTimeInterval = 30000; //30 seconds

//These are for the OLED Display --------------------------------------
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
String screenText[] = {"", "", "", "", "","","", ""};
String buttonText[] = {"CLEAN","TREM","DIRTY","BTN 4","BTN 5","BTN 6","BTN 7","BTN 8","TUNER","NEXT SONG"};
String songs[] = {"WndrflCross","LrdINeedU","ForIWasFar","Gratitude","GreatIsThy","","","","",""};
int currentSong = 0;
int numSongs = 4;
int x, minX;
String message;
boolean doneConnecting = false;
boolean disconnected = false;
boolean tunerActive = false;
int lastButton = 0;
//----------------------------------------------------------------------

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
const long blinkInterval = 1000;
const long voltageCheckInterval = 10000; //10 seconds
unsigned long previousMillis = 0;
unsigned long previousMillisVoltage = 0;
bool blinkLed = true;
bool isLedOn = true;

// Setting PWM bit resolution
const int resolution = 256;




#define SerialMon Serial
#define APPLEMIDI_DEBUG SerialMon
#include <AppleMIDI.h>

// DEFINE HERE THE KNOWN NETWORKS
const char* KNOWN_SSID[] = {"LL", "TellMyWifiLover", "CoSWTP"};
const char* KNOWN_PASSWORD[] = {"***REMOVED***", "***REMOVED***", "***REMOVED***"};
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
  blinkLed = false;
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  setRGBColor("red");
 
  originalColor = "red";
  message = "DISCONNECTED FROM WIFI!";
  disconnected = true;
  //blinkLed = true;
  
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
const int batteryVoltagePin = 36;
//Initialize variables to read expression pedal status
int newExpVal = 0;
int lastExpVal = 0;
//Initialize variables to read battery voltage
int newVoltage = 0;
int lastVoltage = 0;

char pass[] = "***REMOVED***";    // your network password (use for WPA, or use as key for WEP)

unsigned long t0 = millis();
int8_t isConnected = 0;

APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup()
{

  
  //Get time on from onboard storage
  preferences.begin("pedalboard", false);
  storedOnTime = preferences.getULong("ontime", 0);
  
  //Get button text from onboard storage
   preferences.begin("pedalboard", false);
  preferences.getUInt("counter", 0);
  for (int i=1;i<=10;i++) {
    String stringKey = "btn" + String(i);
    char key[6];
    stringKey.toCharArray(key,6);
    Serial.print("Key: ");
    Serial.println(key);
    buttonText[i-1] = preferences.getString(key,String(""));
    Serial.print("Button Text ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(buttonText[i-1]);
  }
  
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

  //---------------------initialize oled display---------------------------------------
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  //delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE,BLACK);
  display.setTextWrap(false);
  x = display.width();
  minX = 0;

  //-------------------------------------------NEW WIFI CODE ------------------------------------------
    // ----------------------------------------------------------------
  // WiFi.scanNetworks will return the number of networks found
  // ----------------------------------------------------------------
  Serial.println(F("scan start"));
  displayText("scan start");
  
  int nbVisibleNetworks = WiFi.scanNetworks();
  Serial.println(F("scan done"));
  if (nbVisibleNetworks == 0) {
    Serial.println(F("no networks found. Reset to try again"));
    displayText("no networks found. Reset to try again");
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here at least some networks are visible
  // ----------------------------------------------------------------
  Serial.print(nbVisibleNetworks);
  Serial.println(" network(s) found");
  displayText(String(nbVisibleNetworks) + " network(s) found");

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
    displayText("no Known network identified. Reset to try again");
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here you found 1 known SSID
  // ----------------------------------------------------------------
  Serial.print(F("\nConnecting to "));
  Serial.println(KNOWN_SSID[n]);
  displayText(String("Connecting to ") + String(KNOWN_SSID[n]));
  

  // ----------------------------------------------------------------
  // We try to connect to the WiFi network we found
  // ----------------------------------------------------------------
  
  // Configures static IP address
  if (!WiFi.config(KNOWN_STATICIP[n], KNOWN_GATEWAY[n], subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static ip");
    displayText("Failed to configure static ip");
  }
  WiFi.begin(KNOWN_SSID[n], KNOWN_PASSWORD[n]);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    displayText("Connecting...");
  }
  Serial.println("");

  // ----------------------------------------------------------------
  // SUCCESS, you are connected to the known WiFi network
  // ----------------------------------------------------------------
  Serial.println(F("WiFi connected, your IP address is "));
  Serial.println(WiFi.localIP());
  displayText("Connected. IP address is: ");
  displayText((WiFi.localIP().toString()));
  
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
    blinkLed = false;
    setRGBColor(originalColor);
    DBG(F("Connected to session"), ssrc, name);
    displayText(String("Connected to session") + String(ssrc));
    doneConnecting = true;
    disconnected = false;
    message = "Connected to Session";
  });
  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    isConnected--;
    DBG(F("Disconnected"), ssrc);
    displayText(String("Disconnected ") +  String(ssrc));
    message = "DISCONNECTED FROM SESSION!";
    disconnected = true;
    blinkLed = true;
  });
  
  MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
    DBG(F("NoteOn"), note);
    
  });
  MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
    DBG(F("NoteOff"), note);
  });

  server.begin();

  
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{

   
  //update stored on time---------------------------------------------------------------------
  currentTime = millis();
  if (currentTime - lastCheckTime > checkOnTimeInterval) {
    lastCheckTime = currentTime;
    //update stored on time variable
    storedOnTime = storedOnTime + (checkOnTimeInterval / 1000); //store time elapsed in seconds
    preferences.putULong("ontime",storedOnTime);
    Serial.print("Stored On Time: ");
    Serial.println(storedOnTime);
  }
   
   unsigned long currentMillis = millis();

   if ((currentMillis - previousMillis >= blinkInterval) && blinkLed == true) {
      previousMillis = currentMillis;

      if (isLedOn) {
        isLedOn = false;
        setRGBColor("off");
        
      } else {
        isLedOn = true;
        setRGBColor(originalColor);
      }
      
   }
   
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
  

 
 
  //------------Button 6 --------------------------------
  if (debouncer2.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note2, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[5];
    lastButton = 5;
    Serial.println(F("note 2 on"));
  }
  else if (debouncer2.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note2, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 2 off"));
  }

 

  //----------Button 8-------------------
  if (debouncer4.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note4, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[7];
    lastButton = 7;
    Serial.println(F("note 4 on"));
  }
  else if (debouncer4.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note4, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 4 off"));
  }

  //----------Button 7 --------------------
  if (debouncer5.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note5, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[6];
    lastButton = 6;
    Serial.println(F("note 5 on"));
  }
  else if (debouncer5.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note5, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 5 off"));
  }

  
  //-------------Button 2----------------------
  if (debouncer13.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note13, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[1];
    lastButton = 1;
    Serial.println(F("note 13 on"));
  }
  else if (debouncer13.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note13, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 13 off"));
  }

  //------------Button 1------------------------
  if (debouncer14.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note14, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[0];
    lastButton = 0;
    Serial.println(F("note 14 on"));
  }
  else if (debouncer14.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note14, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 14 off"));
  }

  //-------------Button 3 -----------------------
  if (debouncer15.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note15, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[2];
    lastButton = 2;
    Serial.println(F("note 15 on"));
  }
  else if (debouncer15.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note15, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 15 off"));
  }

  //-------------Button 10-----------------------
  if (debouncer16.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note16, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[9];
    lastButton = 9;
    currentSong++;
    if (currentSong > numSongs -1) currentSong = 0;
    Serial.println(F("note 16 on"));
  }
  else if (debouncer16.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note16, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 16 off"));
  }

  //-------------Button 9 TUNER------------------------
  if (debouncer17.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note17, velocity, channel);
    setRGBColor("white");
    if (tunerActive) {
     if (disconnected == false) message = buttonText[lastButton];
     tunerActive = false;
    } else {
      if (disconnected == false) message = buttonText[8];
      
      tunerActive = true;
    }
    
    Serial.println(F("note 17 on"));
  }
  else if (debouncer17.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note17, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 17 off"));
  }

  //------------Button 4----------------------
  if (debouncer18.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note18, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[3];
    lastButton = 3;
    Serial.println(F("note 18 on"));
  }
  else if (debouncer18.rose()) {
    // button released so semd Note Off
    MIDI.sendNoteOff(note18, velocity, channel);
    setRGBColor(originalColor);
    Serial.println(F("note 18 off"));
  }

  //-------------Button 5-----------------------
  if (debouncer19.fell()) {
    // button pressed so send Note On
    MIDI.sendNoteOn(note19, velocity, channel);
    setRGBColor("white");
    if (disconnected == false) message = buttonText[4];
    lastButton = 4;
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
      //Serial.println(analogRead(expPin));
  }
  lastExpVal = newExpVal;


 //---CHECK BATTERY VOLTAGE CODE -------------------------------------------
//  if (currentMillis - previousMillisVoltage >= voltageCheckInterval) {
//    previousMillisVoltage = currentMillis;
//
//
//    //------------take multiple samples--------------------
//    int sampleSize = 10;
//    int sampleRate = 10;
//    int myArray[sampleSize];
//    float average = 0;
//    int maxVal = 0; //set low so any value read will be higher
//    int minVal = 4024; //set arbitrarily high so any value read will be lower
//    
//    for(int i=0; i < sampleSize; i++)
//    {
//      myArray[i] = analogRead(batteryVoltagePin);
//      Serial.print("Single Read: ");
//      Serial.println(myArray[i]);
//    
//      if(myArray[i] >= maxVal) {
//        maxVal = myArray[i];
//      }
//      
//      if(myArray[i] <= minVal) {
//        minVal = myArray[i];
//      }
//    
//      delay(sampleRate);
//    }
//    int sum = 0;
//    for(int i; i < sampleSize; i++)
//    {
//      sum = sum + myArray[i];
//    }
//    Serial.print("Min: ");
//    Serial.print(minVal);
//    Serial.print(" Max: ");
//    Serial.println(maxVal);
//    average = (sum - (maxVal+minVal))/(sampleSize-2);
//    Serial.print("Average discarding min/max: ");
//    Serial.println(average);
//    newVoltage = (sum / sampleSize);
//  
//        
//        float voltage = (float)(newVoltage/4096.0)*3.3*1.095;
//        float actualVoltage = voltage * 9.588;
//        Serial.print("Vpin Average Reading: ");
//        Serial.print(newVoltage);
//        Serial.print(" Voltage: ");
//        Serial.print(voltage);
//        Serial.print(" actualVoltage: ");
//        Serial.print(actualVoltage);
//        Serial.print(" Voltage Per Cell: ");
//        Serial.println(actualVoltage/4);
//        display.setCursor(0, (4*8));
//        display.setTextSize(2);
//        display.println(String(actualVoltage/4) + String("v"));
//        display.display();
//  }

  
  delay(10);

  //draw scrolling message on oled
  if (doneConnecting) {
    display.clearDisplay();
    display.setCursor(0,7);
    display.setTextSize(2);
    display.print(songs[currentSong]);
    display.setTextSize(3);
    display.setCursor(x,28);
    display.print(message);
    //draw bar showing expression pedal value on bottom screen
    display.fillRect(0, 55, newExpVal, 10,WHITE);
    display.display();
    x= x-4;
    int minX = -18 * message.length(); // 18 = 6 pixels/character * text size 3
    if (x < minX) x = display.width();
  }
   

 
  // Listen to incoming notes
  MIDI.read();


  //Wifi Server for button text code --------------------------------------------------
  WiFiClient client = server.available();   // Listen for incoming clients
   //declare name and value to use the request parameters and cookies
  char name[16], value[50];

    if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {            // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        httpReq.parseRequest(c);
        Serial.write(c);                    // print it out the serial monitor
        header += c;
                //IF request has ended -> handle response
        if (httpReq.endOfRequest()) {

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connnection: close");
          client.println();


          
            for(int i=1;i<=httpReq.paramCount;i++){
              httpReq.getParam(i,name,value);
              Serial.print(name);
              Serial.print("-");
              Serial.print(value);
              Serial.println("");

              if (String(name).indexOf("btn") >= 0) {
                String text = String(value);
                text.replace(String("+"),String(" "));
                preferences.putString(name,text);
                int btnNum = String(name).substring(3,5).toInt();
                Serial.print("button #: ");
                Serial.println(btnNum);
                buttonText[btnNum-1] = text;
              }

              if (String(name).indexOf("onTime") >= 0) {
                storedOnTime = int(atof(value) * 60 * 60);
                Serial.print("updated onTime: ");
                Serial.println(storedOnTime); 
                preferences.putULong("ontime",storedOnTime);
              }
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\">");
            client.println("</head><body><form action=\"/get\"><div class=\"container\"><div class=\"row\"><h1>Text To Display</h1></div>");
            for (int i=1;i<=10;i++) {
              client.println("<div class='form-group row'><label class='col-sm-2 col-form-label'>Btn" + String(i) + "</label><div class='col-sm-10'><input type=\"text\" class='form-control' name=\"btn" + String(i) + "\" value=\"" + buttonText[i-1] + "\"/></div></div>");
            }
            client.println("<div class='form-group row'><label class='col-sm-2 col-form-label'>On Time (hours)</label><input type=\"text\" class='form-control' name=\"onTime\" value=\"" + String((float)storedOnTime/60/60) + "\"/></div></div>");
            client.println("<input class='btn btn-primary btn-lg' type=\"submit\" value=\"Update Settings\"> ");
            client.println("</form></body></html>");
            // The HTTP response ends with another blank line
            client.println();
            
            Serial.println(buttonText[0]);
            message = buttonText[0];
            minX = -18 * message.length(); //12 = 6 pixels/character * text size 2

          //Reset object and free dynamic allocated memory
          httpReq.resetRequest();
          
          break;
        }
        

      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
     
  }
  //----------------------------------------------------------------------------------

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
    analogWrite(bluePin, 10);
  }

  if (color == "green") {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 10);
    analogWrite(bluePin, 0);
  }

  if (color == "white") {
    analogWrite(redPin, 10);
    analogWrite(greenPin, 10);
    analogWrite(bluePin, 10);
  }

    if (color == "off") {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  }
}

void displayText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  //shift all lines up 1 and get rid of first line
  for (int i = 0; i < 7; ++i) { 
    screenText[i] = screenText[i+1];
  }

  screenText[7] = text;
  
  for (int i = 0; i <= 7; ++i) {
    display.setCursor(0, (i*8));
    display.println(screenText[i]);
  }
  display.display();
}
