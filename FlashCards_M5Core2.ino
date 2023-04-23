#include <M5Core2.h>

// include the SD library:
#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include "SPIFFS.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true
#include <Wire.h>

/*
  Flash Cards application with Teensy 3.6 and 
  RGB LCD Keypad Shield

  Copyright 2021 Tom Dison
  Licensed under GPL 3
  https://gnu.org/licenses/gpl-3.0.html
 */

#define TEXT_XPOS 10
#define LINE1_YPOS 10
#define LINE2_YPOS 60
#define LINE3_YPOS 110
#define LINE4_YPOS 160

#define FC_FILE_NAME "/fcards.txt"
#define SAVED_REPEAT_ARRAY_FILE "/saved.txt"
#define SAVE_CARD_NUMBER_FILE "/cardno.txt"
#define MAX_LINE_LEN 255
char fcBuffer[MAX_LINE_LEN + 1]; // room for \0
const int  FC_DELAY = 2 * 1000;
const int MAX_CARDS = 1000;
int curCardNum = 0;
bool repeatArray[MAX_CARDS];

struct FlashCard {
  String Tagalog;
  String English;
};

bool TagalogFirst = true;

FlashCard currentCard;

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8


#define SD_CS_PIN 4
#define BUTTON_CORRECT  1
#define BUTTON_INCORRECT 2
#define BUTTON_SAVE 3 // Middle Button
#define BUTTON_NONE -1
#define NEW_START 1
#define RESUME 2

int lastMillis = 0;
const int buttonTestDelay = 100;
int btn_pressed = BUTTON_NONE;

const int SCREEN_ROWS = 4;
const int SCREEN_COLS = 16;
const String TRUNC_CHARS = "...";
const int TEXT_SIZE = 3;

File fcFile;



const bool is_debug = true;
bool first_time = true;
bool can_save = false;

void debugOut(String value, bool addLF = true) {
  if(is_debug) {
    if(addLF) {
      Serial.println(value);
    } else {
      Serial.print(value);
    }
  }
}


void initRepeatArray() {
  debugOut("Initializing repeat array...");
  for(int x=0; x<MAX_CARDS; x++) {
    // This way all cards show initially
    repeatArray[x] = true; 
  }
}

boolean saveFileExists() {
  debugOut("Checking for existence of save file...", false);
  boolean bRet = SPIFFS.exists(SAVED_REPEAT_ARRAY_FILE);
  
  debugOut(String(bRet));
  return bRet;
}

boolean saveCardNumberExists() {
 debugOut("Checking for existence of save file...", false);
  boolean bRet = SPIFFS.exists(SAVE_CARD_NUMBER_FILE);
  
  debugOut(String(bRet));
  return bRet;
}

boolean saveRepeatArray() {
  debugOut("Saving repeat array...");


  File save_file = SPIFFS.open(SAVED_REPEAT_ARRAY_FILE, FILE_WRITE);

  if(!save_file) {
    debugOut("Could not create/access save file");
    return false;
  }

  save_file.seek(0);

  for(int x=0; x<MAX_CARDS; x++) {
    // This way all cards show initially
    save_file.write(repeatArray[x]);
  }

  save_file.flush();
  save_file.close();
  return true;
}

int loadCurrentCardNumber() {
  debugOut("Loading saved card number");
  File save_file = SPIFFS.open(SAVE_CARD_NUMBER_FILE, FILE_READ);
  
  if(!save_file) {
    debugOut("Unable to open saved Current Card Number");
    return false;
  }
  String strVal = save_file.readString();
  
  save_file.close();  

  return strVal.toInt(); // Will bw 0 if invalid string
}

boolean saveCurrentCardNumber() {
  debugOut("Saving card number");
  File save_file = SPIFFS.open(SAVE_CARD_NUMBER_FILE, FILE_WRITE);
  
  if(!save_file) {
    debugOut("Failed to open/create saved card file");
    return false;
  }
  save_file.print(curCardNum);
  save_file.flush();
  save_file.close();

  return true;
}

boolean loadRepeatArray() {
  debugOut("Loading repeat array");
  File save_file = SPIFFS.open(SAVED_REPEAT_ARRAY_FILE, FILE_READ);

  if(!save_file) {
    debugOut("Unable to find saved file.");
    initRepeatArray();
    return false;
  }

  for(int x=0; x<MAX_CARDS; x++) {
    // This way all cards show initially
    if(save_file.available()) {
      int read = save_file.read();

      if(0 == read) {
        repeatArray[x] = false;
      } else {
        repeatArray[x] = true;
      }
    }
  }

  save_file.flush();
  save_file.close();
  return true;
}


void markCardForRepeat(int whichCard/*1-based*/, bool doRepeat) {
  if(whichCard <= MAX_CARDS && whichCard > 0) {
    repeatArray[whichCard - 1] = doRepeat;
  }
}

bool shouldShow(int whichCard/*1-based*/) {
  debugOut("Should show, card number ");
  debugOut(String(whichCard), false);
  boolean bRet = repeatArray[whichCard - 1] == true;

  debugOut(" = ", false);
  debugOut(String(bRet));

  return bRet;
}

// 1 -based line
int getYPostForLine(int whichLine) {
  switch(whichLine) {
    case 1:
      return LINE1_YPOS;
    case 2:
      return LINE2_YPOS;
    case 3:
      return LINE3_YPOS;
    case 4:
      return LINE4_YPOS;
    default:
      return LINE1_YPOS;
  }
}

// Lines are 1 based
void printScreen(const char* line, bool cls = true, int whichLine = 1, bool isAnswer = false) {
  // Clear screen
  if(cls) {
    M5.lcd.clear(TFT_BLACK);
  }

  M5.lcd.setCursor(TEXT_XPOS,getYPostForLine(whichLine));
  M5.lcd.print(line);
}

void setupScreen() {
  // set up the LCD's number of columns and rows:
  M5.lcd.begin();

  // clear the screen with a black background
  M5.lcd.fillScreen(TFT_BLACK);
  
  
  M5.lcd.setTextColor(TFT_WHITE);
  // set the font size
  M5.lcd.setTextSize(2);
  M5.lcd.setCursor(TEXT_XPOS, LINE1_YPOS);
  M5.lcd.print("FlashCards by Tom Dison");
  M5.lcd.setCursor(TEXT_XPOS, LINE2_YPOS);  
  M5.lcd.print("Btn 1 is Incorrect");
  M5.lcd.setCursor(TEXT_XPOS, LINE3_YPOS);  
  M5.lcd.print("Btn 3 is Correct");
  M5.lcd.setCursor(TEXT_XPOS, LINE4_YPOS);  
  M5.lcd.print("Btn 2 is Save");
  M5.lcd.setTextSize(TEXT_SIZE);
}

bool setupSDCard() {
    debugOut("\nInitializing SD card...", false);


  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!SD.begin()) {
    debugOut("initialization failed. Things to check:");
    debugOut("* is a card inserted?");
    debugOut("* is your wiring correct?");
    debugOut("* did you change the chipSelect pin to match your shield or module?");
    return false;
  } else {
   debugOut("Wiring is correct and a card is present.");
  }

  return true;
}

// read the buttons
int read_buttons()
{
    M5.update();
    
  // Read save PIN first
  if(M5.BtnB.read()) {
    return BUTTON_SAVE;
  }

  // Then LEFT
  if(M5.BtnA.read()) {
    return BUTTON_INCORRECT;
  }

  // THEN RIGHT
  if(M5.BtnC.read()) {
    return BUTTON_CORRECT;
  }

  return BUTTON_NONE;
}

int showMenu() {
  String choice1 = "1: Btn 1 = new"; 
  String choice2 = "2: Btn 2 = resume"; 
  int key = -1;

  printScreen(choice1.c_str(), true, 1);
  printScreen(choice2.c_str(), false, 2);

  while(!(BUTTON_INCORRECT == key || BUTTON_SAVE == key)) {
      key = read_buttons();

      switch(key) {
        case BUTTON_INCORRECT:
          return NEW_START;
        case BUTTON_SAVE:
          return RESUME;
        default:
        delay(100);
      }
  }

  return -1;
}

bool setupSPIFFS() {
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    debugOut("SPIFFS Mount Failed");
    return false;
  }

  return true;
}

void setup()
{
 // Open serial communications and wait for port to open:
  if(is_debug) {
    Serial.begin(115200);
     
    while (!Serial) {
      ; // wait for serial port to connect.
    }
  }

  M5.begin();
  
  can_save = setupSPIFFS();

  debugOut("can_save = ", false);
  debugOut(String(can_save));

  initRepeatArray();

  setupScreen();
  
  if(!setupSDCard()) {
    printScreen("Failed to access SD card!", true);
    debugOut("Failed to access SD card!");
    while(1); // don't continue
  }

  fcFile = SD.open(FC_FILE_NAME, FILE_READ);

  if(!fcFile) {
    printScreen("Could not open flash card file!");
    debugOut("Failed to access SD card!");
    while(1);
  }

  fcFile.seek(0);
}

bool isLineTerminator(char c) {
  return '\r' == c || '\n' == c;
}

String readLine() {
  int charsRead = 0;

  while(!isLineTerminator(fcFile.peek())
  && charsRead <= MAX_LINE_LEN && fcFile.available()) {
     fcBuffer[charsRead] = fcFile.read();
     charsRead++; // Yes could be done in one line
  }

  fcBuffer[charsRead] = '\0';

  // Now remove line terminator(s)
  while(fcFile.available() && isLineTerminator(fcFile.peek())) {
    fcFile.read(); // discard
    debugOut("Discarded terminator");
  }

  
  return String(fcBuffer);
}

void readNextCard() {
  currentCard.Tagalog = "";
  currentCard.English = "";

 if(!fcFile.available()) {
    fcFile.seek(0);
    curCardNum = 0;
  }

  currentCard.Tagalog = readLine();
  currentCard.English = readLine();

  debugOut(currentCard.Tagalog);
  debugOut(currentCard.English);
}

int getNextChunkPos(int startPos, String value, int max_chars,
    bool truncate) {
  int spacePos = 0;
  int lastPos = startPos + max_chars;
  
  if(lastPos > value.length()) {
    lastPos = value.length();
  } else {
    spacePos = value.lastIndexOf(' ', lastPos);
    if(spacePos != -1 && spacePos > startPos) {
      lastPos = spacePos;
    }

    if(truncate && 
        ((lastPos - startPos) > (max_chars - TRUNC_CHARS.length()))) {
      // We need to allow room for ...
      spacePos = value.lastIndexOf(' ', lastPos - 1);
      
      if(spacePos != -1 && spacePos > startPos) {
        lastPos = spacePos; 
      }
    }
  }

  return lastPos;
}

void displayString(String value, bool isAnswer) {
  int startPos = 0;
  int curLine = 1;
  int lastPos = 0;
  bool moreChunks = true;
  String line = "";

  if(isAnswer) {
    M5.lcd.setTextColor(TFT_YELLOW);
  } else {
    M5.lcd.setTextColor(TFT_WHITE);
  }

  while(moreChunks) {
    lastPos = getNextChunkPos(startPos, value, SCREEN_COLS, curLine == SCREEN_ROWS);

    if(lastPos == -1) {
      moreChunks = false;
    } else {
      moreChunks = lastPos < value.length();

      if(curLine == SCREEN_ROWS && moreChunks) {
        line = value.substring(startPos, lastPos) + TRUNC_CHARS;
      } else {
        line = value.substring(startPos, lastPos);
      }

      printScreen(line.c_str(), curLine == 1, curLine, isAnswer);
  
      if(moreChunks) {
        startPos = lastPos;
        
        if(value.charAt(startPos) == ' ') {
          startPos++;
        }
        
        if(++curLine == SCREEN_ROWS + 1) {
          curLine = 1;
          delay(FC_DELAY); // Leave this part of the verse up
        }
      } else {
        if(!isAnswer) {
            delay(FC_DELAY); // we are done with card
        }
      }
    }
  }
}

bool reOpenCardFile() {
  fcFile.close();

  curCardNum = 0;
  
  fcFile =  SD.open(FC_FILE_NAME, FILE_READ);

  if(!fcFile) {
    printScreen("Could not open flash card file!");
    debugOut("Failed to access SD card!");
    return false;
  } 

  TagalogFirst = !TagalogFirst;
  return true;
}

bool displayCard() {
  if(currentCard.Tagalog.length() == 0 ||
      currentCard.English.length() == 0) 
  {
    debugOut("Empty Flash Card!");
    return false;
  }

  if(TagalogFirst) {
    displayString(currentCard.Tagalog.c_str(), false);
    displayString(currentCard.English.c_str(), true);
  } else {
    displayString(currentCard.English.c_str(), false);
    displayString(currentCard.Tagalog.c_str(), true);
  }

  return true;
}

bool showNextCard() {
  debugOut("Reading next Card");
  readNextCard();
  curCardNum++;

  while(!shouldShow(curCardNum)) {
    readNextCard();
    curCardNum++;
  }

  bool result = displayCard();
  debugOut("Result of display card is: ", false);
  debugOut(String(result)); 

  if(false == result) {
    debugOut("...Reopending card file", true);
    if(!reOpenCardFile()) {
      printScreen("Could not restart flash cards!");
      debugOut("Failed to reopen flash card file!");
      while(1);
    }
    result =  displayCard();
  }
  
  return result;
}

void doNothing() {

}

void doNext() {
  if(!showNextCard()) {
    printScreen("Error showing flash cards!");
    debugOut("Error showing flash cards");
    while(1);    
  }
}

void doSave() {
  debugOut("DoSave pressed.");
  printScreen("Saving...");
  delay(500);
  if(saveRepeatArray() &&
    saveCurrentCardNumber()) {
    printScreen("Saved!");
  } else {
    printScreen("Error saving!");
  }
  delay(1000);
}

void skipCards(int dest_card_no) {
  curCardNum++;

  debugOut("skipping card to saved card num ", false);
  debugOut(String(dest_card_no));

  while(curCardNum < dest_card_no) {
    readNextCard();
    curCardNum++;
  }

  debugOut("Skipped cards, current card is now ", false);
  debugOut(String(curCardNum));
}

void loop(void) {
  int curMillis = millis();

  if(first_time) {
    first_time = false;
    if(can_save && saveFileExists()) {
      int sel = showMenu();

      if(RESUME == sel) {
        printScreen("Loading saved progress...");
        if(!loadRepeatArray()) {
          debugOut("Failed to load save");
          printScreen("Resume failed.");
          delay(2000);
        } {
          int saved_card_no = loadCurrentCardNumber(); // don't worry if fails, will be 0
          if(saved_card_no > 0) {
            skipCards(saved_card_no);
          }
          printScreen("Resume success!");
        }
      } else {
        initRepeatArray();
      }
    } else {
      initRepeatArray();
    }
    delay(1000);
  }
  
  if((curMillis - lastMillis) > buttonTestDelay) {
    lastMillis = curMillis;
    //int test = digitalRead(BUTTON_NEXT_PIN);
    btn_pressed = read_buttons();


    switch(btn_pressed) {
      case BUTTON_CORRECT:
        debugOut(" Next Pressed");
        markCardForRepeat(curCardNum,false);
        doNext();
        break;
      case BUTTON_SAVE:
        if(can_save) {
          doSave();
        }
        break;
      case BUTTON_INCORRECT:
        debugOut("IncorrectPressed");
        // Handle saving incorrect card
        markCardForRepeat(curCardNum,true);
        doNext();
        break;
      default:
      doNothing();
    }   
  }
}
