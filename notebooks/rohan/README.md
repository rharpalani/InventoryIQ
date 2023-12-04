## 2023-11-07

Today I spent the entire day on development since we've finally received many of the parts required for our user interface subsystem. My goal was to understand how to use the RFID reader module and LCD. My process was as follows:

1. First, I wanted to just test each module on the Arduino Uno. To ensure that it was working correctly and to make the setup/debugging process easier, I initialized both the RFID reader and the LCD using libraries provided by each prospective manufactuer. 
2. Once I understood the basics of each (ie the library to control the module, the correct pin orderings, etc.), then I setup the ESP32 Development Board, plugged these components into the breadboard, and tested on that.
3. Finally, once components were working individually on the dev board, I wrote some code to integrate them together, making up a large part of the overall user interface subsystem. By the end of the day, I was able to scan an RFID tag and have that tag's unique identifier (UID) display on both the LCD and the Arduino console's serial terminal. 

img/nov7.heic