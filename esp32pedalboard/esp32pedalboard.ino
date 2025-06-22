#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <analogWrite.h>
#include <ArduinoOTA.h>

//Use this to format preferences storage if there's an issue
#include <nvs_flash.h>

#include <HttpRequest.h>
#include <Preferences.h>

#include <ardumidi.h> // For sending MIDI

//These are for the OLED Display --------------------------------------
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// --- CONFIGURATION CONSTANTS ---
const int MAX_SONGS = 50;        // Maximum number of songs supported
const int NUM_BUTTON_SCREENS = 6; // Number of dedicated button OLED screens

// --- GLOBAL VARIABLES ---
// WiFi & Web Server
WiFiServer server(8888);
Preferences preferences; // Still used by web interface for saving settings unless modified
HttpRequest httpReq;
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;
String header;

// OLED Display & Song/Button Data
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
String screenText[8] = {"", "", "", "", "", "", "", ""}; // For main screen scrolling text lines
String buttonText[MAX_SONGS][NUM_BUTTON_SCREENS];
boolean buttonState[NUM_BUTTON_SCREENS]; // State for each of the physical buttons (e.g., for pedalboard mode toggles)
String songs[MAX_SONGS];
String connectedMessage = "offline";
int currentSong = 0;
int numSongs = 0; // Actual number of songs populated by SysEx or initialized
int x_scroll, minX_scroll; // For main screen scrolling text
String message_main_display; // Main message shown on screen 0 (song name or other status)
boolean doneConnecting = false; // WiFi connection process state
boolean disconnected = true;    // WiFi connection status (start disconnected)
boolean tunerActive = false;
int lastButtonPressed = -1; // Index of the last physical button pressed (0-5 for main buttons), -1 if none or SysEx change

// SysEx MIDI Processing
#define SYSEX_BUFFER_MAX_LENGTH 128 
byte sysexBuffer[SYSEX_BUFFER_MAX_LENGTH];
int sysexBytesReceived = 0;
bool inSysexMessage = false;

const byte PEDALBOARD_MANUF_ID_FROM_GP = 0x7D;
const byte PEDALBOARD_DEVICE_ID_FROM_GP = 0x01;
const byte CMD_SET_SONG_NAME_FROM_GP = 0x01;
const byte CMD_SET_BUTTON_NAME_FROM_GP = 0x02;
const int MAX_NAME_LENGTH_FROM_SYSEX = 31; // Max chars for a name from SysEx (+null)

// Buttons & LED
#include <Bounce2.h>
Bounce debouncer2, debouncer4, debouncer5, debouncer13, debouncer14, debouncer15,
       debouncer16, debouncer17, debouncer18, debouncer19, debouncer27;

const int redPin = 25; const int greenPin = 32; const int bluePin = 33;
char* originalColor = "red"; // Stores the base color (e.g. based on WiFi network)
const long blinkInterval = 1000;
unsigned long previousMillis_blink = 0;
bool blinkLed = true; // Whether the LED should be blinking (e.g. when disconnected)
bool isLedOn = true;  // Current physical state of the blink

// WiFi Network Configuration
const char* KNOWN_SSID[] = {"LL", "TellMyWifiLover","CoS"};
const char* KNOWN_PASSWORD[] = {"password","password","password"}; // Ensure this matches your actual setup
const IPAddress KNOWN_STATICIP[] = {IPAddress(192,168,137,20), IPAddress(192,168,100,22), IPAddress(192,168,50,20)};
const IPAddress KNOWN_GATEWAY[] = {IPAddress(192,168,137,1), IPAddress(192,168,100,254), IPAddress(192,168,50,1)};
boolean wifiFound = false;
int wifi_scan_idx, wifi_known_idx; // Loop iterators for WiFi scan

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   
IPAddress secondaryDNS(8, 8, 4, 4); 

// Device & Pins
const char* deviceName = "pedalboard";
const int expPin = 34; const int exp2Pin = 35;
// const int batteryVoltagePin = 36; // If battery monitoring is used
int newExpVal = 0; int lastExpVal = 0;
int newExp2Val = 0; int newExp2ValPercent = 0; int lastExp2Val = 0;


// --- FUNCTION PROTOTYPES (Optional, but good practice) ---
void parseAndApplySysex();
void SelectScreen(uint8_t bus);
void processMidiInput();
void setRGBColor(const char* color); // Use const char* for string literals
void displayText(String text);
void updateScreens();
String getPadded(int num);
void initializeArrays();
void recountNumSongs();


// --- SYSEX PARSING ---
void parseAndApplySysex() {
    if (sysexBytesReceived < 4) { /*Serial.println(F("SysEx: HDR_SHORT"));*/ return; }
    if (sysexBuffer[0] != PEDALBOARD_MANUF_ID_FROM_GP || sysexBuffer[1] != PEDALBOARD_DEVICE_ID_FROM_GP) { /*Serial.println(F("SysEx: ID_MISMATCH"));*/ return; }

    byte command = sysexBuffer[2];
    byte songIdx = sysexBuffer[3];

    if (songIdx >= MAX_SONGS) { /*Serial.print(F("SysEx: SONG_IDX_OOB: ")); Serial.println(songIdx);*/ return; }

    char nameBuffer[MAX_NAME_LENGTH_FROM_SYSEX];
    int textStartIndex;
    int payloadTextLength;

    if (command == CMD_SET_SONG_NAME_FROM_GP) {
        textStartIndex = 4;
        if (sysexBytesReceived < textStartIndex) { // No text data present
             songs[songIdx] = ""; 
        } else {
            payloadTextLength = sysexBytesReceived - textStartIndex;
            int charsToCopy = 0;
            for (int k = 0; k < payloadTextLength; k++) {
                char currentChar = (char)sysexBuffer[textStartIndex + k];
                if (currentChar == '\0') break; // Stop at first embedded null from GP's padding
                if (charsToCopy < MAX_NAME_LENGTH_FROM_SYSEX - 1) {
                    nameBuffer[charsToCopy++] = currentChar;
                } else { break; } // ESP32 buffer full
            }
            nameBuffer[charsToCopy] = '\0';
            songs[songIdx] = String(nameBuffer);
        }
        // Serial.print(F("SysEx: Set Song[")); Serial.print(songIdx); Serial.print(F("] to '")); Serial.print(songs[songIdx]); Serial.println(F("'"));
        recountNumSongs(); // Update the effective number of songs

    } else if (command == CMD_SET_BUTTON_NAME_FROM_GP) {
        textStartIndex = 5; 
        if (sysexBytesReceived < textStartIndex) { /*Serial.println(F("SysEx: BTN_HDR_SHORT"));*/ return; } // Needs at least button index
        byte buttonIdx = sysexBuffer[4];
        if (buttonIdx >= NUM_BUTTON_SCREENS) { /*Serial.print(F("SysEx: BTN_IDX_OOB: ")); Serial.println(buttonIdx);*/ return; }

        if (sysexBytesReceived < textStartIndex ) { // No text data for button
            buttonText[songIdx][buttonIdx] = ""; 
        } else {
            payloadTextLength = sysexBytesReceived - textStartIndex;
            int charsToCopy = 0;
            for (int k = 0; k < payloadTextLength; k++) {
                char currentChar = (char)sysexBuffer[textStartIndex + k];
                if (currentChar == '\0') break; 
                if (charsToCopy < MAX_NAME_LENGTH_FROM_SYSEX - 1) {
                    nameBuffer[charsToCopy++] = currentChar;
                } else { break; }
            }
            nameBuffer[charsToCopy] = '\0';
            buttonText[songIdx][buttonIdx] = String(nameBuffer);
        }
        // Serial.print(F("SysEx: Set btn[")); Serial.print(songIdx); Serial.print("]["); Serial.print(buttonIdx); // ...
    } else {
        // Serial.print(F("SysEx: UNK_CMD: 0x")); Serial.println(command, HEX);
        return;
    }

    if (songIdx == currentSong) {
        message_main_display = songs[currentSong]; 
        lastButtonPressed = -1; 
        updateScreens(); 
    }
}

void recountNumSongs() {
    int highestPopulatedSong = -1;
    for (int k = 0; k < MAX_SONGS; k++) {
        // A song is considered "populated" if it's not empty AND it's not the default placeholder "Song X+1"
        // OR if it's song 0 and is "Pedalboard" (even if other songs are default placeholders)
        bool isDefaultPlaceholder = (k > 0 && songs[k] == ("Song " + String(k + 1)));
        bool isDefaultPedalboard = (k == 0 && songs[k] == "Pedalboard");

        if (!songs[k].isEmpty()) {
            if (!isDefaultPlaceholder || isDefaultPedalboard) {
                 highestPopulatedSong = k;
            }
        }
    }
    numSongs = highestPopulatedSong + 1;

    if (numSongs == 0) { // Fallback if all songs somehow became empty or only default placeholders
        if(songs[0].isEmpty() || songs[0] == "Song 1") songs[0] = "Pedalboard";
        numSongs = 1;
    }

    // Ensure currentSong is valid after numSongs might have changed
    if (currentSong >= numSongs && numSongs > 0) {
        currentSong = numSongs - 1;
    } else if (numSongs == 0) { // Should not happen given the fallback above
        currentSong = 0;
    }
}


// --- I2C MULTIPLEXER ---
void SelectScreen(uint8_t bus){
  Wire.beginTransmission(0x70);  
  Wire.write(1 << bus);          
  Wire.endTransmission();
}

// --- WIFI EVENT HANDLERS ---
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){ /* Serial.println("WiFi AP Connected"); */ }
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  doneConnecting = true;
  disconnected = false;
  message_main_display = String("IP: ") + String(WiFi.localIP().toString());
  connectedMessage = WiFi.localIP().toString() + String(":8888");
  // Serial.print("WiFi Got IP: "); Serial.println(WiFi.localIP());
}
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  setRGBColor("red"); // Solid red on disconnect
  blinkLed = true;    // Start blinking red
  originalColor = "red";
  message_main_display = "DISCONNECTED WIFI!";
  disconnected = true;
  connectedMessage = "offline";
  // Serial.println("WiFi Disconnected");
}

// --- INITIALIZATION ---
void initializeArrays() {
    numSongs = MAX_SONGS; // Assume all slots are available for placeholders
    for (int k = 0; k < MAX_SONGS; k++) {
        if (k == 0) {
            songs[k] = "Pedalboard"; 
        } else {
            songs[k] = "Song " + String(k + 1); 
        }
        for (int j = 0; j < NUM_BUTTON_SCREENS; j++) {
            buttonText[k][j] = "Btn " + String(j + 1); 
            if (k==0) buttonState[j] = true; // Default button states for pedalboard mode
        }
    }
    message_main_display = songs[currentSong]; 
    recountNumSongs(); // Recalculate actual numSongs based on non-placeholder content (will be 1 initially)
}

void setup() {
   Serial.begin(115200);
   Serial.println(F("\nBooting Pedalboard Controller..."));
  
  Wire.begin(); // For I2C
  for (int k=0; k < (1 + NUM_BUTTON_SCREENS); ++k) { // 1 main screen + NUM_BUTTON_SCREENS
     SelectScreen(k);
     if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.print(F("SSD1306 allocation failed for screen ")); Serial.println(k);
      for(;;); // Halt
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Explicit colors
    if (k == 0) { display.setTextWrap(false); } 
    else { display.setTextWrap(true); }
    display.display(); // Show cleared screen
  }
  x_scroll = display.width(); // Initialize scroll position
  minX_scroll = 0;
 
  initializeArrays(); // Set default song/button names and numSongs
 
  preferences.begin("pedalboard", false); // For web server settings if any are saved

  updateScreens(); // Initial display update for button screens
  
  // Debouncer setup
  debouncer2.attach(2, INPUT_PULLUP); debouncer2.interval(5);
  debouncer4.attach(4, INPUT_PULLUP); debouncer4.interval(5);
  debouncer5.attach(5, INPUT_PULLUP); debouncer5.interval(5);
  debouncer13.attach(13, INPUT_PULLUP); debouncer13.interval(5);
  debouncer14.attach(14, INPUT_PULLUP); debouncer14.interval(5);
  debouncer15.attach(15, INPUT_PULLUP); debouncer15.interval(5);
  debouncer16.attach(16, INPUT_PULLUP); debouncer16.interval(5);
  debouncer17.attach(17, INPUT_PULLUP); debouncer17.interval(5);
  debouncer18.attach(18, INPUT_PULLUP); debouncer18.interval(5);
  debouncer19.attach(19, INPUT_PULLUP); debouncer19.interval(5);
  debouncer27.attach(27, INPUT_PULLUP); debouncer27.interval(5);

  analogWrite(redPin, 0); analogWrite(greenPin, 0); analogWrite(bluePin, 0);
  originalColor = "red"; // Default before WiFi connection
  setRGBColor(originalColor); // Set initial LED color
  blinkLed = true; // Blink red initially

  // WiFi Connection
  displayText("WiFi Scan..."); // Show on main OLED
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);

  int nbVisibleNetworks = WiFi.scanNetworks();
  if (nbVisibleNetworks == 0) {
    displayText("No WiFi APs found");
  } else {
    displayText(String(nbVisibleNetworks) + " APs found");
    for (wifi_scan_idx = 0; wifi_scan_idx < nbVisibleNetworks; ++wifi_scan_idx) {
      for (wifi_known_idx = 0; wifi_known_idx < (sizeof(KNOWN_SSID) / sizeof(KNOWN_SSID[0])); wifi_known_idx++) {
        if (strcmp(KNOWN_SSID[wifi_known_idx], WiFi.SSID(wifi_scan_idx).c_str()) == 0) { // strcmp returns 0 on match
          wifiFound = true;
          break; 
        }
      }
      if (wifiFound) break;
    }
  }

  if (wifiFound) {
    displayText("Found: " + String(KNOWN_SSID[wifi_known_idx]));
    if (!WiFi.config(KNOWN_STATICIP[wifi_known_idx], KNOWN_GATEWAY[wifi_known_idx], subnet, primaryDNS, secondaryDNS)) {
      displayText("STA Config Failed");
    }
    WiFi.begin(KNOWN_SSID[wifi_known_idx], KNOWN_PASSWORD[wifi_known_idx]);
    
    // Wait for connection, with timeout
    unsigned long wifiConnectStartTime = millis();
    bool connectedWithinTimeout = false;
    while (millis() - wifiConnectStartTime < 20000) { // 20 second timeout for WiFi
        if (WiFi.status() == WL_CONNECTED) {
            connectedWithinTimeout = true;
            break;
        }
        displayText("Connecting " + String(KNOWN_SSID[wifi_known_idx]));
        delay(500);
    }

    if (connectedWithinTimeout) {
      // WiFiGotIP callback will set doneConnecting, disconnected, messages, etc.
      // Set LED color based on connected network
      displayText("WiFi Connected!"); // WiFiGotIP will show IP
      blinkLed = false; // Stop blinking once connected
      if (strcmp(KNOWN_SSID[wifi_known_idx],"LL") == 0) { originalColor = "blue"; }
      else if (strcmp(KNOWN_SSID[wifi_known_idx],"TellMyWifiLover") == 0) { originalColor = "green"; }
      else if (strcmp(KNOWN_SSID[wifi_known_idx],"CoS") == 0) { originalColor = "yellow"; } // Define yellow in setRGBColor
      else { originalColor = "green"; } // Default connected color
      setRGBColor(originalColor);
      
      server.begin();
      ArduinoOTA.setHostname(deviceName);
      ArduinoOTA.onStart([](){ Serial.println("OTA Start");}).onEnd([](){ Serial.println("\nOTA End");}).onProgress([](unsigned int p, unsigned int t){ Serial.printf("OTA Progress: %u%%\r", (p/(t/100)));}).onError([](ota_error_t e){ Serial.printf("OTA Error[%u]\n", e);});
      ArduinoOTA.begin();
    } else {
      displayText("WiFi Conn. Timeout");
      originalColor = "red"; setRGBColor(originalColor); blinkLed = true;
      doneConnecting = true; // Allow loop to run but disconnected
      disconnected = true;
      message_main_display = "WiFi Fail"; connectedMessage = "offline";
    }
  } else {
    displayText("No Known WiFi Net");
    originalColor = "red"; setRGBColor(originalColor); blinkLed = true;
    doneConnecting = true; disconnected = true;
    message_main_display = "Offline"; connectedMessage = "offline";
  }
}


// --- MIDI PROCESSING ---
void processMidiInput() {
    static byte incomingByte;
    static byte statusByte = 0; 
    static byte dataByte1 = 0;  
    static int regularMessageState = 0; // 0: waiting for status, 1: data1, 2: data2

    while (Serial.available() > 0) {
        incomingByte = Serial.read();

        if (inSysexMessage) {
            if (incomingByte == 0xF7) { 
                // Serial.print(F("SysEx End. Bytes: ")); Serial.println(sysexBytesReceived);
                parseAndApplySysex(); 
                inSysexMessage = false;
                sysexBytesReceived = 0; 
            } else if (sysexBytesReceived < SYSEX_BUFFER_MAX_LENGTH) {
                if (incomingByte < 0x80) { 
                    sysexBuffer[sysexBytesReceived++] = incomingByte;
                } else {
                    // Serial.print(F("SysEx Abort: Invalid data 0x")); Serial.println(incomingByte, HEX);
                    inSysexMessage = false;
                    sysexBytesReceived = 0;
                    // Let this new status byte fall through if it is one
                }
            } else { // Buffer overflow
                // Serial.println(F("SysEx Buffer Overflow."));
                inSysexMessage = false; 
                sysexBytesReceived = 0; 
            }
            // If SysEx just ended or aborted, and current byte is NOT a new status, consume & loop
            if (!inSysexMessage && incomingByte < 0x80) { 
                continue; 
            }
        }
        
        // This 'if' must be separate for fall-through from SysEx abortion by a status byte.
        if (!inSysexMessage) { 
            if (incomingByte == 0xF0) { // SysEx Start
                inSysexMessage = true;
                sysexBytesReceived = 0; 
                statusByte = 0; // Clear regular MIDI state         
                regularMessageState = 0;
                // Serial.println(F("SysEx Start."));
            } else if (incomingByte >= 0xF8) { // Real-time messages (Clock, Start, Stop, ActiveSensing)
                continue; // Ignore for now
            } else if (incomingByte >= 0xF1 && incomingByte <= 0xF6 ) { // Other System Common (MTC, Song Sel, Tune Req)
                statusByte = 0; regularMessageState = 0; // Reset state
                continue; // Ignore for now
            } else if (incomingByte >= 0x80) { // Regular MIDI Status byte
                statusByte = incomingByte;
                regularMessageState = 1; 
            } else if (statusByte != 0) { // Data byte for a regular MIDI message
                if (regularMessageState == 1) { 
                    dataByte1 = incomingByte;
                    byte commandType = statusByte & 0xF0;
                    // Check for 2-byte messages (Program Change, Channel Pressure)
                    if (commandType == 0xC0 || commandType == 0xD0) { 
                        // Process 2-byte message here if needed
                        // Example: if(commandType == 0xC0 && (statusByte & 0x0F) == MY_TARGET_CHANNEL) { handleProgramChange(dataByte1); }
                        regularMessageState = 1; // Ready for next dataByte1 (running status) or new status byte
                    } else { // Expect dataByte2 for 3-byte messages
                        regularMessageState = 2; 
                    }
                } else if (regularMessageState == 2) { // This is dataByte2 for a 3-byte message
                    byte command = statusByte & 0xF0;
                    byte midiChannel = statusByte & 0x0F; // 0-15

                    if (command == 0x90 && midiChannel == 2 && incomingByte > 0) { // Note On, MIDI Channel 3 (index 2), velocity > 0
                        int newSongCandidate = dataByte1; // Note number
                        
                        if (newSongCandidate >= 0 && newSongCandidate < MAX_SONGS) { 
                            currentSong = newSongCandidate; 
                            // Serial.print(F("MIDI NoteOn: Set currentSong to ")); Serial.println(currentSong);
                            
                            if (currentSong < numSongs && !songs[currentSong].isEmpty()) { // Check against dynamically known numSongs
                                message_main_display = songs[currentSong];
                            } else { // Fallback if name not yet received or out of current numSongs range (but within MAX_SONGS)
                                message_main_display = "Song " + String(currentSong + 1); 
                            }
                            lastButtonPressed = -1; // This change was via MIDI, not a physical button
                            updateScreens(); 
                            // Optional visual feedback for MIDI song change:
                            // setRGBColor("white"); delay(50); setRGBColor(originalColor);
                        } else {
                            // Serial.print(F("MIDI NoteOn: songIdx OOB: ")); Serial.println(newSongCandidate);
                        }
                    }
                    // Add other 3-byte message handling here (Note Off 0x80, CC 0xB0, PitchBend 0xE0, PolyPressure 0xA0) if needed.
                    
                    regularMessageState = 1; // Ready for next dataByte1 (running status) or new status byte
                }
            }
            // If incomingByte < 0x80 and statusByte is 0, it's an orphaned data byte, ignore.
        } 
    } 
}


// --- MAIN LOOP ---
void loop() {
  if (WiFi.status() == WL_CONNECTED && !disconnected) {
    ArduinoOTA.handle();
  }

  processMidiInput();
   
  currentTime = millis(); // Update current time for blink logic
  if ((currentTime - previousMillis_blink >= blinkInterval) && blinkLed) {
      previousMillis_blink = currentTime;
      isLedOn = !isLedOn;
      setRGBColor(isLedOn ? originalColor : "off");
  }
   
  debouncer2.update(); debouncer4.update(); debouncer5.update();
  debouncer13.update(); debouncer14.update(); debouncer15.update();
  debouncer16.update(); debouncer17.update(); debouncer18.update();
  debouncer19.update(); debouncer27.update();
  
  byte velocity = 127; // Use full velocity for footswitch presses usually
  byte channel = 1;   // MIDI channel for sending (0-15 for ardumidi library, so 0 for CH1)

  // Define MIDI notes for different modes/buttons
  // Pedalboard Mode (currentSong == 0)
  byte pb_note_btn1 = 41, pb_note_btn2 = 42, pb_note_btn3 = 43;
  byte pb_note_btn4 = 44, pb_note_btn5 = 45, pb_note_btn6 = 46;
  byte pb_note_other1 = 48, pb_note_other2 = 49; // For buttons 17, 16

  // Song Mode (currentSong > 0) - these are likely command/CC numbers or different notes
  byte song_note_btn1 = 21, song_note_btn2 = 22, song_note_btn3 = 23;
  byte song_note_btn4 = 24, song_note_btn5 = 25, song_note_btn6 = 26;
  byte song_note_other1 = 29, song_note_other2 = 30; // For buttons 17, 16

  // Command notes (sent TO Gig Performer)
  byte cmd_note_next_song_to_gp = 27; // Example: What your pedalboard sends for "Next Song"
  byte cmd_note_prev_song_to_gp = 31; // Example: What your pedalboard sends for "Previous Song"
  byte cmd_note_tuner_to_gp = 28;     // Example: For tuner toggle

  // --- Footswitch Logic ---
  // Button 1 (Pin 2, debouncer2)
  if (debouncer2.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn1 : song_note_btn1), velocity);
    setRGBColor("white"); lastButtonPressed = 0; updateScreens();
  } else if (debouncer2.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn1 : song_note_btn1), 0);
    setRGBColor(originalColor);
  }
  // Button 2 (Pin 5, debouncer5)
  if (debouncer5.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn2 : song_note_btn2), velocity);
    setRGBColor("white"); lastButtonPressed = 1; updateScreens();
  } else if (debouncer5.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn2 : song_note_btn2), 0);
    setRGBColor(originalColor);
  }
  // Button 3 (Pin 4, debouncer4)
  if (debouncer4.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn3 : song_note_btn3), velocity);
    setRGBColor("white"); lastButtonPressed = 2; updateScreens();
  } else if (debouncer4.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn3 : song_note_btn3), 0);
    setRGBColor(originalColor);
  }
  // Button 4 (Pin 14, debouncer14)
  if (debouncer14.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn4 : song_note_btn4), velocity);
    setRGBColor("white"); lastButtonPressed = 3; updateScreens();
  } else if (debouncer14.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn4 : song_note_btn4), 0);
    setRGBColor(originalColor);
  }
  // Button 5 (Pin 13, debouncer13)
  if (debouncer13.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn5 : song_note_btn5), velocity);
    setRGBColor("white"); lastButtonPressed = 4; updateScreens();
  } else if (debouncer13.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn5 : song_note_btn5), 0);
    setRGBColor(originalColor);
  }
  // Button 6 (Pin 15, debouncer15)
  if (debouncer15.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_btn6 : song_note_btn6), velocity);
    setRGBColor("white"); lastButtonPressed = 5; updateScreens();
  } else if (debouncer15.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_btn6 : song_note_btn6), 0);
    setRGBColor(originalColor);
  }

  // --- Navigation Buttons ---
  // Button 10 NEXT SONG (debouncer19) - Sends command TO GP
  if (debouncer19.fell()) {
    midi_note_on(channel, cmd_note_next_song_to_gp, velocity); 
    setRGBColor("white");
    // DO NOT change currentSong here. Let GP send MIDI Note on Ch3 to change song.
    lastButtonPressed = -1; // Or map to a display if this button has one (e.g. 9 for old mapping)
  } else if (debouncer19.rose()) {
    midi_note_off(channel, cmd_note_next_song_to_gp, 0);
    setRGBColor(originalColor);
  }
  // Button 11 PREVIOUS SONG (debouncer27) - Sends command TO GP
  if (debouncer27.fell()) {
    midi_note_on(channel, cmd_note_prev_song_to_gp, velocity); 
    setRGBColor("white");
    lastButtonPressed = -1; // Or map to a display (e.g. 10 for old mapping)
  } else if (debouncer27.rose()) {
    midi_note_off(channel, cmd_note_prev_song_to_gp, 0);
    setRGBColor(originalColor);
  }

  // Button 9 TUNER (debouncer18)
  if (debouncer18.fell()) {
    midi_note_on(channel, cmd_note_tuner_to_gp, velocity); // Send tuner command TO GP
    // Visual feedback for tuner is handled based on tunerActive state change
    tunerActive = !tunerActive;
    if (tunerActive) {
      message_main_display = "TUNER";
      // Potentially change LED color for tuner mode
    } else {
      message_main_display = songs[currentSong];
      // Restore original LED color
    }
    lastButtonPressed = -1; // Or map to a display (e.g. 8 for old mapping)
    // updateScreens(); // Only if tuner state affects button screens directly
  } else if (debouncer18.rose()) {
    midi_note_off(channel, cmd_note_tuner_to_gp, 0);
    // setRGBColor(originalColor); // Restore color if tuner changed it and is now off
  }

  // Button for Pin 17 (debouncer17) - Assuming it's a general purpose button
  if (debouncer17.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_other1 : song_note_other1), velocity);
    setRGBColor("white"); 
    // lastButtonPressed = index_for_this_button_display_if_any; updateScreens();
  } else if (debouncer17.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_other1 : song_note_other1), 0);
    setRGBColor(originalColor);
  }
  // Button for Pin 16 (debouncer16) - Assuming it's a general purpose button
  if (debouncer16.fell()) {
    midi_note_on(channel, (currentSong == 0 ? pb_note_other2 : song_note_other2), velocity);
    setRGBColor("white");
    // lastButtonPressed = index_for_this_button_display_if_any; updateScreens();
  } else if (debouncer16.rose()) {
    midi_note_off(channel, (currentSong == 0 ? pb_note_other2 : song_note_other2), 0);
    setRGBColor(originalColor);
  }

  // Expression Pedals
  newExpVal = map(analogRead(expPin), 0, 4095, 0, 127);
  if (newExpVal != lastExpVal) {
      midi_controller_change(channel, 16, newExpVal); // CC16
      lastExpVal = newExpVal;
  }
  int rawExp2 = analogRead(exp2Pin); // Read once
  newExp2Val = map(rawExp2, 0, 4095, 0, 127);
  if (newExp2Val != lastExp2Val) {
      midi_controller_change(channel, 17, newExp2Val); // CC17
      lastExp2Val = newExp2Val;
      // Update display if EXP2 has a dedicated visual element or changes main message
      // newExp2ValPercent = map(rawExp2, 0, 4095, 0, 100);
      // if (!tunerActive) message_main_display = String(newExp2ValPercent) + "%"; 
  }
  
  delay(5); // Reduced main loop delay to improve responsiveness

  // Main OLED Display Update (Screen 0)
  if (doneConnecting || disconnected) { 
    SelectScreen(0);
    display.clearDisplay();
    display.setTextWrap(false);
    display.setCursor(0,7); display.setTextSize(2);
    if (currentSong >= 0 && currentSong < MAX_SONGS) {
         display.print(songs[currentSong]);
    } else { // Should not happen with proper currentSong management
         display.print(F("Err:SNG OOB")); 
    }
    display.setTextSize(3); 
    // Determine if main message should scroll or be static
    int textPixelWidth = message_main_display.length() * 18; // Approx 6px/char_width * textSize 3
    if (textPixelWidth > SCREEN_WIDTH - 20) { // If text is wider than screen (minus some padding)
        display.setCursor(x_scroll, 28);
        x_scroll = x_scroll - 4; // Scroll speed
        if (x_scroll < -textPixelWidth) x_scroll = display.width();
    } else { // Static text
        display.setCursor((SCREEN_WIDTH - textPixelWidth) / 2, 28); // Center it
        x_scroll = display.width(); // Reset scroll for next time if text becomes long
    }
    display.print(message_main_display);   
    
    display.setTextSize(1); display.setCursor(0,56);
    display.print(connectedMessage); 
    display.fillRect(0, SCREEN_HEIGHT - 9, map(newExpVal,0,127,0,SCREEN_WIDTH), 8, SSD1306_WHITE); 
    display.display();
  }
   
  // --- Web Server ---
  WiFiClient client = server.available();
  if (client) {
    currentTime = millis(); previousTime = currentTime; header = "";
    while (client.connected() && currentTime - previousTime <= timeoutTime) { 
      currentTime = millis();
      if (client.available()) {             
        char c = client.read(); httpReq.parseRequest(c); header += c; // Build header for debugging if needed
        if (httpReq.endOfRequest()) {
          client.println(F("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n"));
          
          bool webDataChanged = httpReq.paramCount > 0;

          for(int k_param=0; k_param < httpReq.paramCount; k_param++){
            char name_buf[32], value_buf[128]; 
            httpReq.getParam(k_param, name_buf, value_buf); 
            String paramName = String(name_buf);
            String paramValue = String(value_buf);
            paramValue.replace("+"," "); 

            if (paramName.equals("clearStorage") && paramValue.equals("yes")) { // Use .equals for Strings
                nvs_flash_erase(); nvs_flash_init(); ESP.restart();
            } else if (paramName.startsWith("btn")) {
                // preferences.putString(paramName.c_str(), paramValue); // Optional: persist web changes
                int p_songNum = paramName.substring(3,5).toInt();
                int p_btnNum = paramName.substring(5,7).toInt();
                if (p_songNum > 0 && p_songNum <= MAX_SONGS && p_btnNum > 0 && p_btnNum <= NUM_BUTTON_SCREENS) {
                    buttonText[p_songNum-1][p_btnNum-1] = paramValue;
                }
            } else if (paramName.startsWith("song")) { 
                // preferences.putString(paramName.c_str(), paramValue); // Optional: persist web changes
                int p_songNum = paramName.substring(4).toInt(); // Assumes "song" + Number (e.g. song1, song10)
                if (p_songNum > 0 && p_songNum <= MAX_SONGS) {
                    songs[p_songNum-1] = paramValue;
                }
            }
          }
          
          if(webDataChanged) { // If web form submitted data, recount numSongs
            recountNumSongs();
            if (currentSong == 0 && !songs[0].isEmpty()) message_main_display = songs[0]; // Update main display if pedalboard mode name changed
          }
            
          // --- HTML Page Generation ---
          client.println(F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='icon' href='data:,'><link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css'></head><body><form action='/' name='form' id='form' method='post'><div class='container'>"));
          client.print(F("<div class='row'><div class='col-sm-12'><h2>Pedalboard Config (Max "));
          client.print(MAX_SONGS); client.println(F(" Songs)</h2></div></div>"));
          client.println(F("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-info btn-sm' type='button' value='Export JSON' onClick='saveSongs();'></div></div>"));
          
          for (int k_html = 1; k_html <= MAX_SONGS; k_html++) {
            client.print(F("<div class='row mt-2'><div class='col-sm-12'><h5>Song ")); client.print(k_html); client.println(F("</h5></div></div>"));
            client.print(F("<div class='form-group row'><div class='input-group col-sm-12 col-md-8'><div class='input-group-prepend'><div class='input-group-text' style='min-width:60px;'>Name</div></div><input type='text' class='form-control' name='song"));
            client.print(k_html); client.print(F("' id='song")); client.print(k_html); client.print(F("' value='"));
            client.print(songs[k_html-1]);
            client.print(F("'/> <input onclick='move(")); client.print(k_html); client.print(F(",-1)' class='btn btn-secondary btn-sm' type='button' value='↑' title='Move Up'/> <input onclick='move("));
            client.print(k_html); client.print(F(",1)' class='btn btn-secondary btn-sm' type='button' value='↓' title='Move Down'/></div></div>"));
            
            for (int x_html = 1; x_html <= NUM_BUTTON_SCREENS; x_html++) {
                 client.print(F("<div class='form-group row'><div class='input-group col-sm-12 col-md-7 offset-md-1'><div class='input-group-prepend'><div class='input-group-text' style='min-width:60px;'>Btn "));
                 client.print(x_html); client.print(F("</div></div><input type='text' class='form-control' name='btn"));
                 client.print(getPadded(k_html)); client.print(getPadded(x_html)); client.print(F("' id='btn"));
                 client.print(getPadded(k_html)); client.print(getPadded(x_html)); client.print(F("' value='"));
                 client.print(buttonText[k_html-1][x_html-1]); client.println(F("'/></div></div>"));
            }
          }
          client.println(F("<div class='form-group row mt-3'><div class='col-sm-10'><input class='btn btn-primary btn-lg' type='submit' value='Update Settings'></div></div>"));
          client.println(F("<div class='form-group row'><div class='col-sm-10'><input class='btn btn-danger btn-lg' type='button' value='Clear NVS Storage' onClick=\"if(confirm('Sure? This will erase NVS & restart.')){document.getElementById('clearStorage').value='yes';document.getElementById('form').submit();}\"></div></div>"));
          client.println(F("<input type='hidden' id='clearStorage' name='clearStorage' value='no'>"));
          client.println(F("</form><hr>Upload Setlist JSON: <input id='file' type='file' class='form-control-file mb-3'/>"));
          // JavaScript
          client.println(F("<script>const JSONToFile=(o,f)=>{const b=new Blob([JSON.stringify(o,null,2)],{type:'application/json'});const u=URL.createObjectURL(b);const a=document.createElement('a');a.href=u;a.download=`${f}.json`;a.click();URL.revokeObjectURL(u)};function saveSongs(){var d=new FormData(document.querySelector('form'));var o={};d.forEach((v,k)=>{o[k]=v});JSONToFile(o,'pedalboard-setlist')}var ulJSON;function onFileSel(e){var r=new FileReader();r.onload=onReaderLoad;r.readAsText(e.target.files[0])}function onReaderLoad(e){try{var j=JSON.parse(e.target.result);ulJSON=JSON.parse(j);Object.entries(ulJSON).forEach(([k,v])=>{const el=document.getElementById(k);if(el&&k!='clearStorage'&&k!='onTime')el.value=v})}catch(err){alert('JSON Parse Error')}}"));
          client.print(F("function move(n,d){const t=n+d;if(t<1||t>")); client.print(MAX_SONGS);
          client.println(F(")return;const sN=document.getElementById('song'+n),sT=document.getElementById('song'+t);if(!sN||!sT)return;const vN=sN.value,vT=sT.value;sN.value=vT;sT.value=vN;for(let i=1;i<="));
          client.print(NUM_BUTTON_SCREENS); client.println(F(";i++){const pN=getPad(n),pT=getPad(t),pI=getPad(i);const bN=document.getElementById('btn'+pN+pI),bT=document.getElementById('btn'+pT+pI);if(!bN||!bT)continue;const vBN=bN.value,vBT=bT.value;bN.value=vBT;bT.value=vBN}}function getPad(x){return('00'+x).slice(-2)}document.getElementById('file').addEventListener('change',onFileSel);</script>"));
          client.println(F("</div></body></html>"));

          httpReq.resetRequest();
          updateScreens(); 
          // minX_scroll might need recalculation if message_main_display changed via web
          break; 
        }
      }
    }
    client.stop();
  }
}

// --- RGB LED CONTROL ---
void setRGBColor(const char* color) { // Parameter type const char*
  if (strcmp(color, "red") == 0) { analogWrite(redPin, 20); analogWrite(greenPin, 0); analogWrite(bluePin, 0); }
  else if (strcmp(color, "blue") == 0) { analogWrite(redPin, 0); analogWrite(greenPin, 0); analogWrite(bluePin, 10); }
  else if (strcmp(color, "green") == 0) { analogWrite(redPin, 0); analogWrite(greenPin, 10); analogWrite(bluePin, 0); }
  else if (strcmp(color, "white") == 0) { analogWrite(redPin, 10); analogWrite(greenPin, 10); analogWrite(bluePin, 10); }
  else if (strcmp(color, "yellow") == 0) { analogWrite(redPin, 20); analogWrite(greenPin, 15); analogWrite(bluePin, 0); } // Adjusted yellow
  else if (strcmp(color, "off") == 0) { analogWrite(redPin, 0); analogWrite(greenPin, 0); analogWrite(bluePin, 0); }
}

// --- MAIN SCREEN DEBUG TEXT ---
void displayText(String text) { // For initial boot/WiFi messages on main screen
  SelectScreen(0);
  // display.clearDisplay(); // Avoid clearing if called rapidly
  for (int k = 0; k < 7; ++k) screenText[k] = screenText[k+1];
  screenText[7] = text.substring(0, 21); // Truncate for display line (128px / ~6px_char_width = ~21)
  
  display.clearDisplay(); // Clear once before drawing all lines
  for (int k = 0; k <= 7; ++k) { 
    display.setCursor(0, (k*8)); 
    display.println(screenText[k]); 
  }
  display.display();
}

// --- BUTTON SCREEN UPDATES ---
void updateScreens() {
    // This function updates the NUM_BUTTON_SCREENS (e.g., 6) button OLEDs.
    // Main screen (Screen 0) is updated in loop() for scrolling message_main_display.
    // Serial.print(F("Updating button screens for currentSong: ")); Serial.println(currentSong);

    for (int k=0; k < NUM_BUTTON_SCREENS; ++k) { 
        SelectScreen(k+1); // Selects physical screen 1 through NUM_BUTTON_SCREENS

        if (currentSong < 0 || currentSong >= MAX_SONGS ) { 
            display.clearDisplay(); display.setTextSize(2); display.setCursor(2,2);
            display.print(F("SONG ERR")); display.display();
            continue; 
        }

        String txt_to_display = buttonText[currentSong][k];
        int txt_size = 4; 
        if (txt_to_display.isEmpty()) { txt_size = 2; txt_to_display = F("-"); } 
        else if (txt_to_display.length() < 5) txt_size = 5;
        else if (txt_to_display.length() <= 14) txt_size = 3; // Max ~4 chars wide at size 3 (12px), ~2 lines
        else if (txt_to_display.length() > 14) txt_size = 2; // Max ~6 chars wide at size 2 (24px), ~4 lines (but text wrap helps)

        display.clearDisplay(); display.setTextWrap(true); 
        // Try to center text vertically a bit better, crude adjustment
        int yPos = 2;
        if (txt_size == 5) yPos = (SCREEN_HEIGHT - (5*8))/2 -4; // 5*8 is approx height
        else if (txt_size == 4) yPos = (SCREEN_HEIGHT - (4*8))/2 -2;
        else if (txt_size == 3) yPos = (SCREEN_HEIGHT - (3*8))/2; // Assuming roughly 2 lines
        else if (txt_size == 2) yPos = 2; // More lines, start near top
        if (yPos < 2) yPos = 2;
        display.setCursor(2,yPos); 

        bool highlight_button = false;
        if (currentSong == 0) { // Pedalboard Mode
            if (lastButtonPressed == k) { // Only toggle if this button (k) was the one pressed
                 buttonState[k] = !buttonState[k];
            }
            highlight_button = buttonState[k]; // Highlight based on toggled state
        } else { // Song/Snapshot Mode
            // Highlight only if this physical button (k) was the last one pressed to activate something in this song.
            // SysEx changes (lastButtonPressed = -1) won't cause highlights here.
            highlight_button = (lastButtonPressed == k); 
        }

        if (highlight_button) { 
            display.fillRect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,SSD1306_WHITE); 
            display.setTextColor(SSD1306_BLACK,SSD1306_WHITE); 
        } else { 
            display.setTextColor(SSD1306_WHITE,SSD1306_BLACK); 
        }
        display.setTextSize(txt_size); 
        display.print(txt_to_display);
        display.display();
    }
    // If a physical button press (0-5) caused this update,
    // reset lastButtonPressed AFTER screens are drawn so it doesn't persist highlight
    // unless pressed again. MIDI/SysEx changes should set lastButtonPressed to -1.
    if (lastButtonPressed >= 0 && lastButtonPressed < NUM_BUTTON_SCREENS) {
        lastButtonPressed = -1; 
    }
}

// --- UTILITY: PADDED STRING ---
String getPadded(int num) { // For numbers up to 99.
  char buff[3]; // "99\0"
  sprintf(buff, "%02d", num); 
  return String(buff);
}
