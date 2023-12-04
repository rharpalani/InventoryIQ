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

This [library](https://github.com/mobizt/Firebase-ESP32) was very useful in setting up Firebase authorizations. 

## 2023-11-15

Today I finished the software functionality to authorize users with RFIDs, display an unlock message to the LCD, unlock the box, and store these records in the databse. Though Krish was still working on getting the camera to decode QR codes, I wrote a Python script to mimic this process of sending components from the camera to the main system to see the entire user interface and database functionality at once. This was the major software component of the project, so there were a couple interesting design decisions involved here. 

#### Polling-Based RFID Reading

The first step in the user interaction sequence is authorization through the RFID tag. At this point, since the system waits on an RFID tag to initiate, I had to decide between using polling- or interrupt- based IO. After having taken ECE391, I weighed the pros and cons of this decision, but ultimately decided on polling-based IO for a few reasons:

* Simplicity: Since the system wasn't doing any computation before the user interface sequence was initiated, there would be no process to be interrupted by the RFID tag, making an interrupt-based IO superfluous and overly complicated.
* Predictability: Since polling occurs at predefined intervals, it provides a predictable and consistent approach to checking the RFID reader's status, which is very beneficial given our real-time system. 

With a polling-based system, the loop repeatedly checks whether an RFID is present before initiating the rest of the sequence.

```c
echoLCD("Tap iCard!", "");
while (! mfrc522.PICC_IsNewCardPresent());
```

#### Database Design

I designed the database to maintain 3 types of collections:

* Checkouts: Lists all parts checked out, separated by user
* Inventory: Lists all parts in system, divided by whether they are in the container or outside of it
* Users: Lists all users, including whether they are authorized or not

The completed database looks like this:

![nov15](img/nov15.png)

When storing these components in Firebase, I used a Firebase paradigm suggested in their documentation to invert the data as keys and set the values to either True, None, or some other (don't care) value. This makes checking whether a key is present a very easy process - I just have to try to access the value at /users/$uid/component (for example) to see if a user has checked out a certain component. If that access operation returns None, then I know the component is not present in that list. 

Though it seems like some duplicate information is stored (ie having both a checkouts and inventory list), for this data inversion paradigm to work, the keys need to be stored in multiple places within the database to encode multiple pieces of information. For example, to say that a screwdriver has both been checked out and is checked out to a certain user, it is easier to store this component both in the checkouts and inventory lists. 

An example of this paradigm can be seen here:

```py
def check_component(component):
    try:
        if db.reference(f'inventory/out/{component}').get():
            print("component present")
        else:
            print("component not present")
    except Exception as e:
        print("Database push failed")
        print(str(e))
```

I also created a [Python script](/src/firebase.py) to allow manipulation of the database by ECE 445 course staff. This includes things such as setting the inventory, authorizing users, or checking out components without requiring an RFID tap and QR code scan, in case the TAs need to manually make any adjustments. For example, course staff could use this script to set which users are authorized to access the component container:

```py
def set_users():
    try:

        data = {
            "authorized"  : {
                "Rohan" : True,
                "Rushil" : True,
            },
            "unauthorized" : {
                "Krish" : True,
            }
        }

        db.reference("users").set(data)
        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))
```

#### Software Design to Create a Frictionless UX

When designing the software, I also took the time to think about the user experience and student journey as they go to check out components. Rather than forcing students to manually input whether they are checking out or returning components (specify the type of transaction), my database & software design automatically checks whether the component is in the inventory or has already been checked out, indicating whether the transaction is a borrow or a return, another advantage of having both *checkouts* and *inventory* lists. This allows students to seamlessly tap their iCard and scan the component's QR code, and the system will automatically update the database as a checkout or as a return. 

An excerpt of this process is shown here:

```c
if (Firebase.get(fbdo, "inventory/in/" + component)) {
    // component in box, so student borrowing
    Serial.println("Initiate borrow");

    // checkout entry {component : true}
    FirebaseJson checkoutData;
    checkoutData.set(component, currentTime);

    // inventory entry {component : true}
    FirebaseJson jsonData;
    jsonData.set(component, true);

    // update student checkout record
    Firebase.updateNode(fbdo, "checkouts/" + user, checkoutData);

    // remove part from in record
    Firebase.deleteNode(fbdo, "inventory/in/" + component);

    // add part to out record
    Firebase.updateNode(fbdo, "inventory/out", jsonData);

    echoLCD(component, "Checked out!");
}
```


