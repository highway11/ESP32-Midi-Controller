## ESP32-Midi-Controller (Multiple OLED Version with Wired HairlessMidi )
Midi Foot Controller with Expression Pedal<br/>
This controller was built to control VST plugins in Reaper.<br/>
It can be built quite cheaply, and uses an ESP32, 10 footswitches, 7 OLED Screens, and 1 expression pedal.<br/>

This branch uses multiple OLED screens using a [TCA9548A multiplexer](https://www.amazon.ca/gp/product/B08DY5VXZ3/)

I also switched from using RTPMidi to using [Hairless Midi](https://projectgus.github.io/hairless-midiserial/) which is MIDI over Usb/Serial

Previous Iterations of this pedalboard
1. <a href='https://github.com/highway11/ESP32-Midi-Controller/'>Single-OLED RTPMidi (wireless) version</a><br/> 
2. <a href='https://github.com/highway11/ESP32-Midi-Controller/tree/MultipleScreens'>Multi-OLED RTPMidi (wireless) version</a><br/> 


Parts List:<br/>
1 x <a href='https://www.amazon.ca/gp/product/B07PP1R8YK/'>38-Pin ESP32</a></br>
10 x <a href='https://www.amazon.ca/gp/product/B077P7NSFJ'>Momentary Footswitches</a></br>
7 x <a href='https://www.amazon.ca/gp/product/B0833PF7ML/'>SSD1306 0.96" OLED Screens</a></br>
1 x <a href='https://www.amazon.ca/gp/product/B08DY5VXZ3/'>TCA9548A Multiplexer</a> (for controlling multiple screens)</br>
1 x <a href='https://www.long-mcquade.com/235511/Keyboards/Keyboard-Accessories/M-Audio/Universal-Expression-Pedal.htm'>M-Audio Expression Pedal</a></br>
 

### Entire Rig
<img src='https://github.com/highway11/ESP32-Midi-Controller/blob/main/EntireRig.jpg?raw=true' width=400 />

## Multi OLED Version
### Schematic
![alt text](https://github.com/highway11/ESP32-Midi-Controller/blob/MultipleScreens-USBSerialMIDI/ESP32MidiControllerMultiScreenSchematic.jpg?raw=true)

### Gut shot (Multi OLED version)
Here's an example of how not to wire it :) .  I apologize for my messy wiring. You can see remnants of previous versions which used a USB Battery bank and switch. I've also stopped using the Female TRS Jack to connect the M-Audio expression pedal and hard wired it. Most of the wires are taken from CAT5 ethernet cabling, as well as some previous wires I had laying around. 
![alt text](https://github.com/highway11/ESP32-Midi-Controller/blob/main/InternalWiringMulitOLED.jpg?raw=true)
### Finished!
![alt text](https://github.com/highway11/ESP32-Midi-Controller/blob/main/FinishedPedalBoard.jpg?raw=true)

### Operation
This controller has an embedded web server. You can connect to it at the IP address you specify in the code, and change the text of the screens for each song in your setlist. Currently it only supports 10 songs, or "banks", but this could be modified. I currently play short set lists that never exceed 10 songs. I have a "Next Song" button that cycles through the songs, as well as switches projects in my Reaper DAW. Each "Song" can have unique text displayed on the screens. Because its simply a MIDI controller you can assign the buttons to anything you like in your DAW. I currently use 6 buttons to switch "snapshots" in Helix Native VST, and 4 buttons to start/stop click and cue tracks </br></br>
![alt text](https://github.com/highway11/ESP32-Midi-Controller/blob/main/webServer.jpg?raw=true)


