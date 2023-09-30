  
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <analogWrite.h>

#include <HttpRequest.h>
#include <Preferences.h>

#include <ardumidi.h>

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
const long checkOnTimeInterval = 60000; //60 seconds

//These are for the OLED Display --------------------------------------
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
String screenText[] = {"", "", "", "", "","","", ""};
String buttonText[10][8] = {{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""},{"","","","","","","",""}};
String songs[] = {"","","","","","","","","",""};
int currentSong = 0;
int numSongs = 0;
int x, minX;
int xArray[5];
int minXArray[5];
String message;
boolean doneConnecting = false;
boolean disconnected = false;
boolean tunerActive = false;
int lastButton = 0;

// Select I2C BUS
void SelectScreen(uint8_t bus){
  Wire.beginTransmission(0x70);  // TCA9548A address
  Wire.write(1 << bus);          // send byte to select bus
  Wire.endTransmission();
  //Serial.print(bus);
}
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


// DEFINE HERE THE KNOWN NETWORKS
const char* KNOWN_SSID[] = {"LL", "TellMyWifiLover","CoSWTP"};
const char* KNOWN_PASSWORD[] = {"billow11", "billow11","660MainStreet"};
const IPAddress KNOWN_STATICIP[] = {IPAddress(192,168,137,20), IPAddress(192,168,100,20), IPAddress(192,168,50,20)};
const IPAddress KNOWN_GATEWAY[] = {IPAddress(192,168,137,1), IPAddress(192,168,100,1), IPAddress(192,168,50,1)};
boolean wifiFound = false;
int i, n;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  //Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  //Serial.println("WiFi connected");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  //displayText(String("IP: ") + String(WiFi.localIP()));
  doneConnecting = true;
  disconnected = false;
  //message = "Connected to Session";
  message = String("IP: ") + String(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  //Serial.println("Disconnected from WiFi access point");
  //Serial.print("WiFi lost connection. Reason: ");
  setRGBColor("red");
 
  originalColor = "red";
  message = "DISCONNECTED FROM WIFI!";
  disconnected = true;
  //blinkLed = true;
  
  //Serial.println(info.disconnected.reason);
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
const int exp2Pin = 35;
const int batteryVoltagePin = 36;
//Initialize variables to read expression pedal status
int newExpVal = 0;
int lastExpVal = 0;
int newExp2Val = 0;
int newExp2ValPercent = 0;
int lastExp2Val = 0;

unsigned long t0 = millis();



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void setup()
{

   Serial.begin(115200);
   //DBG("Booting");

  //---------------------initialize oled displays---------------------------------------
  //Serial.println("Initializing OLED Displays");
  // Start I2C communication with the Multiplexer
  Wire.begin();
  for (i=0; i<=6; ++i) {
     SelectScreen(i);
     if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      //Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
  
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE,BLACK);
    if (i == 0) {
      display.setTextWrap(false);
    } else {
      display.setTextWrap(true);
    }

    xArray[i] = display.width();
    minXArray[i] = 0;
  
  }
  x = display.width();
  minX = 0;

  
  //Get time on from onboard storage
  preferences.begin("pedalboard", false);
  storedOnTime = preferences.getULong("ontime", 0);
  
  //Get button text from onboard storage
   preferences.begin("pedalboard", false);
  
  for (int i=1;i<=10;i++) {

    //populate songs
    String stringKey1 = "song" + String(i);
    char key1[7];
    stringKey1.toCharArray(key1,7);
    songs[i-1] = preferences.getString(key1,String(""));
    if (songs[i-1].length() > 0) numSongs += 1;
    //Serial.println("");
    //Serial.print("Song ");
    //Serial.print(i);
    //Serial.print(": ");
    //Serial.println(songs[i-1]);
    
    //populate button text
    String stringKey = "btn" + getPadded(i);
    char key[6];
    stringKey.toCharArray(key,6);
    for (int x=1;x<=8;x++) {
      String newStringKey = stringKey + getPadded(x);
      
      char newKey[8];
      newStringKey.toCharArray(newKey,8);
      //Serial.print("New Key: ");
      //Serial.println(newKey);
      buttonText[i-1][x-1] = preferences.getString(newKey,String(""));
 
      //Serial.print(" btn ");
      //Serial.print(x);
      //Serial.print(": ");
      //Serial.print(buttonText[i-1][x-1]);
    }
  
  }

  updateScreens();
  
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

  

 



  //-------------------------------------------NEW WIFI CODE ------------------------------------------
    // ----------------------------------------------------------------
  // WiFi.scanNetworks will return the number of networks found
  // ----------------------------------------------------------------
  //Serial.println(F("scan start"));
  displayText("scan start");
  
  int nbVisibleNetworks = WiFi.scanNetworks();
  //Serial.println(F("scan done"));
  if (nbVisibleNetworks == 0) {
    //Serial.println(F("no networks found. Reset to try again"));
    displayText("no networks found. Reset to try again");
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here at least some networks are visible
  // ----------------------------------------------------------------
  //Serial.print(nbVisibleNetworks);
  //Serial.println(" network(s) found");
  displayText(String(nbVisibleNetworks) + " network(s) found");

  // ----------------------------------------------------------------
  // check if we recognize one by comparing the visible networks
  // one by one with our list of known networks
  // ----------------------------------------------------------------
  for (i = 0; i < nbVisibleNetworks; ++i) {
    //Serial.println(WiFi.SSID(i)); // Print current SSID
    for (n = 0; n < KNOWN_SSID_COUNT; n++) { // walk through the list of known SSID and check for a match
      if (strcmp(KNOWN_SSID[n], WiFi.SSID(i).c_str())) {
        //Serial.print(F("\tNot matching "));
        //Serial.println(KNOWN_SSID[n]);
      } else { // we got a match
        wifiFound = true;
        break; // n is the network index we found
      }
    } // end for each known wifi SSID
    if (wifiFound) break; // break from the "for each visible network" loop
  } // end for each visible network

  if (!wifiFound) {
    //Serial.println(F("no Known network identified. Reset to try again"));
    displayText("no Known network identified. Reset to try again");
    while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  }

  // ----------------------------------------------------------------
  // if you arrive here you found 1 known SSID
  // ----------------------------------------------------------------
  //Serial.print(F("\nConnecting to "));
  //Serial.println(KNOWN_SSID[n]);
  displayText(String("Connecting to ") + String(KNOWN_SSID[n]));
  

  // ----------------------------------------------------------------
  // We try to connect to the WiFi network we found
  // ----------------------------------------------------------------
  
  // Configures static IP address
  if (!WiFi.config(KNOWN_STATICIP[n], KNOWN_GATEWAY[n], subnet, primaryDNS, secondaryDNS)) {
    //Serial.println("Failed to configure static ip");
    displayText("Failed to configure static ip");
  }
  WiFi.begin(KNOWN_SSID[n], KNOWN_PASSWORD[n]);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    //Serial.print(".");
    displayText("Connecting...");
  }
  //Serial.println("");

  // ----------------------------------------------------------------
  // SUCCESS, you are connected to the known WiFi network
  // ----------------------------------------------------------------
  //Serial.println(F("WiFi connected, your IP address is "));
  //Serial.println(WiFi.localIP());
  displayText("Connected. IP address is: ");
  displayText((WiFi.localIP().toString()));
  doneConnecting = true;
  disconnected = false;
  //message = "Connected to Session";
  message = String(WiFi.localIP().toString() + String(":8888"));
  
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
    //Serial.print("Stored On Time: ");
    //Serial.println(storedOnTime);
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

  
  
  byte note21 = 21;  //1
  byte note22 = 22;  //2
  byte note23 = 23;  //3
  byte note24 = 24; //4
  byte note25 = 25; //5
  byte note26 = 26; //6
  byte note16 = 27; //7
  byte note17 = 28; //8
  byte note18 = 29; //9
  byte note19 = 30; //10
  

 
 
  //------------Button 4 --------------------------------
  if (debouncer14.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note24,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][5];
    lastButton = 3;
    updateScreens();
    //Serial.println(F("button on"));
  }
  else if (debouncer14.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note24,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("button 4 off"));
  }

 

  //----------Button 3-------------------
  if (debouncer4.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note23,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][7];
    lastButton = 2;
    updateScreens();
    //Serial.println(F("button 3 on"));
  }
  else if (debouncer4.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note23,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("button 3 off"));
  }

  //----------Button 2 --------------------
  if (debouncer5.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note22,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][6];
    lastButton = 1;
    updateScreens();
    //Serial.println(F("button 2 on"));
  }
  else if (debouncer5.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note22,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("button 2 off"));
  }

  
  //-------------Button 5----------------------
  if (debouncer13.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note25,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][1];
    lastButton = 4;
    updateScreens();
    //Serial.println(F("button 5 on"));
  }
  else if (debouncer13.rose()) {
    // button released so semd Note Off
    setRGBColor(originalColor);
    //Serial.println(F("button 5 off"));
  }

  //------------Button 1------------------------
  if (debouncer2.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note21,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][0];
    lastButton = 0;
    updateScreens();
    //Serial.println(F("button 1 on"));
  }
  else if (debouncer2.rose()) {
    // button released so semd Note Off
    setRGBColor(originalColor);
    //Serial.println(F("button 1  off"));
  }

  //-------------Button 6 -----------------------
  if (debouncer15.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note26,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][2];
    lastButton = 5;
    updateScreens();
    //Serial.println(F("button 6 on"));
  }
  else if (debouncer15.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note26,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("button 6 off"));
  }

  //-------------Button 10 NEXT SONG-----------------------
  if (debouncer19.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note16,velocity);
    setRGBColor("white");
    lastButton = 9;
    currentSong++;
    if (currentSong > numSongs -1) currentSong = 0;
    if (disconnected == false) message = songs[currentSong];
    updateScreens();
    //Serial.println(F("Next Song on"));
  }
  else if (debouncer19.rose()) {
    // button released so send Note Off
    midi_note_off(channel,note16,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("Next Song off"));
  }

  //-------------Button 9 TUNER------------------------
  if (debouncer18.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note17,velocity);
    //setRGBColor("white");
    if (tunerActive) {
     if (disconnected == false) message = songs[currentSong];
     tunerActive = false;
    } else {
      if (disconnected == false) message = "TUNER";
      
      tunerActive = true;
    }
    
    //Serial.println(F("Tuner on"));
  }
  else if (debouncer18.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note17,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("Tuner off"));
  }

  //------------Button ? ----------------------
  if (debouncer17.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note18,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][3];
    //lastButton = 3;
    //updateScreens();
    //Serial.println(F("note 17 on"));
  }
  else if (debouncer17.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note18,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("note 17 off"));
  }

  //------------- Button ?-----------------------
  if (debouncer16.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note19,velocity);
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][4];
    //lastButton = 4;
    //updateScreens();
    //Serial.println(F("note 19 on"));
  }
  else if (debouncer16.rose()) {
    // button released so semd Note Off
    midi_note_off(channel,note19,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("note 19 off"));
  }

  //PROCESS EXPRESSION PEDAL...
  newExpVal = analogRead(expPin);
  newExpVal = map(newExpVal, 0, 4095, 0, 127);
  newExpVal = constrain(newExpVal, 0, 127);
  if (newExpVal != lastExpVal) {
      midi_note_on(channel,31,newExpVal);
      //Serial.println(newExpVal);
      //Serial.println(analogRead(expPin));
  }
  lastExpVal = newExpVal;

  //PROCESS EXPRESSION PEDAL 2...
  newExp2Val = analogRead(exp2Pin);
  newExp2ValPercent = analogRead(exp2Pin);
  newExp2Val = map(newExp2Val, 0, 4095, 0, 127);
  newExp2Val = constrain(newExp2Val, 0, 127);
 
  if (newExp2Val != lastExp2Val) {
      midi_note_on(channel,32,newExp2Val);
      newExp2ValPercent = map(newExp2ValPercent, 0, 4095, 0, 100);
      newExp2ValPercent = constrain(newExp2ValPercent, 0, 100);
      message = newExp2ValPercent;
      //Serial.println(newExp2Val);
      //Serial.println(analogRead(exp2Pin));
  }
  lastExp2Val = newExp2Val;


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
//      //Serial.print("Single Read: ");
//      //Serial.println(myArray[i]);
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
//    //Serial.print("Min: ");
//    //Serial.print(minVal);
//    //Serial.print(" Max: ");
//    //Serial.println(maxVal);
//    average = (sum - (maxVal+minVal))/(sampleSize-2);
//    //Serial.print("Average discarding min/max: ");
//    //Serial.println(average);
//    newVoltage = (sum / sampleSize);
//  
//        
//        float voltage = (float)(newVoltage/4096.0)*3.3*1.095;
//        float actualVoltage = voltage * 9.588;
//        //Serial.print("Vpin Average Reading: ");
//        //Serial.print(newVoltage);
//        //Serial.print(" Voltage: ");
//        //Serial.print(voltage);
//        //Serial.print(" actualVoltage: ");
//        //Serial.print(actualVoltage);
//        //Serial.print(" Voltage Per Cell: ");
//        //Serial.println(actualVoltage/4);
//        display.setCursor(0, (4*8));
//        display.setTextSize(2);
//        display.println(String(actualVoltage/4) + String("v"));
//        display.display();
//  }

  
  delay(10);

  //draw scrolling message on oled
  if (doneConnecting) {
    SelectScreen(0);
    display.clearDisplay();
    display.setTextWrap(false);
    display.setCursor(0,7);
    display.setTextSize(2);
    display.print(songs[currentSong]);
    display.setTextSize(3);
    //disable scrolling for exp2
    //display.setCursor(x,28);
    display.setCursor(20,28);
    display.print(message);
    display.setTextSize(1);
    //show onTime
    display.setCursor(56,56);
    display.print(String((float)storedOnTime/60/60));
    //draw bar showing expression pedal value on bottom screen
    display.fillRect(0, 55, newExpVal, 10,WHITE);
    display.display();
    x= x-6;
    int minX = -18 * message.length(); // 18 = 6 pixels/character * text size 3
    if (x < minX) x = display.width();


  }
   

 
  


  //Wifi Server for button text code --------------------------------------------------
  WiFiClient client = server.available();   // Listen for incoming clients
   //declare name and value to use the request parameters and cookies
  char name[16], value[50];

    if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    //Serial.println("New Client.");          // print a message out in the serial port
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

          if (httpReq.paramCount > 0) numSongs = 0;
          int songNum, btnNum;
          String substr;
          
            for(int i=1;i<=httpReq.paramCount;i++){
              httpReq.getParam(i,name,value);
              //Serial.print(name);
              //Serial.print(" - ");
              //Serial.print(value);
              //Serial.println("");

              if (String(name).indexOf("btn") >= 0) {
                String text = String(value);
                text.replace(String("+"),String(" "));
                preferences.putString(name,text);
                songNum = String(name).substring(3,5).toInt();
                btnNum = String(name).substring(5,7).toInt();
                buttonText[songNum-1][btnNum-1] = text;
              }

               if (String(name).indexOf("song") >= 0) {
                String text = String(value);
                text.replace(String("+"),String(" "));
                //first param is glitched with a leading carriage return! hacky fix
                if (name[0] == 's') {
                  preferences.putString(name,text);
                } else {
                  char fixedName[6];
                  fixedName[0] = name[1];
                  fixedName[1] = name[2];
                  fixedName[2] = name[3];
                  fixedName[3] = name[4];
                  fixedName[4] = name[5];
                  fixedName[5] = '\0'; // The terminating NULL
                  preferences.putString(fixedName,text);
                }
                
               
                char sub[3];
                //first param is glitched with a leading carriage return! hacky fix
                if (name[0] == 's') {
                  sub[0] = name[4];
                  sub[1] = name[5];
                } else {
                  sub[0] = name[5];
                  sub[1] = name[6];
                }
                
                sub[2] = '\0'; // The terminating NULL
                songNum = String(sub).toInt();
                songs[songNum-1] = text;
                if (text.length() > 0) numSongs += 1;
              }

              if (String(name).indexOf("onTime") >= 0) {
                storedOnTime = int(atof(value) * 60 * 60);
                //Serial.print("updated onTime: ");
                //Serial.println(storedOnTime); 
                preferences.putULong("ontime",storedOnTime);
              }
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\">");
            client.println("</head><body><form action=\"/post\" method=\"post\"><div class=\"container\">");
            client.println("<div class='row'><div class='col-sm-12'><h2>Button Text</h2></div></div>");
            for (int i=1;i<=10;i++) {
              client.println("<div class='row'><div class='col-sm-12'><h2>Song " + String(i) + "</h2></div></div>");
              client.println("<div class='form-group row'><div class='input-group col-sm-12 col-md-6'><div class='input-group-prepend'><div class='input-group-text'>Song " + String(i) + "</div></div><input type=\"text\" class='form-control' name=\"song" + String(i) + "\" value=\"" + songs[i-1] + "\"/></div></div>");
               for (int x=1;x<=8;x++) {
                  client.println("<div class='form-group row'><div class='input-group col-sm-12 col-md-6'><div class='input-group-prepend'><div class='input-group-text'>" + String(x) + "</div></div><input type=\"text\" class='form-control' name=\"btn" + getPadded(i) + getPadded(x) + "\" value=\"" + buttonText[i-1][x-1] + "\"/></div></div>");
            
              }
           }
            
           
            client.println("<div class='form-group row'><label class='col-xs-2 col-form-label'>On Time (hours)</label><div class='col-xs-10'><input type=\"text\" class='form-control' name=\"onTime\" value=\"" + String((float)storedOnTime/60/60) + "\"/></div></div>");
            client.println("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-primary btn-lg' type=\"submit\" value=\"Update Settings\"></div></div> ");
            client.println("</form></body></html>");
            // The HTTP response ends with another blank line
            client.println();
            
            //Serial.println(buttonText[0][0]);
            //message = buttonText[0][0];
            updateScreens();
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
    //Serial.println("Client disconnected.");
    //Serial.println("");
     
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
  SelectScreen(0);
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

//update each button screen
void updateScreens() {
   
    for (i=0;i<=6;++i) {
      SelectScreen(i+1);
      display.clearDisplay();
      display.setTextWrap(true );
      //show white bar if this button is active
      display.setCursor(0,0);
      if (lastButton == i) {
        display.fillRect(0, 0, 128, 64,WHITE);
        display.setTextColor(BLACK, WHITE);
      } else {
        display.setTextColor(WHITE,BLACK);
      }
      
      int textSize = 4;
      if (buttonText[currentSong][i].length() < 5) {
        textSize = 5;
      }
      if (buttonText[currentSong][i].length() > 5) {
        textSize = 3;
      }
      if (buttonText[currentSong][i].length() > 14) {
        textSize = 2;
      }
      display.setTextSize(textSize);
      display.setCursor(2,2);
      display.print(buttonText[currentSong][i]);
      display.display();
    }
}

//get padded string from int
String getPadded(int num) {
  char buff[3];
  char padded[4];
  
  //sprintf function will convert the long to a string
  sprintf(buff, "%.2u", num); // buff will be "01238"

  padded[0] = buff[0];
  padded[1] = buff[1];
  padded[2] = buff[2];
  padded[3] = buff[3];
  padded[4] = '\0'; // The terminating NULL

  return String(padded);
}
