#include "variant.h"
#include <due_can.h>
#include "LedControl.h"

// Leave defined if you use native port, comment if using programming port
// #define Serial SerialUSB


// (dataPin/TX, clockPin, load_csPin/CTRL, numDevices)
LedControl display = LedControl(29, 28, 36, 1);
LedControl wheel = LedControl(46, 38, 47, 1);

int sentMessage = 0;
volatile unsigned long userSentTime = 0;
volatile unsigned long logTime = 0;

void setup() {
    Can0.begin(CAN_BPS_250K);

    Can0.watchFor();

    // 50 is to the button, 51 is to the switch
    attachInterrupt(51, send_message, CHANGE); 
    attachInterrupt(50, logging, CHANGE);

    // Wake up the MAX7219
    display.shutdown(0, false);
    wheel.shutdown(0, false);
    // Set the brightness to max
    display.setIntensity(0, 15);
    wheel.setIntensity(0, 15);
    // Wipe the previous numbers
    display.clearDisplay(0);
    wheel.clearDisplay(0);

    updateSpeed(88);
    updateID(5);
    updateDataDecimal2(1234);

    for (int i = 1; i <= 13; i++) {
        pinMode(i, OUTPUT);
        digitalWrite(i, HIGH);
    }
}



void loop() {
    CAN_FRAME message;
    int id;
    int data;
    int hours, minutes, seconds;

    if (Can0.available() > 0) {
        Can0.read(message);

        id = message.id;

        if (sentMessage > 0) {
          sentMessage--;
        }

        // Only One ID carries the speed data
        if (id == 0xEE && !sentMessage) {
            data = message.data.bytes[0];
            updateSpeed(data);
            return;
        }
        // This range will send single decimal place floats
        else if (id == 0xBB) {
            data = 0;
            data |= ((int) message.data.bytes[0]) << 8;
            data |= (int) message.data.bytes[1];
            updateDataDecimal(data);
        }
        // This range will send two decimal place floats
        else if (id == 0xCC) {
            data = 0;
            data |= ((int) message.data.bytes[0]) << 8;
            data |= (int) message.data.bytes[1];
            updateDataDecimal2(data);
        
        } 
        // This is for time
        else if (id == 0xDD) {
            hours = (int) message.data.bytes[0];
            minutes = (int) message.data.bytes[1];
            seconds = (int) message.data.bytes[2];
            displayTime(hours, minutes, seconds);
        } 
        // Anything else is just an integer
        else {
            data = 0;
            data |= ((int) message.data.bytes[0]) << 8;
            data |= (int) message.data.bytes[1];
            updateData(data);
        }
        if (id != 0xDD)
          updateID((int) message.data.bytes[2]);
    }
}


/**
 * This method sends the message to the RaceCapture when a driver presses a 
 * button on the steering wheel. This is used to signal that the driver wants
 * some data flagged, such as completing a lap or right before or after turns.
 * It is up to the driver to determine what to flag.
 */ 
void send_message() {
  if (millis() - userSentTime >= 50) {
    CAN_FRAME flag;
    flag.id = 0x50;
    flag.extended = false;
    flag.length = 1;
    flag.data.bytes[0] = 42;
    Can0.sendFrame(flag);
    wheel.setDigit(0, 0, 15, false);
    wheel.setDigit(0, 1, 15, false);
    sentMessage = 5;
  }
  userSentTime = millis();
}


void logging() {
  if (millis() - logTime >= 50) {
      CAN_FRAME message;
      if (digitalRead(15)) 
          message.id = 0x32;
      else
          message.id = 0x26;
      message.extended = false;
      message.length = 1;
      message.data.bytes[0] = 20;
      Can0.sendFrame(message);
      wheel.setDigit(0, 0, 11, false);
      wheel.setDigit(0, 1, 11, false);
      sentMessage = 5;
  }
  logTime = millis();
}

void displayTime(int hour, int minute, int second) {
    int hrl = hour % 10;
    int hrh = hour / 10;
    display.setDigit(0, 0, (byte) hrh, false);
    display.setDigit(0, 1, (byte) hrl, true);
    int minl = minute % 10;
    int minh = minute / 10;
    display.setDigit(0, 2, (byte) minh, false);
    display.setDigit(0, 3, (byte) minl, true);
    int secl = second % 10;
    int sech = second / 10;
    display.setDigit(0, 4, (byte) sech, false);
    display.setDigit(0, 5, (byte) secl, false);
}

/**
 * This method updates the two digits of the wheel to the speed that 
 * the driver is currently going, according to the data recieved from the 
 * CAN message. This used ID 0xEE, so it is easy to identify
 */
void updateSpeed(int speed) {
    // Assuming we aren't going above 100 mph
    int ones = speed % 10;
    speed /= 10;
    int tens = speed;

    // The speed displays are the last two on the MAX7219

    if (tens > 0)
        wheel.setDigit(0, 1, (byte) tens, false);
    else
        wheel.setChar(0, 1, ' ', false);
    wheel.setDigit(0, 0, (byte) ones, false);
}

/**
 * This method updates the first two digits of the display with the option of the
 * message that was recieved. This is from the third field of the CAN message
 */
void updateID(int num) {
    int digitTwo = num % 10;
    num /= 10;
    int digitOne = num % 10;

    display.setDigit(0, 0, (byte) digitOne, false);
    display.setDigit(0, 1, (byte) digitTwo, true);
}

/**
 * This method updates the third through the sixth digits of the display with
 * the data that was recieved. This is from the 'data' array of the CAN
 * message. These messages are generally small numbers, so an int should be 
 * able to hold it all.
 */
void updateData(int num) {
    int ones = num % 10;
    num /= 10;
    int tens = num % 10;
    num /= 10;
    int hundreds = num % 10;
    num /= 10;
    int thousands = num % 10;

    bool numberStarted = false;

    if (thousands > 0) {
        display.setDigit(0, 2, (byte) thousands, false);
        numberStarted = true;
    } else  
        display.setChar(0, 2, ' ', false);

    if (numberStarted || hundreds > 0) {
        display.setDigit(0, 3, (byte) tens, false);
        numberStarted = true;
    } else
        display.setChar(0, 3, ' ', false);

    if (numberStarted || tens > 0)
        display.setDigit(0, 4, (byte) tens, false);
    else
        display.setChar(0, 4, ' ', false);

    display.setDigit(0, 5, (byte) ones, false);
}

void updateDataDecimal(int num) {
    int tenths = num % 10;
    num /= 10;
    int ones = num % 10;
    num /= 10;
    int tens =num % 10;
    num /= 10;
    int hundreds = num;

    bool numberStarted = false;

    if (hundreds > 0) {
        display.setDigit(0, 2, (byte) hundreds, false);
        numberStarted = true;
    } else  
        display.setChar(0, 2, ' ', false);

    if (numberStarted || tens > 0)
        display.setDigit(0, 3, (byte) tens, false);
    else
        display.setChar(0, 3, ' ', false);

    display.setDigit(0, 4, (byte) ones, true);
    display.setDigit(0, 5, (byte) tenths, false);
}

void updateDataDecimal2(int num) {
    int hundreths = num % 10;
    num /= 10;
    int tenths = num % 10;
    num /= 10;
    int ones = num % 10;
    num /= 10;
    int tens =num % 10;
    num /= 10;

    if (tens > 0)
        display.setDigit(0, 2, (byte) tens, false);
    else  
        display.setChar(0, 2, ' ', false);

    display.setDigit(0, 3, (byte) ones, true);
    display.setDigit(0, 4, (byte) tenths, false);
    display.setDigit(0, 5, (byte) hundreths, false);
}

