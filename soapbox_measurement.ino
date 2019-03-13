// **************************************************************************************
// Soap box measurement system
// **************************************************************************************

// **************************************************************************************
// License
// **************************************************************************************

/*
MIT License

Copyright (c) 2019 Thomas Kleinknecht & Patrick Fial

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// **************************************************************************************
// Includes
// **************************************************************************************

#include <LiquidCrystal.h>
#include <EEPROM.h>

// **************************************************************************************
// customization
// **************************************************************************************

//#define ENGLISH
#define GERMAN

#define USE_EEPROM      // comment out/remove to disable eeprom access 
#define SHIELD_TYPE 0   // 0: "DFRobot SKU: DFR0009" 1: "Velleman VMA203 shield"

// **************************************************************************************
// definitions
// **************************************************************************************

// button definitions

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// menu item counts

#define itemcntROOT 3
#define itemcntMEASURE 1
#define itemcntLIST 30
#define itemcntCALIBRATE 2

// state machine for measurements

#define msReady   0
#define msRunning 1
#define msPaused  2
#define msStopped 3

// state machine for calibration

#define csOff 0
#define csOn  1

#define lightNoContact 0
#define lightContact 1

// lichtschranke detection window

#define DETECTED_NONE 0
#define DETECTED_START 1
#define DETECTED_STOP 2

#define DETECTION_WINDOW_SIZE 3
#define START_DETECTION_TIMEFRAME 500
#define STOP_DETECTION_TIMEFRAME 2000

#define BUTTON_DEBOUNCE_INTERVAL 50
#define DELETE_INTERVAL 1000*5

#ifdef ENGLISH
#  define LAUNCHSCREEN_UPPER "SB Measure      "
#  define LAUNCHSCREEN_LOWER "v1.0.0          "
#else
#  define LAUNCHSCREEN_UPPER "SK Messsystem   "
#  define LAUNCHSCREEN_LOWER "v1.0.0          "
#endif

#define DOTCOUNT 12

// **************************************************************************************
// objects
// **************************************************************************************

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// **************************************************************************************
// menus
// **************************************************************************************

#ifdef ENGLISH
const char* menuRoot[] =
{
  " Measurement   ",
  " Results       ",
  " Calibration   ",
  0
};

const char* menuMeasure[] = 
{
  " Save          ",
  0
};

char* menuList[itemcntLIST];

const char* menuCalibrate[] =
{
  " Start         ",
  " Stop          ",
  0
};
#else
const char* menuRoot[] =
{
  " Zeitmessung   ",
  " Ergebnisse    ",
  " Kalibrierung  ",
  0
};

const char* menuMeasure[] = 
{
  " Speichern     ",
  0
};

char* menuList[itemcntLIST];

const char* menuCalibrate[] =
{
  " Start         ",
  " Stop          ",
  0
};
#endif

const char* dots[] =
{
  "  >             ",
  "   >            ",
  "    >           ",
  "     >          ",
  "      >         ",
  "       >        ",
  "        >       ",
  "         >      ",
  "          >     ",
  "           >    ",
  "            >   ",
  "             >  "
  };


// **************************************************************************************
// flags/markers/pointers
// **************************************************************************************

const char** currentMenu= menuRoot;
int currentMenuItems= itemcntROOT;

int lastButton = 0;
int showInitialMenu= 1;
int lastButtonState= 0;
int launchScreenShown= 0;
int upperLine= 0;
int lowerLine= 1;
int selection = 0;
int subMenuSelection= 0;
int dotIndex = 0;
int deleteConfirms = 0;

// **************************************************************************************
// submenu states
// **************************************************************************************

int stateMeasurement= msReady;
int stateCalibrate= csOff;

// **************************************************************************************
// timers
// **************************************************************************************

unsigned long timeStarted= -1;
unsigned long timeAccumulated= 0;
unsigned long lastMeasurement= 0;
unsigned long lastUpdate= 0;
unsigned long lastDotUpdate= 0;

// **************************************************************************************
// light sensor
// **************************************************************************************

int hadContact= 0;
int lastSensorValue = 0;

// **************************************************************************************
// functions
// **************************************************************************************

int getPressedButton();
void scroll(int down);
void printMenu(int contact= 0);
int msToString(unsigned long total, char* buf);
void saveMeasurement(unsigned long total);
void deleteMeasurement(int index);
void triggerStart();
void triggerStop();
void triggerStartStop();


int getLightDetection();
void loadEEPROM();
void saveEEPROM();
unsigned long millis();

const char* toButtonName(int val)
{
  switch (val)
  {
    case btnRIGHT:  return "btnRIGHT";
    case btnUP:     return "btnUP";
    case btnDOWN:   return "btnDOWN";
    case btnLEFT:   return "btnLEFT";
    case btnSELECT: return "btnSELECT";
    case btnNONE:   return "btnNONE";
  }

  return "???";
}

// **************************************************************************************
// Setup
// **************************************************************************************

void setup()
{
  for (int i= 0; i < itemcntLIST; i++)
    menuList[i]= (char*)calloc(16+1, 1);
  
  Serial.begin(9600);
  Serial.println("---------------------------- STARTING ------------------------");

  pinMode(A1, INPUT);
  
  lcd.begin(16,2);

  loadEEPROM();
}

// **************************************************************************************
// Loop
// **************************************************************************************

void loop()
{
  // upon start, show launch screen for given duration
  
  if (!launchScreenShown || ((millis() - launchScreenShown) < 3000))
  {
    if (!launchScreenShown)
      launchScreenShown= millis();

    lcd.setCursor(0,0);
    lcd.print(LAUNCHSCREEN_UPPER);
    lcd.setCursor(0,1);
    lcd.print(LAUNCHSCREEN_LOWER);
      
    return;
  }
  else if (showInitialMenu)
  {
    printMenu();
    showInitialMenu= 0;
  }

  // get input from buttons / light sensor, and evaluate triggers
  
  int button = getPressedButton();
  int sensor= getLightDetection();

  // light sensor first

  if (sensor != lastSensorValue)
  {
    lastSensorValue= sensor;
    
    if (sensor == lightContact)
    {
      // nothing to do when sensor has contact

      hadContact= 1;
    }
    else
    {
      // contact lost, trigger start or stop

      hadContact= 0;
      triggerStartStop();
    }
  }
 
  // no button was pressed. evtl. update running time measurement

  if (currentMenu == menuMeasure && stateMeasurement == msRunning)
  {
    // update only every 50ms at most

    if ((millis() - lastUpdate) > 50)
    {
      lastUpdate= millis();
      printMenu();
    }    
  }

  if (currentMenu == menuCalibrate)
  {
    // only print menu, no buttons

    printMenu(hadContact);
  }

  // check if we have a button CHANGE

  if (button == lastButton)
    return;

  lastButton= button;

  // normally, do NOTHING when no button was pressed

  if (button == btnNONE)
    return;

  // button was pressed, check action

  switch (button)
  {
    case btnUP:
    case btnDOWN:
    {
      // menu up/down

      deleteConfirms= 0;
      scroll(button == btnDOWN);
      printMenu();
      break;
    }

    case btnSELECT:
    {
      // menu "select" button
      
      if (currentMenu == menuRoot)
      {
        int selectedItem = !selection ? upperLine : lowerLine;

        switch (selectedItem)
        {
          case 0: currentMenu= menuMeasure;             currentMenuItems= itemcntMEASURE;   break;
          case 1: currentMenu= (const char**)menuList;  currentMenuItems= itemcntLIST;      break;
          case 2: currentMenu= menuCalibrate;           currentMenuItems= itemcntCALIBRATE; break;
          default: break;
        }

        printMenu();
      }
      else 
      {
        if (currentMenu == menuMeasure)
          return selectMeasure();

        if (currentMenu == menuList)
        {
          if (deleteConfirms == 2)
          {
            deleteMeasurement(subMenuSelection);
            deleteConfirms= 0;
            printMenu();
            saveEEPROM();
          }
          else
          {
            deleteConfirms++;
            printMenu();
          }
        }
      }
      
      break;
    }

    case btnLEFT:
    {
      // menu "back" button
      
      if (currentMenu == menuRoot || (currentMenu == menuMeasure && stateMeasurement == msRunning))
        return;

      deleteConfirms= 0;
      subMenuSelection= 0;
      currentMenu= menuRoot;
      currentMenuItems= itemcntROOT;
      printMenu();
      
      break;
    }

    case btnRIGHT:
    {
      // shortcut to go directly to measurements menu, from everywhere

      currentMenu= menuMeasure;
      currentMenuItems= itemcntMEASURE;
      printMenu();

      break;
    }
    
    default: break;
  }
}

// **************************************************************************************
// Trigger start/stop
// **************************************************************************************

void triggerStart()
{
  if (currentMenu != menuMeasure || stateMeasurement != msReady)
    return;

  stateMeasurement= msRunning;
  timeStarted= millis();
  lastMeasurement= 0;
}

void triggerStop()
{
  if (stateMeasurement != msRunning)
    return;

  dotIndex= 0;
  lastDotUpdate= 0;
  stateMeasurement= msStopped;
  lastMeasurement= timeAccumulated + timeStarted ? (millis() - timeStarted) : 0;
  timeStarted= timeAccumulated= 0;
  printMenu();
}

void triggerStartStop()
{
  // triggered by light sensor
  
  if (currentMenu != menuMeasure)
    return;

  if (stateMeasurement == msReady)
    return triggerStart();

  if (stateMeasurement == msRunning)
    return triggerStop();
}

// **************************************************************************************
// Select Measure
// Save measurement
// **************************************************************************************

void selectMeasure()
{
  switch (subMenuSelection)
  {
    case 0:
    {
      // save

      if (!lastMeasurement || stateMeasurement != msStopped)
        return;

      saveMeasurement(lastMeasurement);
      lastMeasurement= 0;
      timeStarted= timeAccumulated= 0;

      stateMeasurement= msReady;
      printMenu();

      saveEEPROM();
    }

    default:
      break;
  }
}

// **************************************************************************************
// Scroll
// menu up/down
// **************************************************************************************

void scroll(int down)
{
  if (currentMenu == menuRoot)
  {
    if (down && lowerLine >= (currentMenuItems-1) && selection)
      return;
  
    if (!down && upperLine == 0 && !selection)
      return;
  
    if (down)
    {
      if (selection)
      {
        upperLine++;
        lowerLine++;
      }
  
      selection= 1;
    }
    else
    {
      if (!selection)
      {
        upperLine--;
        lowerLine--;
      }
  
      selection= 0;
    }
  }
  else
  {
    if (down && subMenuSelection >= (currentMenuItems-1))
      return;

    if (!down && subMenuSelection == 0)
      return;

    subMenuSelection= down ? subMenuSelection+1 : subMenuSelection-1;
  }
}

// **************************************************************************************
// Print Menu
// rootmenu / submenu
// **************************************************************************************

void printMenu(int contact)
{
  char line[16+1];

  if (currentMenu == menuRoot)
  {
    const char* upperText = upperLine >= 0 && upperLine < currentMenuItems ? currentMenu[upperLine] : " Error         ";
    const char* lowerText = lowerLine >= 0 && lowerLine < currentMenuItems ? currentMenu[lowerLine] : " Error         ";
    
    sprintf(line, "%c%s", selection ? ' ' : '>', upperText);
    lcd.setCursor(0,0);
    lcd.print(line);
  
    sprintf(line, "%c%s", selection ? '>' : ' ', lowerText);
    lcd.setCursor(0,1);
    lcd.print(line);
  }
  else if (currentMenu)
  {
    if (currentMenu == menuMeasure)
    {
      const char* text= subMenuSelection >= 0 && subMenuSelection < currentMenuItems ? currentMenu[subMenuSelection] : " Error         ";

      lcd.setCursor(0,1);

      // add some pretty animated dots effect
      
      if (stateMeasurement == msRunning)
      {
        if (!lastDotUpdate)
        {
          lastDotUpdate= millis();
        }
        else if ((millis() - lastDotUpdate) > 500)
        {
          lastDotUpdate= millis();
          dotIndex= (dotIndex+1) % DOTCOUNT;
        }

        lcd.print(dots[dotIndex]);
      }
      else
      {
        sprintf(line, ">%s", text);
        lcd.print(line);
      }

      lcd.setCursor(0,0);

      switch (stateMeasurement)
      {
        case msReady:     lcd.print("  --:--:--.---  "); break;
        case msStopped:
        case msPaused:
        case msRunning:
        {
          unsigned long total;
          char buf[16+1];
          char buf2[16+1];

          if (stateMeasurement == msRunning)
            total= millis() - timeStarted + timeAccumulated;
          else if (stateMeasurement == msStopped)
            total= lastMeasurement ? lastMeasurement : 0;
          else
            total= timeAccumulated;

          msToString(total, buf);

          sprintf(buf2, "%c %s%*s", stateMeasurement == msRunning ? ' ' : (stateMeasurement == msStopped ? ' ' : ' '), buf, 16-strlen(buf)-2, "");

          lcd.print(buf2);
          break;
        }
        
        default: break;
      }
    }
    else if (currentMenu == menuList)
    {
      const char* textLine1= subMenuSelection >= 0 && subMenuSelection < currentMenuItems ? currentMenu[subMenuSelection] : "              ";
      const char* textLine2= "> Loeschen?    ";

#ifdef ENGLISH
      textLine2= "> Delete?      ";

      if (deleteConfirms == 1)
        textLine2= "> Delete?      ";
      else if (deleteConfirms == 2)
        textLine2= "> Really?      ";
#else
      if (deleteConfirms == 1)
        textLine2= "> Loeschen?    ";
      else if (deleteConfirms == 2)
        textLine2= "> Wirklich?    ";
#endif      

      char line1[16+1];

      if (!*textLine1)
        sprintf(line1, "%d%s --:--:--.--- ", subMenuSelection+1, subMenuSelection >= 9 ? "" : " ");
      else
        sprintf(line1, "%d%s %12.12s ", subMenuSelection+1, subMenuSelection >= 9 ? "" : " ", textLine1);

      lcd.setCursor(0,0);
      lcd.print(line1);
      lcd.setCursor(0,1);
      lcd.print(textLine2);
    }
    else if (currentMenu == menuCalibrate)
    {
#ifdef ENGLISH
      lcd.setCursor(0,0);
      lcd.print("  Calibration   ");
      lcd.setCursor(0,1);
      lcd.print(contact ? "  - contact -   " : " - no contact - ");
#else
      lcd.setCursor(0,0);
      lcd.print("  Kalibrierung  ");
      lcd.setCursor(0,1);
      lcd.print(contact ? "  - Kontakt -   " : "- kein Kontakt -");
#endif
    }
  }
}

// **************************************************************************************
// Save Measurement
// **************************************************************************************

void saveMeasurement(unsigned long total)
{
  for (int i= itemcntLIST-1; i >= 0; i--)
  {
    if (i)
    {
      if (*menuList[i-1])
        strcpy(menuList[i], menuList[i-1]);
    }
    else
    {
      msToString(total, menuList[i]);
    }
  }
}

// **************************************************************************************
// Stop Measurement
// **************************************************************************************

void deleteMeasurement(int index)
{
  if (index < 0 || index > itemcntLIST+1)
    return;

  for (int i= index; i < itemcntLIST; i++)
  {
    *menuList[i]= 0;

    if (i < itemcntLIST-1)
    {
      strcpy(menuList[i], menuList[i+1]);
      *menuList[i+1]= 0;
    }
  }
}

// **************************************************************************************
// getLightDetection
// **************************************************************************************

int getLightDetection()
{
  int lightValue= analogRead(A1);

  if (lightValue > 500)
    return lightContact;

  return lightNoContact;
}

// **************************************************************************************
// Ms To String
// **************************************************************************************

int msToString(unsigned long total, char* buf)
{
  if (!buf)
    return -1;

  // calculate pretty-print parts

  unsigned long hours= total / 3600000;

  total %= 3600000;

  unsigned long minutes= total / 60000;

  total %= 60000;
  
  unsigned long secs= (unsigned long)(total / 1000);
  unsigned long ms= total % 1000;

  return sprintf(buf, "%0.2ld:%0.2ld:%0.2ld.%0.3ld", hours, minutes, secs, ms);
}

// **************************************************************************************
// Get Pressed Button
// **************************************************************************************

int getPressedButton()
{
  int value= analogRead(0);

  if (value >= 1000)
  {
    lastButtonState= 0;
    return btnNONE;
  }

  if (!lastButtonState || ((millis() - lastButtonState) < BUTTON_DEBOUNCE_INTERVAL))
  {
    if (!lastButtonState)
      lastButtonState = millis();

    return;
  }

  lastButtonState= 0;

  if (value > 1000) return btnNONE;

  if (SHIELD_TYPE == 0)
  {
    if (value < 50)   return btnRIGHT;          // 0
    if (value < 250 || (value >= 600 && value <= 615))  return btnUP;             // 129-130, 612
    if (value < 470)  return btnDOWN;           // 306-307, 340, 468
    if (value < 650)  return btnLEFT;           // 479-480
    if (value < 850)  return btnSELECT;         // 721 - 723, 747, 730, 728
  }

  if (SHIELD_TYPE == 1)
  {
    if (value < 50)   return btnRIGHT;          // 0
    if (value < 150)  return btnUP;             // 100
    if (value < 300)  return btnDOWN;           // 256
    if (value < 500)  return btnLEFT;           // 409
    if (value < 750)  return btnSELECT;         // 640
  }
  
  return btnNONE;
}

// **************************************************************************************
// load EEPROM
// **************************************************************************************

void loadEEPROM()
{
#ifdef USE_EEPROM
  int col= 0;
  int line= 0;
  int address = 0;
  byte eepromValue = EEPROM.read(address);

  while (eepromValue != 255)
  {
    menuList[line][col]= (char)eepromValue;
    address++;
    col++;

    if (col == 12)
    {
      col= 0;
      line++;
    }

    if (line >= itemcntLIST-1)
      return;

    eepromValue = EEPROM.read(address);
  }
#endif
}

// **************************************************************************************
// save EEPROM
// **************************************************************************************

void saveEEPROM()
{
#ifdef USE_EEPROM
  int address = 0;
  
  for (int i= 0; i < itemcntLIST; i++)
  {
    if (!*menuList[i])
      break;

    for (int k= 0; k < 12; k++, address++)
      EEPROM.write(address, (int)menuList[i][k]);
  }  
      
  EEPROM.write(address, 255);
#endif
}
