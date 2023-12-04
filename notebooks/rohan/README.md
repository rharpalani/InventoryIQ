# Rohan Worklog & Journal

## 2023-11-07

Today I spent the entire day on development since we've finally received many of the parts required for our user interface subsystem. My goal was to understand how to use the RFID reader module and LCD. To effectively learn how each of these parts worked before integrating them into the system, I first tested each module on my Arduino Uno using the manufacturer-provided test files and libraries. Once each of these was working individually, my objective for the day was to be able to read an RFID's unique ID (UID) using the Arduino and then echo it to the LCD. 

First, I found this [tutorial](https://learn.sunfounder.com/lesson-1-display-by-i2c-lcd1602/) on the SunFounder website to learn how to display text to the LCD. Similarly, I used this [guide](https://randomnerdtutorials.com/security-access-using-mfrc522-rfid-reader-with-arduino/) to setup & wire the RFID reader using my Arduino. Once those were setup, I was able to use the following code to get the RFID to print its UID to the serial monitor.

![nov72](img/nov72.png)

Our project initially proposed using iCards as the RFID tags to allow authentication into the system. However, I unfortuntely realized that the RFID reader we had ordered is not compatible with iCards, as only the provided test RFID tags were being read. I ended up looking into [types of RFID](https://www.atlasrfidstore.com/rfid-insider/low-frequency-rfid-vs-high-frequency-rfid/) and realized that there are two primary forms, high and low frequency readers and tags. The high frequency reader that we had ordered is more common in amateur or hobbyist settings, while low frequency ones are used in professional settings, like in iCards. However, buying a low frequency reader wouldn't be enough to solve this issue either - after some further investigation from this [forum](https://www.reddit.com/r/accesscontrol/comments/xr02mi/hid_seos_cards_returning_random_values_from_a/), I found that the iCard manufactuer, HID, restricts its cards to only be read by its HID brand. Even if I had a high frequency RFID reader, unless it was an HID one, the UID read from each iCard would be different each time as part of the company's encryption process. HID readers were all over $150, so I decided to talk to Sanjana and the course staff about the issue. 

By the end of the day, on the Arduino, I was able to get software running to extract the UID from an RFID tag and display that UID on the LCD, a significant component of our user interface subsystem. 

![nov7](img/nov7.jpg)

## 2023-11-08

Today my objective was to reproduce this system on the ESP32 Dev Board. Though this should be easy enough since all of the fundamental software is the same, I still had to read through the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/linux-macos-setup.html#get-started-linux-macos-first-steps) to understand how to flash the ESP32 with this code. Using this, I learned how to use the idf.py command line tool to flash a simple LED blinking program to the dev module, as shown here. 

![nov82](img/nov82.png)

However, I initially had an issue flashing my Arduino code onto the dev module. I kept running into an issue where the dev module would say that it wasn't in download mode, and thus nothing could be flashed to it. I couldn't understand why this was the case, since flahsing the Espressif example programs had worked fine. After some searching, I found this [forum](http://community.heltec.cn/t/solved-wsl-v3-a-fatal-error-occurred-failed-to-connect-to-esp32-s3-wrong-boot-mode-detected-0x14/11962/6) which explained that if GPIO46 was set to a value, then the ESP would have undefined behavior. After finding the below diagram in the ESP32 datasheet, I moved one of my wires out of GPIO46, and then the module started working.

![nov83](img/nov83.png)

I realized that this was also why there is a DNP spot on the PCB schematic for GPIO46. Now the ESP dev module could run the same program as the Arduino.

## 2023-11-13

Today we received the relay module, so we tested it to ensure that we could use it in tandem with the solenoid lock and box. First, I tried to wire this so that the power input passes through the relay from the wall to the solenoid. However, the relay wasn't working as expected here. After some [research](https://www.circuitbasics.com/what-is-a-relay/) into the physics of relays, I realized that I had a conceptual misunderstanding into how relays worked. Rather than the power being passed through the relays, relays essentially have two different circuits, which are closed when activated by an electromagnet, allowing power to completely pass through. Using the following diagram, I was able to connect the relay in the correct way.

![nov13](img/nov13.png)

However, I still wasn't able to unlock the box with the relay. After even further investigation, I realized that the relay I had ordered was listed as a 12V relay. I initially thought this menat that it could power a 12V load. However, this dual-circuit electromagnet understanding made me realize that the 12V was actually the voltage required to enable the electromagnet. After connecting the enable pin to the 12V power source, the solenoid finally unlocked. This circuit is shown below.

![nov13](img/nov13.jpg)

However, this was not the desired functionality, as the solenoid was supposed to be powered by a 3.3V enable input from the ESP32, since the ESP cannot send a 12V logic signal. Realizing this, I ordered a 3.3V relay for our system. 

## 2023-11-14

Today my objective was to set up the database subsystem for our project. I knew I wanted to use a serverless database because I wanted something easy to start without any infrastructure management required. However, there were a couple different options I explored, including MongoDB Atlas, AWS, and Google Firebase.

I ultimately landed on Google Firebase for a few reasons:
* Generous free tier
* Well-documented service for hosting serverless databases
* Offers schemaless, no-SQL database

Having a no-SQL database was also an important design consideration. Rather than a SQL database organized into rows and tables, Firebase's data is stored in documents, and ordered into collections. This allowed me a lot more freedom in designing the database and the main software for our project, since documents can store multiple different data types. Having this in a no-SQL format and viewed by a GUI on the Firebase web console also allows anyone to access the database, without requiring any technical knowledge or necessitating a SQL query. This is esential for our project, since it should be able to be used by any course staff in ECE 445 with a low barrier to entry. 

As an initial proof of concept, to just make sure that I understood how to push data into the database, I set up a sample program on the dev board to write the UID of an RFID tag to the database along with the time that it was read. The output of this code in the Firebase console can be seen here. 

![nov14](img/nov14.png)

This [library](https://github.com/mobizt/Firebase-ESP32) was very useful in setting up Database authorizations. 

## 2023-11-15

Today I finished the software functionality to authorize users with RFIDs, display an unlock message to the LCD, and unlock the box. This was the major software component of the project, so there were a couple interesting design decisions involved here. 

#### Software Design Decisions

* Interrupt-based RFID reading: 

