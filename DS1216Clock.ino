// File:      DS1216Clock.ino
// Created:   2019 Jan 19
// By:        Brian Mathewson
//
// Purpose:   Dallas (now Maxim) Real time clock chip demonstration program.
//
// Setup:     Arduino connected per PINOUTS below to DS1216
// 
// Inputs:    SETDATETIME: 0 = only read date/time.
//                         1 = write date/time when Arduino starts running.
//            SET_YEAR, SET_MONTH... when SETDATETIME is 1
//
// Outputs:   Serial Monitor displays date and time as read from DS1216.
//
// Note:      To set the time you configure a date/time a minute or so in the future
//            then verify and upload the program. Then immediately change the program
//            back to only read, not set the time. You can easily modify this program 
//            to use digital inputs or other means to set the date and time.

// =================================
// ##### TO READ DATE AND TIME #####
// =================================
// Set SETDATETIME (below) to 0.
// Verify (compile) program.
// Upload to Arduino.
// On the Serial Monitor, observe the reported date and time from the DS1216.
//
// ================================
// ##### TO SET DATE AND TIME #####
// ================================
// Change SETDATETIME (below) to 1.
// Modify SET_YEAR .. SET_SECOND values for a moment shortly in the future.
// Verify (compile) program.
// A few seconds before the moment arrives, upload to Arduino.
// The Arduino will set the date and time in the DS1216.
// On the Serial Monitor, observe the reported date and time from the DS1216.
// If the date/time is acceptable, continue, otherwise update SET_MINUTE etc.
// Change SETDATETIME to 0 (so the next time program runs it doesn't set again).
// Verify (compile) program.
// Upload to Arduino.
// On the Serial Monitor, observe the reported date and time from the DS1216.

// ====================
// Set SETDATETIME to 1 to program the date and time
// Set SETDATETIME to 0 and recompile the program to prevent further changes.
#define SETDATETIME 0

// If SETDATETIME = 1, then the following will be used to initialize
//   the DS1215 clock.
#define SET_YEAR    2019
#define SET_MONTH   02
#define SET_DATE    03
#define SET_DOW     7
#define SET_HOUR    21
#define SET_MINUTE  27
#define SET_SECOND  00


// PINOUTS
//
// DS1216C   Arduino    Function
// Pin       Connection
// -----     -----       -----
//   1        NC         /RST     Not used by this program
//   8        NC         A2       Not used by this program
//   10       NC         A0       Not used by this program
//   11       D4         DQ0      Bidirectional data bit for all data transfers
//   14       GND        GND
//   20       D7         /CE      Chip Enable, '/' means active-low
//   22       D6         /OE      Output Enable (read), '/' means active-low
//   26       NC         VccB     Battery voltage. No connection required.
//   27       D5         /WE      Write Enable, '/' means active-low
//   28       +5V        Vcc
//
//            NC = No Connection

const int dsDQ0pin = 4;   // Data bit, bidirectional
const int ds_WEpin = 5;   // Write Enable, Arduino output to DS1216 input
const int ds_OEpin = 6;   // Output Enable, Arduino output to DS1216 input
const int ds_CEpin = 7;   // Chip Enable, Arduino output to DS1216 input
const int ds_RSTpin = 8;  // NOT USED by this program
const int dsA2pin = 9;    // NOT USED by this program
const int dsA0pin = 10;   // NOT USED by this program

// Index to 1-byte registers on DS1216. See datasheet.
const int dsSubSecondRegister = 0;
const int dsSecondsRegister = 1;
const int dsMinutesRegister = 2;
const int dsHourRegister = 3;
const int dsDayRegister = 4;
const int dsDateRegister = 5;
const int dsMonthRegister = 6;
const int dsYearRegister = 7;

// Important control bits in registers. See DS1216 datasheet.
const int ds_OSCbit = 0x20;   // in dsDayRegister. Set to 0 to enable oscillator.
const int ds1224bit = 0x80;   // in dsHourRegister
const int dsAMPMbit = 0x20;   // in dsHourRegister

// Forward references
void dsWriteRegisters( byte year, byte month, byte day, byte dow, byte hours, byte minutes, byte seconds );


// ====================
// SUPPORTING FUNCTIONS
// ====================

// BCD (Binary Coded Decimal) CONVERSION FUNCTIONS
byte decimal2BCD(byte dec)
{
  return (dec/10)*16 + (dec%10);
}

byte BCD2Decimal(byte bcd)
{
  return (bcd/16) * 10 + bcd%16;
}

// Note: str must be at least length 3
char* BCD2ASCII(char *str, byte bcd)
{
  str[0] = bcd/16 + '0';
  str[1] = bcd%16 + '0';
  str[2] = '\0';
  return str;
}


// Simple helper function to make code more readable.
// Inputs:
//   enableChip = 0 or false =>  Disable the Chip Enable (CE) pin to the DS1216.
//   enableChip = 1 or true  =>  Enable the Chip Enable (CE) pin to the DS1216.
// Ouptuts: 
//   none
// Note: Chip enable can remain on (1) during multiple bit read/write operations,
//   or it can be turned on/off for each bit read/write. This program
//   sets/resets CE once for each byte read/write operation.
//   
void dsChipEnable( bool enableChip )
{
  if( enableChip )
    digitalWrite( ds_CEpin, LOW );  // Enable chip
  else
    digitalWrite( ds_CEpin, HIGH );  // Disable chip
}


// Write a bit to DS1216 device.
//   NOTE: You must assert chip enable before calling this function.
void dsWriteBit( bool bitLevel )
{
  digitalWrite( dsDQ0pin, bitLevel );
  digitalWrite( ds_WEpin, LOW );  // Write operation starts
  digitalWrite( ds_WEpin, HIGH ); // LOW is Write Enable
}

// Send one byte to DS1216, one bit at a time on the Arduino output to DS1216 DQ0 pin.
// Inputs:  n
// Outputs: none
void dsSendByte( byte n )
{
  dsChipEnable( 1 );
  pinMode( dsDQ0pin, OUTPUT );    // DQ0 is a bidirectional data signal

  dsWriteBit( n & 0x01 );
  dsWriteBit( n & 0x02 );
  dsWriteBit( n & 0x04 );
  dsWriteBit( n & 0x08 );

  dsWriteBit( n & 0x10 );
  dsWriteBit( n & 0x20 );
  dsWriteBit( n & 0x40 );
  dsWriteBit( n & 0x80 );
  
  dsChipEnable( 0 );
}


// Read a bit from device, returns 0 or 0.
//   NOTE: The DQ0 bit must be configured as INPUT prior to this call.
//   NOTE: You must assert chip enable before calling this function.
byte dsReadBit()
{
  byte b;
  digitalWrite( ds_OEpin, LOW );  // READ operation starts
  b = digitalRead( dsDQ0pin );
  digitalWrite( ds_OEpin, HIGH ); // End of READ

  return b;
}


// Read one byte from the DS1216, one bit at a time.
//  This operation must be preceded by the dsSendCodes(); function.
byte dsReadByte()
{
  byte b;

  pinMode( dsDQ0pin, INPUT );     // DQ0 is a bidirectional data signal
  dsChipEnable( 1 );

  b = dsReadBit();
  b |= dsReadBit() << 1;
  b |= dsReadBit() << 2;
  b |= dsReadBit() << 3;

  b |= dsReadBit() << 4;
  b |= dsReadBit() << 5;
  b |= dsReadBit() << 6;
  b |= dsReadBit() << 7;

  dsChipEnable( 0 );

  return b;
}


// Read all DS1216 registers into the dsRegister array.
// Inputs:  dsRegister[8]
// Outputs: Updated values in dsRegisters[8]
// Note: This operation must be preceded by the dsSendCodes(); function.
void dsReadRegisters( byte dsRegister[] )
{
  dsRegister[0] = dsReadByte();
  dsRegister[1] = dsReadByte();
  dsRegister[2] = dsReadByte();
  dsRegister[3] = dsReadByte();

  dsRegister[4] = dsReadByte();
  dsRegister[5] = dsReadByte();
  dsRegister[6] = dsReadByte();
  dsRegister[7] = dsReadByte();
}


// Write to the Serial Monitor text showing the date from the dsRegister array.
//   NOTE: See DS1216 datasheet for dsRegister descriptions, which uses BCD format.
void dsSerialPrintDate( byte dsRegister[] )
{
  char str[4];
  
  // Print in ISO standard date order: YYYY-MM-DD
  Serial.print("20");
  Serial.print( BCD2ASCII( str, dsRegister[dsYearRegister] ));
  Serial.print('-');
  Serial.print( BCD2ASCII( str, dsRegister[dsMonthRegister] ));
  Serial.print('-');
  Serial.print( BCD2ASCII( str, dsRegister[dsDateRegister] ));
}


// Write to the Serial Monitor text showing the time from the dsRegister array.
//   NOTE: See DS1216 datsheet for dsRegister descriptions, which uses BCD format.
void dsSerialPrintTime( byte dsRegister[] )
{
  char str[4];

  Serial.print( BCD2ASCII( str, dsRegister[dsHourRegister] ));      // Hours
  Serial.print(":");
  Serial.print( BCD2ASCII( str, dsRegister[dsMinutesRegister] ));   // Minutes
  Serial.print(":");
  Serial.print( BCD2ASCII( str, dsRegister[dsSecondsRegister] ));   // Seconds
  Serial.print(".");
  Serial.print( BCD2ASCII( str, dsRegister[dsSubSecondRegister] )); // 0.xx seconds
}

// Write to the Serial Monitor text showing the full date/time from the dsRegister array.
//   NOTE: See DS1216 datsheet for dsRegister descriptions, which uses BCD format.
void dsSerialPrintRegisters( byte dsRegister[] )
{
//  String dayOfWeekString = "MonTueWedThuFriSatSun";
  const char *dayStrings[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
  int dayIndex = dsRegister[dsDayRegister] & 0x0f;
  Serial.print( dayStrings[dayIndex] );
  Serial.print( ' ' );
  dsSerialPrintDate( dsRegister );
  Serial.print( ' ' );
  dsSerialPrintTime( dsRegister );
  Serial.println();
}


// Sends a 64-bit code to DS1216 that wakes it up 
// and makes it ready to receive data write or read commands.
// See the DS1216 datsheet for full description.
void dsSendCodes( )
{
  byte temp;

  // Must do 1 read first to reset sequence
  dsChipEnable( 1 );
  pinMode( dsDQ0pin, INPUT );
  temp = dsReadBit(); // Don't care what we read
  dsChipEnable( 0 );

  // Write 64 bit (8 byte) code. Codes are straight from the DS1216 Datasheet.
  dsSendByte( 0xC5 );
  dsSendByte( 0x3A );
  dsSendByte( 0xA3 );
  dsSendByte( 0x5C );

  dsSendByte( 0xC5 );
  dsSendByte( 0x3A );
  dsSendByte( 0xA3 );
  dsSendByte( 0x5C );
}


//
// This writes all the DS1216 registers as defined in the datasheet.
// This sets the date and time. 
// You don't have to set all the registers like this at once,
//   you could create a set-time function only for example.
//
// Inputs:
//  year:  00 - 99 (last 2 digits)
//  month: 1 - 12
//  day:   1 - 31
//  dow:   1 - 7  (day of week)
//  hours: 1 - 12 for AM/PM mode or 0 - 23 for 24-hour time
//  minutes: 0 - 59
//  seconds: 0 - 59
// Outputs:
//  None. Verify the time was set by reading the registers.
// 
void dsWriteRegisters( int year, byte month, byte day, byte dow, 
  byte hours, byte minutes, byte seconds )
{
  byte reg[8], temp;

  reg[dsSubSecondRegister] = 0;
  reg[dsSecondsRegister] = decimal2BCD( seconds );   // Seconds
  reg[dsMinutesRegister] = decimal2BCD( minutes );
  reg[dsHourRegister]    = decimal2BCD( hours );  // Add "| ds1224bit" for 12 hour format

  reg[dsDayRegister]   = decimal2BCD( dow );   // Note: Set OSC bit to 0 to enable oscillator
  reg[dsDateRegister]  = decimal2BCD( day );
  reg[dsMonthRegister] = decimal2BCD( month );
  reg[dsYearRegister]  = decimal2BCD( year % 100 );

  // Write registers
  dsSendByte( reg[0] );
  dsSendByte( reg[1] );
  dsSendByte( reg[2] );
  dsSendByte( reg[3] );

  dsSendByte( reg[4] );
  dsSendByte( reg[5] );
  dsSendByte( reg[6] );
  dsSendByte( reg[7] );
}



void setup() {

  // Configure 3 Ardunio digital channels as outputs for DS1216 control signal inputs
  pinMode( ds_CEpin, OUTPUT );
  pinMode( ds_OEpin, OUTPUT );
  pinMode( ds_WEpin, OUTPUT );
  // Initialize outputs, setting CE high to deselect device
  digitalWrite( ds_CEpin, HIGH );
  digitalWrite( ds_OEpin, HIGH );
  digitalWrite( ds_WEpin, HIGH );

  // DQ0 is bidirectional but initially set as output
  pinMode( dsDQ0pin, OUTPUT );

  // Configure others as outputs
//  pinMode( ds_RSTpin, OUTPUT ); // NOT USED
//  pinMode( dsA2pin, OUTPUT );   // NOT USED
//  pinMode( dsA0pin, OUTPUT );   // NOT USED

  // Set up serial port for serial monitor
  Serial.begin(9600);
  delay(100);
  Serial.println("Starting DS1216.");

  // **************************************************
  // Initialize date.
  // NOTE: Do this ONCE to set the time,
  //       then comment it out, since most of the time
  //       you will just read the date or time.
  // **************************************************
#if SETDATETIME
  dsSendCodes();
  dsWriteRegisters( SET_YEAR, SET_MONTH, SET_DATE, SET_DOW, SET_HOUR, SET_MINUITE, SET_SECOND );
  // dsWriteRegisters( 2019,  1, 19,   5, 20,  21,  20 );
  //           Format :yyyy, mm, dd, dow, hh, min, sec )
  // ***************************************************
#endif

}


// Sends wake-up code to DS1216, reads its 8 registers, then prints date/time from registers.
void dsReadAndPrintTime()
{
  byte dsRegisters[8];
  
  dsSendCodes();
  dsReadRegisters( dsRegisters );
  dsSerialPrintRegisters( dsRegisters );
}


// Run repeatedly
void loop() {

  dsReadAndPrintTime();

  delay(300);
}

// End of DS1216Clock.ino
