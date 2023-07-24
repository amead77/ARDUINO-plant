/**
 *  \file plant.ino
 *  \brief 
 *  Version 0.2
 *  Microcontroller : Arduino Mini
 *  Date : 2013-12-24 .. 2013-12-30
 *  Author : Adam Mead - amead77 #at# gmail.com, plus some diddy bits from the examples
 *  Licence: Creative Commons Attribution http://creativecommons.org/licenses/by/4.0/
 *  Details
 *  
 *  Plant waterer, maybe, or maybe an alarm clock..
 *  Working is: keypad, 20x4 serial oled, clock, set time/date/trigger, relay output.
 *  temp sensor.
 *  
 *  Uses SoftwareSerial because I use the hardware TX/RX pins but need to invert the signal.
 *  This is the first time I've used an Arduino or C/C++.
 *  
 *  If you make any changes or improvements to the code please forward them to me.
 *  
 *  TODO
 *  read flow meter
 *  multiple triggers
 *  
 *  figure out c++/arduino Strings properly or use char arrays and clean up code.
 *  program uses a massive amount of resources, jumped up a lot when DallasTemperature and OneWire
 *  was added.
 *  
 */


#include <Wire.h>
#include <DS1307new.h>
#include <SoftwareSerial.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 13

SoftwareSerial mySerial(0, 1,true); // RX, TX, true for serial out inverted

uint16_t startAddr = 0x0000;            // Start address to store in the NV-RAM
uint16_t lastAddr;                      // new address for storing in NV-RAM
uint16_t TimeIsSet = 0xaa55;            // Helper that time must not set again

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
const byte LCDdelay = 5; //delay between chars because picaxe has problems otherwise
const byte LCDdelayCMD = 35; //delay after main commands
const byte pumpPin = 12; //relay output pin

struct sDT { //this is updated by updateDT() with data from the RTC
	byte hour;
	byte minute;
	byte second;
	byte lastsec;
	String longdate;
	String longtime; // me love you
	byte tempC;
};

struct sTrig {
	byte hour;
	byte minute;
	byte triglen;
}

char keys[ROWS][COLS] = { //keypad layout
	{'1','2','3'},
	{'4','5','6'},
	{'7','8','9'},
	{'*','0','#'}
};
byte rowPins[ROWS] = {6,7,8,9}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {3,4,5}; //connect to the column pinouts of the keypad
sDT DT;
sTrig trig[4];
byte whichtrig;
char key;
byte triggerLen=5; //max 59 (seconds)
byte triggerHour=0;
byte triggerMin=0;
byte triggerOn=0;
byte delaytime; //this is to prevent the rtc from being queried many times per second
float ctemp=10.0;
byte whereX; //hello pascal
byte whereY;
byte lastX; //ooh
byte lastY;
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

OneWire oneWire(ONE_WIRE_BUS);	
DallasTemperature sensors(&oneWire);

void setup() {
	whereX=1;
	whereY=1;
	lastX=1;
	
	lastY=0; //we'll use this one as a temp variable for the moment
	
	for (whichtrig=0; whichtrig==3; whichtrig++) {
		trig[whichtrig].hour=EEPROM.read(lastY);
		trig[whichtrig].minute=EEPROM.read(lastY+1);
		trig[whichtrig].triglen=EEPROM.read(lastY+2);
		lastY+=3
	}
	
	lastY=1;
//	triggerHour=EEPROM.read(0);
//	triggerMin=EEPROM.read(1);
//	triggerLen=EEPROM.read(2);
	
	for (whichtrig=0; whichtrig==3; whichtrig++) {
		if (trig[whichtrig].hour > 23) {
			trig[whichtrig].hour=0;
		}
		if (trig[whichtrig].minute > 59) {
			trig[whichtrig].minute=0;
		}
		if (trig[whichtrig].triglen > 59) {
			trig[whichtrig].triglen=0;
		}	
	}
	
	pinMode(pumpPin, OUTPUT);
	digitalWrite(pumpPin, LOW); //just in  case
	mySerial.begin(2400);
	sensors.begin(); //temp sensor
	RTC.startClock(); //if clock is off
	RTC.ctrl = 0x10;                      // 0x00=disable SQW pin, 0x10=1Hz,
	// 0x11=4096Hz, 0x12=8192Hz, 0x13=32768Hz
	RTC.setCTRL();  //set a nice 1Hz flashing led
	delay(1000); //give LCD a chance to catch up
	printinfo(); //print a welcome msg, LCD also has one in the picaxe eeprom that comes on at startup
	delay(4000);
	clearOLED();
	delaytime=0;
	DT.second=0; //this is because lastsec takes the previous data
	DT.tempC=10;
	updateDT(); //first call to update time from RTC
//	printTrigger(1,4);
}


void updateTrigs() {
	lastY=0; //we'll use this one as a temp variable for the moment
	
	for (whichtrig=0; whichtrig==3; whichtrig++) {
		trig[whichtrig].hour=EEPROM.read(lastY);
		trig[whichtrig].minute=EEPROM.read(lastY+1);
		trig[whichtrig].triglen=EEPROM.read(lastY+2);
		lastY+=3
	}
}
	
	lastY=1;
//	triggerHour=EEPROM.read(0);
//	triggerMin=EEPROM.read(1);
//	triggerLen=EEPROM.read(2);
	
	for (whichtrig=0; whichtrig==3; whichtrig++) {
		if (trig[whichtrig].hour > 23) {
			trig[whichtrig].hour=0;
		}
		if (trig[whichtrig].minute > 59) {
			trig[whichtrig].minute=0;
		}
		if (trig[whichtrig].triglen > 59) {
			trig[whichtrig].triglen=0;
		}	
	}

/**
 *  \brief displays temperature in degC
 *  
 *  \param [in] x X position to display at
 *  \param [in] y Y position to display at
 *  \return nothing
 */
void printTemp(byte x, byte y) {
	sensors.requestTemperatures();
	gotoxy(1,3);
	ctemp=sensors.getTempCByIndex(0);
	DT.tempC=round(ctemp);
	//btemp=int(ctemp);
	serprint(String(ctemp)+="c ");
}

/**
 *  \brief Set the clock
 *  
 *  \param [in] hour - I wonder
 *  \param [in] minute 
 *  \return nothing
 *  \details always sets the seconds to zero
 */
void setRTCtime(byte hour, byte minute) {
	RTC.stopClock();
	RTC.fillByHMS(hour,minute,0);
	RTC.setTime();
	RTC.startClock();
}

/**
 *  \brief Sets the date
 *  
 *  \param [in] day 
 *  \param [in] month 
 *  \param [in] year 
 *  \return nothing
 */
void setRTCdate(byte day, byte month, byte year) {
	RTC.stopClock();
	RTC.fillByYMD(year,month,day);
	RTC.setTime();
	RTC.startClock();
}

/**
 *  \brief clears the LCD/OLED
 *  \return nothing
 *  \details Turns on the display, clears all, turns on the cursor (for now, debug mode) then goes to XY 1,1
 */
void clearOLED() {
	mySerial.write(254);
	delay(LCDdelay);
	mySerial.write(12); //turn on display in case it's off
	delay(LCDdelayCMD);
	mySerial.write(254);
	delay(LCDdelay);
	mySerial.write(1); //clear everything
	delay(LCDdelayCMD);
	cursorOff();
	gotoxy(1,1);
}

void cursorOn(){
	mySerial.write(254);
	delay(LCDdelay);
	mySerial.write(14);  //turn on cursor
	delay(LCDdelayCMD);
}

void cursorOff(){
	mySerial.write(254);
	delay(LCDdelay);
	mySerial.write(13);  //turn off cursor
	delay(LCDdelayCMD);
}


/**
 *  \brief Prints data to the serial port
 *
 *  \param [in] serst Input string to print out
 *  \return nothing
 *
 *  \details The picaxe based OLED/LCD cannot accept chars as fast as the Arduino can output, even at low baud rates, this implements an %LCDdelay delay.
 *  also it stops if it hits a NULL character.
 */
void serprint(String serst) { 
	lastX=whereX;
	for (int x=0; serst.length(); x++) {
		delay(LCDdelay);
		if (serst.charAt(x)==0) {
			break;
		}
		mySerial.print(serst.charAt(x));
		whereX=x;
	}
}

/**
 *  \brief print info
 *  
 *  \return nothing
 *  
 *  \details displays some welcome info on the OLED
 */
void printinfo() {
	clearOLED();
	gotoxy(7,1);
	serprint("Adam's");
	gotoxy(8,2);
	serprint("plant");
	gotoxy(6,3);
	serprint("watering");
	gotoxy(8,4);
	serprint("thing");
}

/**
 *  \brief Go to X,Y location on LCD
 *
 *  \param [in] x the X position starting from 1
 *  \param [in] y the Y position starting from 1
 *  \return nothing
 *
 *  \details Note the line numbers, because the LCD memory stores the lines as 1,3,2,4
 */
void gotoxy(byte x,byte y) {
	lastX=whereX;
	lastY=whereY;
	whereY=y;
	whereX=x;
	switch (y)
	{
		case 1:
			y=127+x;
			break;
		case 2:
			y=191+x;
			break;
		case 3:
			y=147+x;
			break;
		case 4:
			y=211+x;
			break;
		default:
			y=127+x;
			whereY=1;
			break;
	}
	mySerial.write(254);
	delay(LCDdelay);
	mySerial.write(y);
	delay(LCDdelayCMD);
}

/**
 *  \brief Adds a leading zero if required for the purpose of formatting an hour correctly.
 *
 *  \param [in] tx A number 0..59
 *  \return String containing 2 characters ex. 01 or 13
 *
 *  \details Details
 */
String leadingzero(byte tx) {
	String lz="0";
	if (tx < 10) {
		lz+=String(tx);
	} else
	{
		lz=String(tx);
	}
	return lz;
}


/**
 *  \brief Prints the date to the LCD
 *
 *  \return nothing
 *
 *  \details Outputs the date in DD/MM/YY DDD to X,Y
 */
void printdate(byte x, byte y) {
	gotoxy(x,y);
	serprint(DT.longdate);
}

/**
 *  \brief Prints the time to the LCD
 *
 *  \return nothing
 *
 *  \details Outputs the time in HH:MM:SS to X,Y
 */
void printtime(byte x, byte y) {
	gotoxy(x,y);
	serprint(DT.longtime);
}

/**
 *  \brief Updates the DT (sDT) data type with the current date and time
 *  
 *  \return nothing
 *  
 *  \details This is the only place the RTC is polled outside of setup() and setting the time/date.
 */
void updateDT() {
	RTC.getTime();
	DT.hour=RTC.hour;
	DT.minute=RTC.minute;
	DT.lastsec=DT.second;
	DT.second=RTC.second;
	DT.longtime=String(leadingzero(RTC.hour))+=":";
	DT.longtime+=String(leadingzero(RTC.minute))+=":";
	DT.longtime+=String(leadingzero(RTC.second));
	DT.longdate=String(leadingzero(RTC.day))+="/";
	DT.longdate+=String(leadingzero(RTC.month))+="/";
	DT.longdate+=String(RTC.year)+=" ";
	switch (RTC.dow)                      // Friendly printout the weekday
	{
		case 0:
			DT.longdate+="SUN";
			break;
		case 1:
			DT.longdate+="MON";
			break;
		case 2:
			DT.longdate+="TUE";
			break;
		case 3:
			DT.longdate+="WED";
			break;
		case 4:
			DT.longdate+="THU";
			break;
		case 5:
			DT.longdate+="FRI";
			break;
		case 6:
			DT.longdate+="SAT";
			break;
	}
}

/**
 *  \brief function to set time
 *  
 *  \return nothing
 *  
 *  \details gets hour and minute from keypad, displays on screen
 *  to do:
 */
void settime() {
	byte bHour=0;
	byte bMin=0;
	String cH1;
	String cH2;
	String cM1;
	String cM2;
	String cTmp;
	clearOLED;
	serprint("Set Time, 4 digits");
	gotoxy(1,2);
	serprint("now: ");
	printtime(6,2);
	gotoxy(1,4);
	serprint("* to exit  ");
	gotoxy(1,3);
	serprint("Time: 00:00     ");
	cursorOn();
	gotoxy(7,3);  // there was a loop here which took out a large chunk of the code below, but I was having problems with
	key = keypad.waitForKey();  // the string routines so scrubbed it for this quick hack, which got copied to the others.
	cH1=String(key);
	serprint(cH1);
	key = keypad.waitForKey();
	cH2=String(key);
	serprint(cH2+=":");
	cTmp=cH1+=cH2;
	bHour=cTmp.toInt();
	key = keypad.waitForKey();
	cM1=String(key);
	serprint(cM1);
	key = keypad.waitForKey();
	cM2=String(key);
	serprint(cM2);
	cTmp=cM1+=cM2;
	bMin=cTmp.toInt();
	clearOLED();
	gotoxy(1,1);
	serprint("Time>");
	serprint(leadingzero(bHour));
	serprint(":");
	serprint(leadingzero(bMin));
	gotoxy(1,2);
	serprint("OK?");
	gotoxy(1,4);
	serprint("* for yes");
	key=keypad.waitForKey();
	if (key=='*') {
		setRTCtime(bHour,bMin);	
		clearOLED();
	} else {
		clearOLED();
		serprint("Cancelled");
		delay(3000);	
	}
}

/**
 *  \brief Gets the date from user
 *  
 *  \return nothing
 *  
 *  \details saves the date to the RTC
 */
void setdate() {
	byte bDay=1;
	byte bMonth=1;
	int bYear=2014;
	String cX1;
	String cX2;
	String cTmp;
	clearOLED;
	serprint("Set Date, 6 digits");
	gotoxy(1,2);
	serprint("now: ");
	printdate(6,2);
	gotoxy(1,4);
	serprint("* to exit  ");
	gotoxy(1,3);
	serprint("date: 01/01/14 ");
	cursorOn();
	gotoxy(7,3);
	key = keypad.waitForKey();
	cX1=String(key);
	serprint(cX1);
	key = keypad.waitForKey();
	cX2=String(key);
	serprint(cX2+="/");
	cTmp=cX1+=cX2;
	bDay=cTmp.toInt();
	key = keypad.waitForKey();
	cX1=String(key);
	serprint(cX1);
	key = keypad.waitForKey();
	cX2=String(key);
	serprint(cX2+="/");
	cTmp=cX1+=cX2;
	bMonth=cTmp.toInt();
	key = keypad.waitForKey();
	cX1=String(key);
	serprint(cX1);
	key = keypad.waitForKey();
	cX2=String(key);
	serprint(cX2);
	cTmp="20";
	cTmp+=cX1+=cX2;
	bYear=cTmp.toInt();
	clearOLED();
	serprint("Date>");
	serprint(leadingzero(bDay));
	serprint("/");
	serprint(leadingzero(bMonth));
	serprint("/");
	serprint(leadingzero(bYear));
	gotoxy(1,2);
	serprint("OK?");
	gotoxy(1,4);
	serprint("* for yes");
	key=keypad.waitForKey();
	if (key=='*') {
		setRTCdate(bDay,bMonth,bYear);	
		clearOLED();
	} else {
		clearOLED();
		serprint("Cancelled");
		delay(3000);	
	}
}

void setTriggerTime(byte xx, byte hour, byte minute){
	switch (xx) { //this could be tidied into a loop
		case 0:
			EEPROM.write(0, hour);
			EEPROM.write(1, minute);
			break;
		case 1:
			EEPROM.write(3, hour);
			EEPROM.write(4, minute);
			break;
		case 2:
			EEPROM.write(6, hour);
			EEPROM.write(7, minute);
			break;
		case 3:
			EEPROM.write(9, hour);
			EEPROM.write(10, minute);
			break;
	}
}

/**
 *  \brief get trigger time from user
 *  
 *  \return nothing
 *  
 *  \details get the time of the trigger from the user and pass to setTriggerTime()
 */
void settrigger(byte xx) {
	whichtrig=xx;
	byte bHour=0;
	byte bMin=0;
	String cH1;
	String cH2;
	String cM1;
	String cM2;
	String cTmp;
	clearOLED;
	gotoxy(1,1);
	serprint("Set Trigger, 4 digit");
	gotoxy(1,2);
	serprint("now:                ");
	printtime(6,2);
	printTrigger(whichtrig,1,4);
	gotoxy(1,3);
	serprint("Time: 00:00         ");
	cursorOn();
	gotoxy(7,3);
	key = keypad.waitForKey(); //from here on this function is a mess
	cH1=String(key);
	serprint(cH1);
	key = keypad.waitForKey();
	cH2=String(key);
	serprint(cH2+=":");
	cTmp=cH1+=cH2;
	bHour=cTmp.toInt();
	key = keypad.waitForKey();
	cM1=String(key);
	serprint(cM1);
	key = keypad.waitForKey();
	cM2=String(key);
	serprint(cM2);
	cTmp=cM1+=cM2;
	bMin=cTmp.toInt();
	clearOLED();
	gotoxy(1,1);
	serprint("Time>");
	serprint(String(bHour));
	serprint(":");
	serprint(String(bMin));
	gotoxy(1,2);
	serprint("OK?");
	gotoxy(1,4);
	serprint("* for yes");
	key=keypad.waitForKey();
	if (key=='*') {
		setTriggerTime(whichtrig,bHour,bMin);	
		updateTrigs();
		clearOLED();
		settriggerLen();
	} else {
		clearOLED();
		serprint("Cancelled");
		delay(3000);	
	}
}

/**
 *  \brief Trigger length
 *  
 *  \return nothing
 *  
 *  \details gets the length of time the relay should operate from the user, then saves that to EEPROM
 */
void settriggerLen(byte xx) {
	whichtrig=xx;
	byte b1=0;
	String cH1;
	String cH2;
	String cTmp;
	clearOLED;
	gotoxy(1,1);
	serprint("Trig length 2 digits");
	gotoxy(1,2);
	serprint("now: ");
	cH1=trig[whichtrig].triglen;
	//cH1=String(EEPROM.read(2));
	serprint(cH1+=" secs         ");
	gotoxy(1,3);
	serprint("secs: 00            ");
	cursorOn();
	gotoxy(7,3);
	key = keypad.waitForKey();
	cH1=String(key);
	serprint(cH1);
	key = keypad.waitForKey();
	cH2=String(key);
	serprint(cH2);
	cTmp=cH1+=cH2;
	b1=cTmp.toInt();
	cursorOff();
	clearOLED();
	gotoxy(1,1);
	serprint("Trig length>");
	if (b1 > 59) {
		b1=01;
	}
	serprint(String(b1));
	gotoxy(1,2);
	serprint("OK?");
	gotoxy(1,4);
	serprint("* for yes");
	key=keypad.waitForKey();
	if (key=='*') {
		switch (whichtrig) {
			case 0:
				EEPROM.write(2, b1);
				break;
			case 1:
				EEPROM.write(5, b1);
				break;
			case 2:
				EEPROM.write(8, b1);
				break;
			case 3:
				EEPROM.write(11, b1);
				break;
		}
		updateTrigs();
//		triggerLen=b1;
		clearOLED();
	} else {
		clearOLED();
		serprint("Cancelled");
		delay(3000);	
	}
}

/**
 *  \brief Display trigger time on screen
 *  
 *  \param [in] x position to print to
 *  \param [in] y 
 *  \return nothing
 */
void printTrigger(byte xx, byte x, byte y) {
	String tmp;
	tmp=leadingzero(trig[xx].hour)+":"+leadingzero(trig[xx].minute)+" - "+String(trig[xx].triglen)+"s ";
	gotoxy(x,y);
	serprint(tmp);
}

/**
 *  \brief main program loop
 */
void loop() {
	key = keypad.getKey();
	if (key) {
		switch (key) {
		case '1':
			settrigger(0);
			clearOLED();
			break;
		case '2':
			settrigger(1);
			clearOLED();
			break;
		case '3':
			settrigger(2);
			clearOLED();
			break;
		case '4':
			settrigger(3);
			clearOLED();
			break;
		case '*':
			updateDT();
			settime();
			clearOLED();
			clearOLED();
			clearOLED();
			break;
		case '#':
			updateDT();
			setdate();
			clearOLED();
			break;
		default: //just a debug
			clearOLED();
			serprint(">");
			serprint(String(key));
			delay(1000);
			clearOLED();
			break;
		} //switch
	} //if key
	delaytime++;
	if (delaytime==30) {  //every 30 cycles of the loop it enters this code block
		delaytime=0;
		updateDT(); //update from the RTC
		if (triggerOn==1) {
			triggerOn=0;
			clearOLED();
		}
		if (triggerOn==0) { // I did have this in a long line of && but seperated it for easier reading
			if (DT.hour==triggerHour) {
				if (DT.minute==triggerMin) {
					if (DT.second <= triggerLen) {
						if (DT.second >= 0) {
							if (DT.tempC >= 4) {
								triggerOn=1;
								gotoxy(8,3);
								serprint("Pump ON     ");
								delay(200);
								digitalWrite(pumpPin, HIGH);
								//this next section was seperate but I was having tripping out problems on the relay
								delay(triggerLen*1000); //this is obviously blocking, but waiting on the clock caused problems
								digitalWrite(pumpPin, LOW);
								delay(1000);
								gotoxy(13,3);
								serprint("Off");
							} else {
								gotoxy(8,3);
								serprint("ICE warning");
							}
						}
					}
				}
			}
		}
		if ((DT.second==0) || (DT.second==30)) {
			printTemp(1,3);
			printTrigger(1,4);
		}
		if (DT.lastsec != DT.second) { //check if this second is different from the last unique one, if so print to OLED
			printtime(1,1); //this no longer updates while pumping because it uses delay() instead of counting the seconds
			printdate(1,2);
		}
	} //delaytime==30
}
