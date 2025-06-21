  
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <analogWrite.h>
#include <ArduinoOTA.h>




//Use this to format preferences storage if there's an issue
#include <nvs_flash.h>

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
//String buttonText[25][6] = {{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","","",},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""},{"","","","","",""}};
String buttonText[25][6];
boolean buttonState[] = {true,true,true,true,true,true};
String songs[] = {"","","","","","","","","","","","","","","","","","","","","","","","",""};
String connectedMessage = "offline";
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


//New code to receive Sysex MIDI ------------------------------------------------------------

#define SYSEX_BUFFER_MAX_LENGTH 128 // Maximum SysEx data bytes (between F0 & F7) we can handle
byte sysexBuffer[SYSEX_BUFFER_MAX_LENGTH];
int sysexBytesReceived = 0;
bool inSysexMessage = false;

// Your SysEx Protocol Constants (numeric byte values)
const byte PEDALBOARD_MANUF_ID_FROM_GP = 0x7D;
const byte PEDALBOARD_DEVICE_ID_FROM_GP = 0x01;
const byte CMD_SET_SONG_NAME_FROM_GP = 0x01;
const byte CMD_SET_BUTTON_NAME_FROM_GP = 0x02;

// Maximum length for song/button names stored on ESP32 (affects char buffer for parsing)
// This should accommodate the longest name you expect, plus null terminator.
// Your GP script truncates to 30, so 30 + 1 for null is safe.
const int MAX_NAME_LENGTH_FROM_SYSEX = 31; 
// ---------------------------------------------------------------------------

// This function will be called when a complete SysEx message (F0...F7) has been received
// The sysexBuffer will contain the data *between* F0 and F7.
// sysexBytesReceived will be the count of these data bytes.
// ---------------------------------------------------------------------------
void parseAndApplySysex() {
    if (sysexBytesReceived < 4) { /* ... */ return; }
    if (sysexBuffer[0] != PEDALBOARD_MANUF_ID_FROM_GP || sysexBuffer[1] != PEDALBOARD_DEVICE_ID_FROM_GP) { /* ... */ return; }

    byte command = sysexBuffer[2];
    byte songIdx = sysexBuffer[3];

    if (songIdx >= 25) { /* ... */ return; }

    char nameBuffer[MAX_NAME_LENGTH_FROM_SYSEX];
    int textStartIndex;
    int actualTextLengthInPayload;

    if (command == CMD_SET_SONG_NAME_FROM_GP) {
        textStartIndex = 4;
        if (sysexBytesReceived < textStartIndex) {
             songs[songIdx] = ""; 
        } else {
            actualTextLengthInPayload = sysexBytesReceived - textStartIndex;
            int charsToCopy = 0;
            for (int k = 0; k < actualTextLengthInPayload; k++) {
                if (charsToCopy < MAX_NAME_LENGTH_FROM_SYSEX - 1) {
                    nameBuffer[charsToCopy++] = (char)sysexBuffer[textStartIndex + k];
                } else { break; }
            }
            nameBuffer[charsToCopy] = '\0';
            songs[songIdx] = String(nameBuffer);
        }
        
        // Serial.print(F("SysEx: Set Song[")); Serial.print(songIdx); Serial.print(F("] to '")); Serial.print(songs[songIdx]); Serial.println(F("'"));

        // --- Revised numSongs logic ---
        // If a name was set for an index >= current numSongs, update numSongs.
        // Or, always recount to find the highest populated song index.
        // Let's try a robust recount.
        int highestPopulatedSong = -1;
        for (int k = 0; k < 25; k++) {
            if (!songs[k].isEmpty() && !(k==0 && songs[k]=="Pedalboard" && songs[k].length()==10 && k>0 && songs[k-1].isEmpty()) ) { // Check if it's not just a default placeholder for higher slots if lower ones are empty
                // More robust: check if songs[k] is not the default "Song X" placeholder
                String defaultPlaceholder = "Song " + String(k + 1);
                if (k > 0 && songs[k] == defaultPlaceholder && songs[k-1].isEmpty()){
                     // This is likely an unpopulated default, don't count it yet unless explicitly set.
                } else if (k == 0 && songs[k] == "Pedalboard" && songs[1].isEmpty() && songs[1] == "Song 2") {
                    // Only pedalboard mode, and next is default placeholder.
                }
                else {
                   highestPopulatedSong = k;
                }
            }
        }
        numSongs = highestPopulatedSong + 1;
        if (numSongs == 0 && !songs[0].isEmpty()) { // If only slot 0 has a name (e.g. "Pedalboard")
            numSongs = 1;
        } else if (numSongs == 0 && songs[0].isEmpty()){ // Truly empty
            songs[0] = "Pedalboard"; // fallback
            numSongs = 1;
        }


        // Ensure currentSong is valid after numSongs might have changed
        if (currentSong >= numSongs && numSongs > 0) {
            currentSong = numSongs - 1;
        } else if (numSongs == 0) { // Should not happen if we default songs[0]
            currentSong = 0;
        }
        // --- End revised numSongs logic ---

    } else if (command == CMD_SET_BUTTON_NAME_FROM_GP) {
        // ... (button name parsing remains the same) ...
        textStartIndex = 5; 
        if (sysexBytesReceived < textStartIndex) { return; }
        byte buttonIdx = sysexBuffer[4];
        if (buttonIdx >= 6) { return; }

        if (sysexBytesReceived < textStartIndex ) { 
            buttonText[songIdx][buttonIdx] = ""; 
        } else {
            actualTextLengthInPayload = sysexBytesReceived - textStartIndex;
            int charsToCopy = 0;
            for (int k = 0; k < actualTextLengthInPayload; k++) {
                if (charsToCopy < MAX_NAME_LENGTH_FROM_SYSEX - 1) {
                    nameBuffer[charsToCopy++] = (char)sysexBuffer[textStartIndex + k];
                } else { break; }
            }
            nameBuffer[charsToCopy] = '\0';
            buttonText[songIdx][buttonIdx] = String(nameBuffer);
        }
        // Serial.print(F("SysEx: Set buttonText[")); // ...
    } else {
        // Serial.print(F("SysEx: Unknown command: 0x")); Serial.println(command, HEX);
        return;
    }

    if (songIdx == currentSong) {
        message = songs[currentSong]; 
        lastButton = -1; 
        updateScreens(); 
    }
}

//End Sysex MIDI Code-----------------------------------------------------------------------



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
Bounce debouncer27 = Bounce();

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
const char* KNOWN_SSID[] = {"LL", "TellMyWifiLover","CoS"};
const char* KNOWN_PASSWORD[] = {"billow11","billow11","225Reimer"};
const IPAddress KNOWN_STATICIP[] = {IPAddress(192,168,137,20), IPAddress(192,168,100,22), IPAddress(192,168,50,20)};
const IPAddress KNOWN_GATEWAY[] = {IPAddress(192,168,137,1), IPAddress(192,168,100,254), IPAddress(192,168,50,1)};
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

  
 
  

  
  //Get song and button text from onboard storage
  //Data is stored in preferences "pedalboard" namespace
  //preferences.begin("pedalboard", false);

 numSongs = 25;

 for (int i = 0; i < 25; i++) {
    if (i == 0) {
        songs[i] = "Pedalboard"; // Default for the first "song"
    } else {
        songs[i] = "Song " + String(i+1); // Placeholder name
    }
    for (int j = 0; j < 6; j++) {
        buttonText[i][j] = "Btn " + String(j+1); // Placeholder button text
    }
 }

 message = songs[currentSong];
 

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

  pinMode(27, INPUT_PULLUP);
  debouncer27.attach(27);
  debouncer27.interval(5); // interval in ms

  // configure LED PWM resolution/range and set pins to LOW
  analogWrite(redPin, 0);
  analogWrite(greenPin, 0);
  analogWrite(bluePin, 0);

  //setRGBColor("red");

  

 



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
    //while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
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
    doneConnecting = true;
    disconnected = false;
    //while (true); // no need to go further, hang in there, will auto launch the Soft WDT reset
  } else {
  
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
      WiFi.begin(KNOWN_SSID[n],KNOWN_PASSWORD[n]);
    
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
      connectedMessage = String(WiFi.localIP().toString() + String(":8888"));
      
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


         //Begin OTA Code -----------------------------------------------------------
     ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  //END OTA CODE --------------------------------------------------------------------------------
  } // if (!wifiFound)

  

  
}


// Function to process incoming MIDI messages from USB Serial (Hairless MIDI)
void processMidiInput() {
    static byte incomingByte;
    static byte statusByte = 0; // For regular MIDI messages
    static byte dataByte1 = 0;  // For regular MIDI messages
    static int regularMessageState = 0; // 0: waiting for status, 1: data1, 2: data2

    while (Serial.available() > 0) {
        incomingByte = Serial.read();

        if (inSysexMessage) {
            if (incomingByte == 0xF7) { // SysEx End
                // A complete SysEx message (excluding F0 and F7) is now in sysexBuffer
                // Print for debug:
                // Serial.print(F("SysEx End. Data bytes received: ")); Serial.println(sysexBytesReceived);
                // Serial.print(F("Buffer: "));
                // for(int i=0; i<sysexBytesReceived; i++) { Serial.print(sysexBuffer[i], HEX); Serial.print(" "); }
                // Serial.println();
                
                parseAndApplySysex(); // Process the buffered SysEx data
                
                inSysexMessage = false;
                sysexBytesReceived = 0; // Reset for next message
            } else if (sysexBytesReceived < SYSEX_BUFFER_MAX_LENGTH) {
                // Store data bytes (those between F0 and F7)
                if (incomingByte < 0x80) { // Valid SysEx data byte
                    sysexBuffer[sysexBytesReceived++] = incomingByte;
                } else {
                    // Invalid byte within SysEx (e.g., another status byte before F7), abort current SysEx
                    // Serial.print(F("SysEx Aborted: Invalid data byte 0x")); Serial.println(incomingByte, HEX);
                    inSysexMessage = false;
                    sysexBytesReceived = 0;
                    // This incomingByte might be a new status byte, so let it fall through
                    // to regular MIDI processing *if* this byte itself is >= 0x80
                }
            } else {
                // SysEx buffer overflow, message too long for our buffer
                // Serial.println(F("SysEx Buffer Overflow. Aborting current SysEx."));
                inSysexMessage = false; // Stop collecting this message
                sysexBytesReceived = 0; // Reset buffer
                // The current incomingByte is discarded as part of the overflowed message
            }
             // If SysEx just ended or was aborted, and the current byte is NOT a new status byte, consume it.
            if (!inSysexMessage && incomingByte < 0x80) {
                continue; 
            }
        }
        
        // This 'if' must be separate to handle fall-through from SysEx abortion if a status byte caused the abort.
        if (!inSysexMessage) { 
            if (incomingByte == 0xF0) { // SysEx Start
                inSysexMessage = true;
                sysexBytesReceived = 0; // Clear buffer for new message
                statusByte = 0;         // Clear regular MIDI message status
                regularMessageState = 0;
                // Serial.println(F("SysEx Start detected."));
            } else if (incomingByte >= 0xF8) { // Real-time messages (Timing Clock, Start, Stop, etc.)
                // Typically ignore these for this application or handle separately if needed
                continue; 
            } else if (incomingByte >= 0xF1 && incomingByte <= 0xF6 ) { // Other System Common (MTC, Song Select, Tune Req)
                // Usually ignored, reset regular message state
                statusByte = 0;
                regularMessageState = 0;
                continue;
            } else if (incomingByte >= 0x80) { // Regular MIDI Status byte (NoteOn, NoteOff, CC, PC, etc.)
                statusByte = incomingByte;
                regularMessageState = 1; // Expecting dataByte1 next
            } else if (statusByte != 0) { // Data byte for a regular MIDI message (running status active)
                if (regularMessageState == 1) { // This is dataByte1
                    dataByte1 = incomingByte;
                    // For 2-byte messages like PC or ChanPressure, process here if needed.
                    // For 3-byte messages, wait for dataByte2.
                    byte commandType = statusByte & 0xF0;
                    if (commandType == 0xC0 || commandType == 0xD0) { // PC or Channel Pressure (2-byte messages)
                        // Process 2-byte message if necessary
                        // Example: if (commandType == 0xC0 && (statusByte & 0x0F) == MY_PC_CHANNEL) { processProgramChange(dataByte1); }
                        regularMessageState = 1; // Ready for next dataByte1 (running status) or new status
                    } else {
                        regularMessageState = 2; // Expecting dataByte2 next (for 3-byte messages)
                    }
                } else if (regularMessageState == 2) { // This is dataByte2
                    // Now we have a complete 3-byte message (statusByte, dataByte1, incomingByte as dataByte2)
                    byte command = statusByte & 0xF0;
                    byte midiChannel = statusByte & 0x0F; // 0-15

                    if (command == 0x90 && midiChannel == 2 && incomingByte > 0) { // Note On, MIDI Channel 3 (index 2), velocity > 0
                        int newSongCandidate = dataByte1; // Note number
                        
                        if (newSongCandidate >= 0 && newSongCandidate < 25) { // Always allow selection within physical slot limits (0-24)
                            currentSong = newSongCandidate; // Set currentSong immediately
                            // Serial.print(F("MIDI NoteOn: Set currentSong to ")); Serial.println(currentSong);
                            
                            // Update message based on potentially SysEx-updated song name
                            // If songs[currentSong] is empty or a placeholder, SysEx will hopefully fill it soon.
                            if (!songs[currentSong].isEmpty()) {
                                message = songs[currentSong];
                            } else {
                                message = "Song " + String(currentSong + 1); // Temporary placeholder
                            }
                            
                            lastButton = -1; 
                            updateScreens(); 
                            //setRGBColor("white"); delay(50); setRGBColor(originalColor);
                        }
                    }
                    // Add other 3-byte message handling here (Note Off 0x80, CC 0xB0, PitchBend 0xE0, PolyPressure 0xA0) if needed.
                    
                    regularMessageState = 1; // After a 3-byte message, next data byte would be dataByte1 for running status
                }
            }
            // If incomingByte < 0x80 and statusByte is 0, it's an orphaned data byte, ignore.
        } // end if (!inSysexMessage) block
    } // end while (Serial.available())
}

void loop()
{

  ArduinoOTA.handle();

  processMidiInput();

   
  //update stored on time NO LONGER TRACKING ON TIME---------------------------------------------------------------------
  currentTime = millis();
  //if (currentTime - lastCheckTime > checkOnTimeInterval) {
    //lastCheckTime = currentTime;
    //update stored on time variable
    //storedOnTime = storedOnTime + (checkOnTimeInterval / 1000); //store time elapsed in seconds
    //preferences.putULong("ontime",storedOnTime);
    //Serial.print("Stored On Time: ");
    //Serial.println(storedOnTime);
  //}
   
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
  debouncer27.update();
  
 
  

  

  
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
  byte note31 = 31; //11

  
 
  //------------Button 4 --------------------------------
  if (debouncer14.fell()) {
    // button pressed so send Note On
    //switch is toggling velocity on pedalboard mode
    if (currentSong == 0) {
      //if (buttonState[3] == true) {
      //  velocity = 0;
      //} else {
      //  velocity = 127;
      //}
    } else {
      velocity = 55;
    }
    if (currentSong == 0) {
      midi_note_on(channel,44,velocity);
    } else {
      midi_note_on(channel,note24,velocity);
    }
    
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][5];
    lastButton = 3;
    updateScreens();
    //Serial.println(F("button on"));
  }
  else if (debouncer14.rose()) {
    // button released so semd Note Off
    if (currentSong == 0) {
      midi_note_off(channel,44,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("button 4 off"));
  }

 

  //----------Button 3-------------------
  if (debouncer4.fell()) {
    // button pressed so send Note On
        //switch is toggling velocity on pedalboard mode
    
      velocity = 55;
    
 
    if (currentSong == 0) {
      midi_note_on(channel,43,velocity);
    } else {
      midi_note_on(channel,note23,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][7];
    lastButton = 2;
    updateScreens();
    //Serial.println(F("button 3 on"));
  }
  else if (debouncer4.rose()) {
    // button released so semd Note Off
     if (currentSong == 0) {
      midi_note_off(channel,43,velocity);
    }
    //midi_note_off(channel,note23,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("button 3 off"));
  }

  //----------Button 2 --------------------
  if (debouncer5.fell()) {
    // button pressed so send Note On
    //switch is toggling velocity on pedalboard mode
    
    velocity = 55;
    
   
    if (currentSong == 0) {
      midi_note_on(channel,42,velocity);
    } else {
      midi_note_on(channel,note22,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][6];
    lastButton = 1;
    updateScreens();
    //Serial.println(F("button 2 on"));
  }
  else if (debouncer5.rose()) {
    // button released so semd Note Off
    //midi_note_off(channel,note22,velocity);
    if (currentSong == 0) {
      midi_note_off(channel,42,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("button 2 off"));
  }

  
  //-------------Button 5----------------------
  if (debouncer13.fell()) {
    // button pressed so send Note On
    
    //switch is toggling velocity on pedalboard mode
   
   velocity = 55;
   
    
    if (currentSong == 0) {
      midi_note_on(channel,45,velocity);
    } else {
      midi_note_on(channel,note25,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][1];
    lastButton = 4;
    updateScreens();
    //Serial.println(F("button 5 on"));
  }
  else if (debouncer13.rose()) {
    // button released so semd Note Off
    if (currentSong == 0) {
      midi_note_off(channel,45,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("button 5 off"));
  }

  //------------Button 1------------------------
  if (debouncer2.fell()) {
    // button pressed so send Note On
    
    //switch is toggling velocity on pedalboard mode
    velocity = 55;
    
    
    if (currentSong == 0) {
      midi_note_on(channel,41,velocity);
    } else {
      midi_note_on(channel,note21,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][0];
    lastButton = 0;
    updateScreens();
    //Serial.println(F("button 1 on"));
  }
  else if (debouncer2.rose()) {
    // button released so semd Note Off
    if (currentSong == 0) {
      midi_note_off(channel,41,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("button 1  off"));
  }

  //-------------Button 6 -----------------------
  if (debouncer15.fell()) {
    // button pressed so send Note On
    //switch is toggling velocity on pedalboard mode
   
    velocity = 55;
    
    
    if (currentSong == 0) {
      midi_note_on(channel,46,velocity);
    } else {
      midi_note_on(channel,note26,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][2];
    lastButton = 5;
    updateScreens();
    //Serial.println(F("button 6 on"));
  }
  else if (debouncer15.rose()) {
    // button released so semd Note Off
    //midi_note_off(channel,note26,velocity);
    if (currentSong == 0) {
      midi_note_off(channel,46,velocity);
    }
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
    if (currentSong > numSongs -1){
      
      //song 0 is pedalboard mode. Redraw all screens manually.
      currentSong = numSongs -1;
      if (disconnected == false) message = songs[currentSong];
     
      lastButton = 9;
    } else {
      if (disconnected == false) message = songs[currentSong];
       updateScreens();
    }
    //if (disconnected == false) message = songs[currentSong];
   
    //Serial.println(F("Next Song on"));
  }
  else if (debouncer19.rose()) {
    // button released so send Note Off
    midi_note_off(channel,note16,velocity);
    setRGBColor(originalColor);
    //Serial.println(F("Next Song off"));
  }


    //-------------Button 11 PREVIOUS SONG-----------------------
  if (debouncer27.fell()) {
    // button pressed so send Note On
    midi_note_on(channel,note31,velocity);
    setRGBColor("white");
    lastButton = 10;
    currentSong--;
    if (currentSong == 0){
      
      //song 0 is pedalboard mode. Redraw all screens manually.
      
      if (disconnected == false) message = songs[currentSong];
      //redraw all button screens
      for (int y=0;y<=5;++y) {
        buttonState[y] = !buttonState[y];
        lastButton = y;
        updateScreens();
      }
      lastButton = 9;
    } else {
      //stay on first song if we are trying to go back. We are no longer cycling through
      if (currentSong < 0) {
        currentSong = 0;
      }
      if (disconnected == false) message = songs[currentSong];
       updateScreens();
    }
    //if (disconnected == false) message = songs[currentSong];
   
    //Serial.println(F("Next Song on"));
  }
  else if (debouncer27.rose()) {
    // button released so send Note Off
    midi_note_off(channel,note31,velocity);
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
    if (currentSong == 0) {
      midi_note_on(channel,48,velocity);
    } else {
      midi_note_on(channel,note18,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][3];
    //lastButton = 3;
    //updateScreens();
    //Serial.println(F("note 17 on"));
  }
  else if (debouncer17.rose()) {
    // button released so semd Note Off
    if (currentSong == 0) {
      midi_note_off(channel,48,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("note 17 off"));
  }

  //------------- Button ?-----------------------
  if (debouncer16.fell()) {
    // button pressed so send Note On
    if (currentSong == 0) {
      midi_note_on(channel,49,velocity);
    } else {
      midi_note_on(channel,note19,velocity);
    }
    setRGBColor("white");
    //if (disconnected == false) message = buttonText[currentSong][4];
    //lastButton = 4;
    //updateScreens();
    //Serial.println(F("note 19 on"));
  }
  else if (debouncer16.rose()) {
    // button released so semd Note Off
    if (currentSong == 0) {
      midi_note_off(channel,49,velocity);
    }
    setRGBColor(originalColor);
    //Serial.println(F("note 19 off"));
  }

  //PROCESS EXPRESSION PEDAL...
  newExpVal = analogRead(expPin);
  newExpVal = map(newExpVal, 0, 4095, 0, 127);
  newExpVal = constrain(newExpVal, 0, 127);
  if (newExpVal != lastExpVal) {
      //midi_note_on(channel,31,newExpVal);
      midi_controller_change(channel,16,newExpVal);
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
      //midi_note_on(channel,33,newExp2Val);
      midi_controller_change(channel,17,newExp2Val);
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
    display.setCursor(0,56);
    //display.print(String((float)storedOnTime/60/60));
    display.print(connectedMessage);
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
  char name[16], value[25];

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
        //Serial.write(c);                    // print it out the serial monitor
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

              if (String(name).indexOf("clearStorage") >= 0) {
                if (String(value).indexOf("yes") >= 0) {
                //Use this to format preferences storage if there's an issue
                nvs_flash_erase(); // erase the NVS partition and...
                nvs_flash_init(); // initialize the NVS partition.
                ESP.restart(); //Restart the esp device
                }
              }

              if (String(name).indexOf("btn") >= 0) {
                String text = String(value);
                text.replace(String("+"),String(" "));
                //Serial.print("Putstring: ");
                //Serial.print(name);
                //Serial.print(" : ");
                //Serial.println(text);
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
                  //Serial.print("Putstring: ");
                  //Serial.print(name);
                  //Serial.print(" : ");
                  //Serial.println(text);
                } else {
                  char fixedName[6];
                  fixedName[0] = name[1];
                  fixedName[1] = name[2];
                  fixedName[2] = name[3];
                  fixedName[3] = name[4];
                  fixedName[4] = name[5];
                  fixedName[5] = '\0'; // The terminating NULL
                  preferences.putString(fixedName,text);
                  //Serial.print("Putstring: ");
                  //Serial.print(fixedName);
                  //Serial.print(" : ");
                  //Serial.println(text);
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
                //preferences.putULong("ontime",storedOnTime);
              }
            }

            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\">");
            client.println("</head><body><form action=\"/post\" name=\"form\" id=\"form\" method=\"post\"><div class=\"container\">");
            client.println("<div class='row'><div class='col-sm-12'><h2>Buttons Text</h2></div></div>");
            client.println("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-primary btn-lg' type=\"button\" value=\"Export Setlist To JSON\" onClick=\"saveSongs(); \"></div></div> ");
            for (int i=1;i<=25;i++) {
              client.println("<div class='row'><div class='col-sm-12'><h2>Song " + String(i) + "</h2></div></div>");
              client.println("<div class='form-group row'><div class='input-group col-sm-12 col-md-6'><div class='input-group-prepend'><div class='input-group-text'>Song " + String(i) + "</div></div><input type=\"text\" class='form-control' name=\"song" + String(i) + "\" id=\"song" + String(i) + "\" value=\"" + songs[i-1] + "\"/> <input onclick=\"move(" + String(i) + ",-1)\" class=\"btn btn-secondary\" type=\"button\" value=\"UP\" /> <input onclick=\"move(" + String(i) + ",1)\" class=\"btn btn-secondary\" type=\"button\" value=\"DWN\" />  </div></div>");
               for (int x=1;x<=6;x++) {
                  client.println("<div class='form-group row'><div class='input-group col-sm-12 col-md-6'><div class='input-group-prepend'><div class='input-group-text'>" + String(x) + "</div></div><input type=\"text\" class='form-control' name=\"btn" + getPadded(i) + getPadded(x) + "\" id=\"btn" + getPadded(i) + getPadded(x) + "\" value=\"" + buttonText[i-1][x-1] + "\"/></div></div>");
            
              }
           }
            
           
            client.println("<div class='form-group row'><label class='col-xs-2 col-form-label'>On Time (hours)</label><div class='col-xs-10'><input type=\"text\" class='form-control' name=\"onTime\" value=\"" + String((float)storedOnTime/60/60) + "\"/></div></div>");
            client.println("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-primary btn-lg' type=\"submit\" value=\"Update Settings\"></div></div> ");
            client.println("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-danger btn-lg' type=\"button\" value=\"Clear Storage\" onClick=\"document.getElementById('clearStorage').value='yes';document.getElementById('form').submit();\"></div></div> ");
            client.println("<input type='hidden' id='clearStorage' name='clearStorage' value='no'>");
                            
                            
                          
                            

            client.println("</form>");
            client.println("<br/><br/>Upload Setlist: ");
            client.println("<input id=\"file\" type=\"file\" />");

            //Add Script to export and import setlists
            client.println("<script>");
            client.println("const JSONToFile = (obj, filename) => {const blob = new Blob([JSON.stringify(obj, null, 2)], {type: 'application/json',}); const url = URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = `${filename}.json`; a.click();  URL.revokeObjectURL(url); };");
            client.println(" function saveSongs() { var formData = new FormData(document.querySelector('form')); var object = {}; formData.forEach(function(value, key){ object[key] = value; }); var json = JSON.stringify(object); JSONToFile(json, 'setlist'); }");
            client.println(" var uploadedJSON;");
            client.println(" function onFileSelect(event) { var reader = new FileReader(); reader.onload = onReaderLoad; reader.readAsText(event.target.files[0]); }");
            client.println(" function onReaderLoad(event){ console.log(event.target.result); var x = JSON.parse(event.target.result); uploadedJSON = JSON.parse(x); Object.entries(uploadedJSON).forEach((entry) => { const [key, value] = entry; if (key != \"onTime\") { document.getElementById(key).value = value; }}); }");
            client.println(" function move(num, direction) { const songName = document.getElementById(\"song\" + num).value; const songName2 = document.getElementById(\"song\" + (num + direction)).value; document.getElementById(\"song\" + num).value = songName2; document.getElementById(\"song\" + (num + direction)).value = songName; for (let i = 1; i <= 6; i++) { const n = (\"0\" + num).slice(-2); const n2 = (\"0\" + (num + direction)).slice(-2); const ii = (\"0\" + i).slice(-2); const btn = document.getElementById(\"btn\" + n + ii).value; const btn2 = document.getElementById(\"btn\" + n2 + ii).value;    document.getElementById(\"btn\" + n + ii).value = btn2;    document.getElementById(\"btn\" + n2 + ii).value = btn;  }}");
            client.println(" document.getElementById('file').addEventListener('change', onFileSelect);");
            client.println("</script");                
            
            client.println("</body></html>");
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

  //Serial.println(F("Updating Screens"));
   
    for (i=0;i<=6;++i) {
      SelectScreen(i+1);

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
      
      if (currentSong == 0) {
       //the first song is now always pedalboard mode
        if (lastButton == i) {
        //only refresh this screen if the button has been pushed
          
            display.clearDisplay();
            display.setTextWrap(true );
            display.setCursor(0,0);
            
            if (buttonState[i] == false) {
              //turn background white
              display.fillRect(0, 0, 128, 64,WHITE);
              display.setTextColor(BLACK, WHITE);
              buttonState[i] = true;
            } else {
              //turn background black
              display.fillRect(0, 0, 128, 64,BLACK);
              display.setTextColor(WHITE,BLACK);
              buttonState[i] = false;
            }

            display.setTextSize(textSize);
            display.setCursor(2,2);
            display.print(buttonText[currentSong][i]);
            display.display();
        }
        
      } else {
      //all other songs are in snapshot mode

       display.clearDisplay();
       display.setTextWrap(true );
       display.setCursor(0,0);
         if (lastButton == i) {
          display.fillRect(0, 0, 128, 64,WHITE);
          display.setTextColor(BLACK, WHITE);
         } else {
          display.setTextColor(WHITE,BLACK);
         }

      display.setTextSize(textSize);
      display.setCursor(2,2);
      display.print(buttonText[currentSong][i]);
      display.display();
      
      }
     
      
      
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
