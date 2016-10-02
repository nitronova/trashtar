/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/* The original source code for this was developed by the awesome Dean Miller,
   who has given permissions for this project! Thanks, Dean!
   https://github.com/deanm1278/arduinoRibbonController/issues/1 */

/*Functions used: *Note: Functions from header files are explained under their headers.
 * analogRead(pin)                  Returns short value from an analog (#) input (0 - 1023).     https://www.arduino.cc/en/Reference/AnalogRead
 * pinMode(pin,mode)                Sets a digital (#)/analog (A#) pin to INPUT/OUTPUT (3.3V).   https://www.arduino.cc/en/Reference/PinMode
 * digitalRead(pin)                 Returns int HIGH (>2 volts) or LOW because 3.3V board.       https://www.arduino.cc/en/Reference/Constants
 * delay(#)                         Wastes clock cycles.*/
#include <EEPROM.h>/* Functions:
* EEPROM.h - Allows us to read and write the chip's EEPROM.
* Def. - EEPROM is a type of memory (temporary and/or not-so-temporary remembering spaces, like RAM) that retains even without power!
*  EEPROM.read(address)             Returns an address' byte (default 255).                      https://www.arduino.cc/en/Reference/EEPROMRead
*  EEPROM.write(address,value)      Writes a value to a byte. 100k writes/erases kills it.       https://www.arduino.cc/en/Reference/EEPROMWrite   (Don't use this)
*  EEPROM.update(address,value)     Write IF the info is new to the byte. Use this instead!      https://www.arduino.cc/en/Reference/EEPROMUpdate  (Use this instead)
*  EEPROM.get(address,data)         Gets from address and puts into data (see example).          https://www.arduino.cc/en/Reference/EEPROMGet (Use these if not working with ints)
*  EEPROM.put(address,data)         Puts the data into address (see example).                    https://www.arduino.cc/en/Reference/EEPROMPut
* EEPROM[address] or EEPROM.length  Array style access. Honestly? Avoid using this.              https://www.arduino.cc/en/Reference/EEPROMObject  (Don't use this incorrectly)*/
#include <Wire.h>/* Functions:
* Serial.begin(baud)              Sets the baud rate for data transfer (over USB in this case) https://www.arduino.cc/en/Serial/Begin
* Serial.write(info)              Send info (byte, strings, buffers+lenghts, etc) over USB.    https://www.arduino.cc/en/Serial/Write */
#ifdef __AVR__
 #include <avr/power.h>
#endif
/* Let's define our input pins. */
#define B0            10 // My simple strum button is connected to digital pin # 10.
#define S0            0  // Analog strings. The first string shall be "string #0."
#define S1            1
#define S2            2  // You'll need to continue adding analog inputs like this.
/* Defined properties of the strings */
#define N_SOFTPOTS    3  // Number of "softpot" potentiometers. Yes, these are our strings.
#define N_FRETS       20 // Number of frets per string.
#define S_PAD         3  // How much "leeway" should we give a string value, I guess? Avoids way-too-precise note tracking, I think.
/* Values involving our now-known inputs. */
int   S_Pins[] = {S0,S1,S2};             // Quick access to individual strings.
int   S_Active[N_SOFTPOTS];              // Determine which strings are active.
int   S_Values[N_SOFTPOTS];              // The analogRead value for each string.
int   S_ValuesPrev[N_SOFTPOTS];          // The previous values for each string.
int   S_FretTouched[N_SOFTPOTS];         // If the string is touched, which "fret" does it value belong to?
short S_FretSeps[N_SOFTPOTS][N_FRETS];   // Define where on the softpot each fret begins.
int   S_FretOffsets[] = {40, 45, 50};    // These offsets are how we get our guitar string starting notes: S[0] is E, 1 is A, 2 is D.
/* Values involving our inputs - the strum button */
bool  B_State = false;                   // Button pressed or released.

/* Pretty much all Arduino projects will have these two functions. */
void setup()
{
 Serial.begin(31250);       // First, set the USB data transfer rate in bits per second.
 pinMode( B0, INPUT );      // Set pin #10 to input mode. This button is an input.
 for(int i = 0; i < N_SOFTPOTS; i++ )
 {
  for( int j = 0; j < N_FRETS; j++ )
  {
   // 0   1   2   3    4    5    6    7    8    9   10   11   12   13   14   15   16   17   18   19 ... "Fret #" on the "fretboard"
   // 0, 29, 58, 87, 120, 155, 192, 231, 272, 317, 363, 413, 466, 519, 580, 643, 710, 780, 855, 935 ... (1023 is the maximum softpot input value)
   // We bumped these numbers around and came up with an approximate equation: 1.36x^2 + 23x
   S_FretSeps[i][j] = (short)( ( 23 * j ) + ( j * j * 1.36 ) );
  }
  pinMode(S_Pins[i], INPUT); // Set all of our analog pins (strings) to input.
 }
}
void loop()                  // Integrated circuits like microcontroller devices often operate in infinite loops until power off or disconnect.
{ 
 scrGetInputs();             // void | Get info fom potrentiometers and buttons.
 scrGetFrets();              // void | Figure out which fret we are holding.
 scrCalcLegato();            // void | Test if legato is going on.
 scrCalcNotes();             // void | Calculate the note/key we are pressing.
 scrCleanUp();
 delay(5);                   // Waste the clock cycles!
}

/* Support functions are listed below. */

// Loop through our strings and get their inputs.
void scrGetInputs()
{
 B_State = digitalRead(B0); // Get button press.
 for( int i = 0; i < N_SOFTPOTS; i++ )
 {
  S_Values[i] = analogRead(S_Pins[i]);
 }
}

// Figure out what frets are being held.
void scrGetFrets()
{
 for( int i = 0; i< N_SOFTPOTS; i++ )
 {
  short s_val = S_Values[i];         // Because I'm gonna use this so much.
  if( s_val == 0 )
  {                                  // IF we have let go of the string.
   S_ValuesPrev[i] = s_val;
   S_FretTouched[i] = 0;
  }
  else if( s_val >= S_FretSeps[i][0] && abs( (int)s_val - (int)S_ValuesPrev[i] ) > S_PAD )
  {                                  // IF we are beyond the first separation && we haven't moved our finger that much on the string.
   S_ValuesPrev[i] = s_val;
   S_FretTouched[i] = 1;
  }
  else                               // Either we aren't beyond the first separation OR we've significantly moved our finger placement on the string.
  {
   for( int j = 1; j < N_FRETS; j++ )// ... So that means we need to go through the string's frets and assign new values to the variables.
   {
    if( s_val >= S_FretSeps[i][j] && s_val < S_FretSeps[i][ j - 1 ] && abs( (int)s_val - (int)S_ValuesPrev[i] ) > S_PAD )
    {                                // IF finger is on the current fret AND less than the next fret (j-1) AND not moving much.
     S_ValuesPrev[i] = s_val;
     S_FretTouched[i] = j + 1;
    }
   }
  }
 }
}

// Testing to see if the notes are doing a legato thing.
void scrCalcLegato()
{
 for( int i = 0; i < N_SOFTPOTS; i++ )
 {
  if(S_Active[i])                                   // If a string is currently active.
  {
   int note = S_FretTouched[i] + S_FretOffsets[i];  // Take its fret, add the offset value.
   if( note != S_Active[i] && S_FretTouched[i] )    // ( S_FretTouched[i] || T_active[i] ) Idk what it was about.
   {
    scrNoteSend( 0x90, note, 100 );                 // Write these as bytes to the serial port (this function will be found at the bottom).
    scrNoteSend( 0x80, S_Active[i], 0 );            // 0x90 is MIDI for note ON, 0x80 is for note OFF.
    S_Active[i] = note;
   }
  }
 }
}

// Calculate which note we are holding down.
void scrCalcNotes()
{
 for(int i = 0; i < N_SOFTPOTS; i++ )
 {
  if(B_State)                                        // If we are pressing the strum button...
  {
   if(S_Active[i])
   {
    scrNoteSend( 0x80, S_Active[i], 0 );             // Disable/Clear the current active note.
   }
   S_Active[i] = S_FretTouched[i] + S_FretOffsets[i];
   scrNoteSend( 0x90, S_Active[i], 100 );            // Send the note that we are pressing.
  }
 }
}

// After we have sent our info, we will "clean up."
void scrCleanUp()
{
 for (int i = 0; i < N_SOFTPOTS; i++ )
 {
  if( S_Active[i] && !S_FretTouched[i] && !B_State)
  {
   scrNoteSend( 0x80, S_Active[i], 0 );
   S_Active[i] = 0;
  }
 }
}

/* Serial writing functions */

// Send information (usually note information) to the USB port.
void scrNoteSend(int cmd, int pitch, int velocity)
{  
 Serial.write(byte(cmd));
 Serial.write(byte(pitch));
 Serial.write(byte(velocity));
}
