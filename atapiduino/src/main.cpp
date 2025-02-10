/* ########################################################################################

    Simple IDE ATAPI controller using the Arduino I2C interface and 
    3 PCF8574 I/O expanders. Release 3.1 (adapted for Arduino 1.0)
	
    Copyright (C) 2012  Carlos Durandal
	
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

	
 ##########################################################################################
 
    PCF8574   A2,A1,A0     Addr.
        #1     0  0  0     0x20    interfaces to IDE DD0-DD7
        #2     0  0  1     0x21    interfaces to IDE DD8-DD15
        #3     0  1  0     0x22    interfaces to the IDE register selection see details below.

    PCF8574#3 bit:    7     6    5    4    3   2   1   0
    IDE pin:        nDIOR nDIOW nRST nCS1 nCS0 DA2 DA1 DA0
 
     pin names with leading 'n' are active LOW

 ##########################################################################################
*/

#include <Wire.h>  // I2C bus library
#include "main.h"

// Start of Definitions
// ####################

// I/O expander addresses:
const int DataL = 0x20;   // IDE DD0-DD7
const int DataH = 0x21;   // IDE DD8-DD15
const int RegSel = 0x22;  // IDE register

// IDE Register addresses
const byte DataReg = 0xF0;  // Addr. Data register of IDE device.
const byte ErrFReg = 0xF1;  // Addr. Error/Feature (rd/wr) register of IDE device.
const byte SecCReg = 0xF2;  // Addr. Sector Count register of IDE device.
const byte SecNReg = 0xF3;  // Addr. Sector Number register of IDE device.
const byte CylLReg = 0xF4;  // Addr. Cylinder Low register of IDE device.
const byte CylHReg = 0xF5;  // Addr. Cylinder High register of IDE device.
const byte HeadReg = 0xF6;  // Addr. Device/Head register of IDE device.
const byte ComSReg = 0xF7;  // Addr. Command/Status (wr/rd) register of IDE device.
const byte AStCReg = 0xEE;  // Addr. Alternate Status/Device Control (rd/wr) register of IDE device.

// Program Variables
byte dataLval;     // dataLval and dataHval hold data from/to
byte dataHval;     // D0-D15 of IDE
byte regval;       // regval holds addr. of reg. to be addressed on IDE
byte reg;          // Holds the addr. of the IDE register with adapted
                   // nDIOR/nDIOW/nRST values to suit purpose.
byte cnt;          // packet byte counter
byte idx;          // index used as pointer within packet array
byte paclen = 12;  // Default packet length
byte s_trck;       // Holds start track
byte e_trck;       // Holds end track
byte c_trck;       // Follows current track while reading TOC
byte c_trck_m;     // MSF values for current track
byte c_trck_s;
byte c_trck_f;
byte a_trck = 1;  // Holds actual track from reading subchannel data
byte MFS_M;       // Holds actual M value from reading subchannel data
byte MFS_S;       // Holds actual S value from reading subchannel data
byte d_trck;      // Destination track
byte d_trck_m;    // MSF values for destination track
byte d_trck_s;
byte d_trck_f;
byte aud_stat = 0xFF;  // subchannel data: 0x11=play, 0x12=pause, 0x15=stop
byte asc;
unsigned long prev_millis = 0;
unsigned long interval = 100;
boolean toc;

// Array containing sets of 16 byte packets corresponding to part of the CD-ROM
// ATAPI function set. If the IDE device only supports packets with 12 byte length
// the last 4 bytes are not sent. The great majority of tested devices use 12 byte.

byte fnc[] = {
  0x1B, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=0 Open tray
  0x1B, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=16 Close tray
  0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=32 Stop unit
  0x47, 0x00, 0x00, 0x10, 0x28, 0x05, 0x4C, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=48 Start PLAY
  0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=64 PAUSE play
  0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=80 RESUME play
  0x43, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=96 Read TOC
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=112 unit ready
  0x5A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=128 mode sense
  0x42, 0x02, 0x40, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=144 rd subch.
  0x03, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // idx=160 req. sense
  0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // idx=176 Stop disk
};

// Arduino pin assignments:
const byte LED = 13;
const byte NEXT = 12;  // NEXT button
const byte EJCT = 11;  // EJECT button, open/close CD-ROM tray
const byte STOP = 10;  // STOP button
const byte PLAY = 9;   // PLAY button
const byte PREV = 8;   // PREV button

// End of Definitions ########################################################################

void setup() {

  // start I2C interface as Master
  Wire.begin();

  // Start Serial Interface
  Serial.begin(9600);  // init LCD interface

  // Set all pins of all PCF8574 to high impedance inputs.
  highZ();

  // initialize the push button pins as inputs with pullup:
  pinMode(NEXT, INPUT);
  pinMode(PREV, INPUT);
  pinMode(EJCT, INPUT);
  pinMode(STOP, INPUT);
  pinMode(PLAY, INPUT);
  pinMode(LED, OUTPUT);

  digitalWrite((byte)NEXT, HIGH);
  digitalWrite((byte)PREV, HIGH);
  digitalWrite((byte)EJCT, HIGH);
  digitalWrite((byte)STOP, HIGH);
  digitalWrite((byte)PLAY, HIGH);
  digitalWrite((byte)LED, LOW);

  // IDE Initialisation Part
  // ########################

  Serial.println("Atapiduino");
  Serial.println("Release 3.1");

  reset_IDE();       
  delay(5000);       // Increased to 5 seconds for slower drives
  BSY_clear_wait();  
  DRY_set_wait();    

  readIDE(CylLReg);  // Check device signature for ATAPI capability
  if (dataLval == 0x14 || dataLval == 0x69) {  // Added alternative signature
    readIDE(CylHReg);
    if (dataLval == 0xEB || dataLval == 0x96) { // Added alternative signature
      Serial.println("Found ATAPI Dev.");
    } else {
      Serial.println("Invalid ATAPI signature high byte");
      while(1);
    }
  } else {
    Serial.println("Invalid ATAPI signature low byte");
    while(1);
  }
  writeIDE(HeadReg, 0x00, 0xFF);  // Set Device to Master (Device 0)

  // Initialise task file
  // ####################
  init_task_file();

  // Run Self Diagnostic
  // ###################
  delay(3000);

  Serial.println("Self Diag. ");
  writeIDE(ComSReg, 0x90, 0xFF);  // Issue Run Self Diagnostic Command
  readIDE(ErrFReg);
  if (dataLval == 0x01) {
    Serial.println("OK");
  } else {
    Serial.println("Fail");  // Units failing this may still work fine
  }

  delay(3000);

  Serial.println("ATAPI Device:");

  // Identify Device
  // ###############
  writeIDE(ComSReg, 0xA1, 0xFF);  // Issue Identify Device Command
  delay(500);
  
  // Add timeout for identify command
  unsigned long startTime = millis();
  boolean timeout = false;
  
  do {
    readIDE(DataReg);
    if (cnt == 0) {
      if (dataLval & (1 << 0)) {
        paclen = 16;
      }
    }
    if (cnt > 26 && cnt < 47) {
      Serial.print(dataHval);
      Serial.print(dataLval);
    }
    cnt++;
    readIDE(ComSReg);
    
    // Add timeout check
    if(millis() - startTime > 5000) {
      timeout = true;
      break;
    }
  } while (dataLval & (1 << 3));
  
  if(timeout) {
    Serial.println("Identify Device Command timeout");
    // Maybe try to recover here instead of hanging
  }
  readIDE(AStCReg);
  DRQ_clear_wait();

  // Check if unit ready
  // ###################
  unit_ready();       // Send packet 'test unit ready'
  req_sense();        // Send packet 'Request Sense'
  if (asc == 0x29) {  // Req. Sense returns 'HW Reset'
    unit_ready();     // (ASC=29h) at first since we had one.
    req_sense();      // New Req. Sense returns if media
  }                   // is present or not.
  do {
    unit_ready();         // Wait until drive is ready.
    req_sense();          // Some devices take some time
  } while (asc == 0x04);  // ASC=04h -> LOGICAL DRIVE NOT READY
}

// ############
// End of setup
// ############


// This part reads the push buttons, checks device audio status, interprets operator commands
// and displays the corresponding data depending on the status and/or the commands resulting
// from pressing the push buttons.

void loop() {
  // Scan push buttons
  if (digitalRead(EJCT) == LOW) {
    toc = false;  // Set toc invalid
    switch (chck_disk()) {
      case 0x00:  // If disk in tray case
        Serial.println("OPEN");
        eject();
        break;
      case 0xFF:  // If tray closed but no disk in case
        eject();
        Serial.println("OPEN");
        break;
      case 0X71:  // If tray open -> close it
        Serial.println("LOAD");
        load();
    }

    a_trck = s_trck;  // Reset to start track
  }

  if (digitalRead(STOP) == LOW) {
    a_trck = s_trck;  // Reset to start track
    stop_disk();      // Stop Disk
    stop();           // Stop unit
    toc = false;
  }

  if (digitalRead(PLAY) == LOW) {  // Play has been pressed
    switch (aud_stat) {
      case 0x15:  // If stopped
        play();   // start play
        break;
      case 0x12:   // if paused
        resume();  // resume play
        break;
      case 0x11:  // if playing
        pause();  // pause playback
    }
    toc = false;  // mark TOC unknown in case disk
                  // is removed using device eject buton
  }               // while play in progress

  if (digitalRead(NEXT) == LOW) {
    a_trck = a_trck + 1;                         // a_track becomes next track
    if (a_trck > e_trck) { (a_trck = s_trck); }  // over last track? -> point to start track
    get_TOC();                                   // Get MSF for a_trck
    fnc[51] = d_trck_m;                          // Store new play start position
    fnc[52] = d_trck_s;                          // in play packet and start play
    fnc[53] = d_trck_f;
    play();
    if (aud_stat == 0x12 || aud_stat == 0x15) {  // If paused or stopped -> pause
      pause();
    }
  }

  if (digitalRead(PREV) == LOW) {  // Basically like the NEXT function above
    a_trck = a_trck - 1;           // only backwards
    if (a_trck < s_trck) { (a_trck = e_trck); }
    get_TOC();
    fnc[51] = d_trck_m;
    fnc[52] = d_trck_s;
    fnc[53] = d_trck_f;
    play();
    if (aud_stat == 0x12 || aud_stat == 0x15) {  // If paused or stopped -> pause
      pause();
    }
  }

  if (millis() - prev_millis > interval) {  // This part will periodically check the
    read_subch_cmd();                       // current audio status and update the display
    if (aud_stat == 0x11) {                 // accordingly.
      Serial.println("PLAY");
      curr_MSF();  // Display pickup position
    }
    if (aud_stat == 0x12) {
      Serial.println("PAUSE");
      curr_MSF();
    }
    if (aud_stat == 0x15 && !toc) {  // If stopped and TOC invalid
      get_TOC();                     
      Disp_CD_data();               
      toc = true;                    
    }
    if (aud_stat == 0x00) {       // Audio status 0 covers all other posible
                                  // states not decoded by this sketch and
      Serial.println("NO DISC");  // handles them as NO DISC.
    }
    prev_millis = millis();
  }
}

// #######################################
// Auxiliary functions for displaying data
// #######################################

void Disp_CD_data() {        // Used to display track range and
                             // Total playing time as recovered
  Serial.print("Tracks  ");  // from reading the TOC
  Serial.print(s_trck, DEC);
  Serial.print("-");
  Serial.println(e_trck, DEC);

  Serial.print("Time   ");
  Serial.print(fnc[54], DEC);
  Serial.print(":");
  if (fnc[55] < 10) {
    Serial.print("0");  // Print a leading 0 for seconds when below 10
  }
  Serial.println(fnc[55], DEC);
}

void curr_MSF() {  // During PLAY or PAUSE operation show the pickup
  Serial.println(a_trck, DEC);
  Serial.println(MFS_M, DEC);
  Serial.println(":");
  if (MFS_S < 10) {
    Serial.println("0");  // Print a leading 0 for seconds when below 10
  }
  Serial.println(MFS_S, DEC);
}
// ##################################
// Auxiliary functions User Interface
// ##################################

void play() {
  idx = 48;   // pointer to play function and Play
  SendPac();  // from MSF location stored at idx=(51-56)
}  // See also doc. sff8020i table 76
void stop() {
  idx = 32;  // pointer to stop unit function
  SendPac();
}
void eject() {
  idx = 0;  // pointer to eject function
  SendPac();
}
void load() {
  idx = 16;  // pointer to load
  SendPac();
}
void pause() {
  idx = 64;  // pointer to hold
  SendPac();
}
void resume() {
  idx = 80;  // pointer to resume
  SendPac();
}
void stop_disk() {
  idx = 176;  // pointer to stop disk function
  SendPac();
}

// ###########################
// Auxiliary functions PCF8475
// ###########################

// Set to high impedance all ports of PCF8475 interfacing to IDE.
void highZ() {
  Wire.beginTransmission(RegSel);  // address IDE Register interface
  Wire.write((byte)255);           // queue FFh into buffer for setting all pins HIGH
  Wire.endTransmission();          // transmit buffered data to IDE Register interface
  Wire.beginTransmission(DataH);   // address IDE DD8-DD15
  Wire.write((byte)255);           // as above
  Wire.endTransmission();          //
  Wire.beginTransmission(DataL);   // address IDE DD0-DD7
  Wire.write((byte)255);           // as above
  Wire.endTransmission();          //
}

// Reset Device
void reset_IDE() {
  Wire.beginTransmission(RegSel);
  Wire.write((byte)B11011111);  // Bit 5 LOW to reset IDE via nRESET
  Wire.endTransmission();
  delay(40);
  Wire.beginTransmission(RegSel);
  Wire.write((byte)B11111111);  // Release reset
  Wire.endTransmission();
  delay(20);
  
  // Add status check after reset
  readIDE(AStCReg);
  if(dataLval == 0xFF) {  // If all bits are 1, device might not be present
    Serial.println("Device not responding after reset");
    return;
  }
}

// Read one word from IDE register
void readIDE(byte regval) {
  reg = regval & B01111111;  // set nDIOR bit LOW preserving register address
  Wire.beginTransmission(RegSel);
  Wire.write((byte)reg);
  Wire.endTransmission();
  Wire.requestFrom(DataH, 1);
  dataHval = Wire.read();
  Wire.requestFrom(DataL, 1);
  dataLval = Wire.read();
  highZ();  // set all I/O pins to HIGH -> impl. nDIOR release
}

// Write one word to IDE register
void writeIDE(byte regval, byte dataLval, byte dataHval) {
  reg = regval | B01000000;  // set nDIOW bit HIGH preserving register address
  Wire.beginTransmission(RegSel);
  Wire.write((byte)reg);
  Wire.endTransmission();
  Wire.beginTransmission(DataH);  // send data for IDE D8-D15
  Wire.write((byte)dataHval);
  Wire.endTransmission();
  Wire.beginTransmission(DataL);  // send data for IDE D0-D7
  Wire.write((byte)dataLval);
  Wire.endTransmission();
  reg = regval & B10111111;  // set nDIOW LOW preserving register address
  Wire.beginTransmission(RegSel);
  Wire.write((byte)reg);
  Wire.endTransmission();
  highZ();  // All I/O pins to high impedance -> impl. nDIOW release
}

// #################################################
// Auxiliary functions ATAPI Status Register related
// #################################################

// Wait for BSY clear
void BSY_clear_wait() {
  do {
    readIDE(ComSReg);
  } while (dataLval & (1 << 7));
}

// Wait for DRQ clear
void DRQ_clear_wait() {
  do {
    readIDE(ComSReg);
  } while (dataLval & (1 << 3));
}

// Wait for DRQ set
void DRQ_set_wait() {
  do {
    readIDE(ComSReg);
  } while ((dataLval & ~(1 << 3)) == true);
}

// Wait for DRY set
void DRY_set_wait() {
  do {
    readIDE(ComSReg);
  } while ((dataLval & ~(1 << 6)) == true);
}

// ##################################
// Auxiliary functions Packet related
// ##################################

// Send a packet starting at fnc array position idx
void SendPac() {
  writeIDE(AStCReg, B00001010, 0xFF);  // Set nIEN before you send the PACKET command!
  writeIDE(ComSReg, 0xA0, 0xFF);       // Write Packet Command Opcode
  delay(400);
  for (cnt = 0; cnt < paclen; cnt = cnt + 2) {  // Send packet with length of 'paclen'
    dataLval = fnc[(idx + cnt)];                // to IDE Data Registeraccording to idx value
    dataHval = fnc[(idx + cnt + 1)];
    writeIDE(DataReg, dataLval, dataHval);
    readIDE(AStCReg);  // Read alternate stat reg.
    readIDE(AStCReg);  // Read alternate stat reg.
  }
  BSY_clear_wait();
}

void get_TOC() {
  idx = 96;   // Pointer to Read TOC Packet
  SendPac();  // Send read TOC command packet
  delay(10);
  DRQ_set_wait();
  read_TOC();  // Fetch result
}

void read_TOC() {
  readIDE(DataReg);  // TOC Data Length not needed, don't care
  readIDE(DataReg);  // Read first and last session
  s_trck = dataLval;
  e_trck = dataHval;
  do {
    readIDE(DataReg);  // Skip Session no. ADR and control fields
    readIDE(DataReg);  // Read curent track number
    c_trck = dataLval;
    readIDE(DataReg);     // Read M
    c_trck_m = dataHval;  // Store M of curent track
    readIDE(DataReg);     // Read S and F
    c_trck_s = dataLval;  // Store S of current track
    c_trck_f = dataHval;  // Store F of current track

    if (c_trck == s_trck) {  // Store MSF of first track
      fnc[51] = c_trck_m;    //
      fnc[52] = c_trck_s;
      fnc[53] = c_trck_f;
    }
    if (c_trck == a_trck) {  // Store MSF of actual track
      d_trck_m = c_trck_m;   //
      d_trck_s = c_trck_s;
      d_trck_f = c_trck_f;
    }
    if (c_trck == 0xAA) {  // Store MSF of end position
      fnc[54] = c_trck_m;
      fnc[55] = c_trck_s;
      fnc[56] = c_trck_f;
    }
    readIDE(ComSReg);
  } while (dataLval & (1 << 3));  // Read data from DataRegister until DRQ=0
}

void read_subch_cmd() {
  idx = 144;               // Pointer to read Subchannel Packet
  SendPac();               // Send read Subchannel command packet
  readIDE(DataReg);        // Get Audio Status
  if (dataHval == 0x13) {  // Play operation successfully completed
    dataHval = 0x15;       // means drive is neither paused nor in play
  }                        // so treat as stopped
  if (dataHval == 0x11 ||   // playing
      dataHval == 0x12 ||   // paused
      dataHval == 0x15)    // stopped
  {
    aud_stat = dataHval;  //
  } else {
    aud_stat = 0;  // all other values will report "NO DISC"
  }
  readIDE(DataReg);  // Get (ignore) Subchannel Data Length
  readIDE(DataReg);  // Get (ignore) Format Code, ADR and Control
  readIDE(DataReg);  // Get actual track
  a_trck = dataLval;
  readIDE(DataReg);  // Get M field of actual MFS data and
  MFS_M = dataHval;  // store M it
  readIDE(DataReg);  // get S and F fields
  MFS_S = dataLval;  // Store S value
  do {
    readIDE(DataReg);
    readIDE(ComSReg);
  } while (dataLval & (1 << 3));  // Read rest of data from Data Reg. until DRQ=0
}

byte chck_disk() {
  byte disk_ok = 0xFF;  // assume no valid disk present.
  idx = 128;            // Send mode sense packet
  SendPac();            //
  delay(10);
  DRQ_set_wait();    // Wait for data ready to read.
  readIDE(DataReg);  // Read and discard Mode Sense data length
  readIDE(DataReg);  // Get Medium Type byte
                     // If valid audio disk present disk_ok=0x00
  if (dataLval == 0x02 || dataLval == 0x06 || dataLval == 0x12 || 
      dataLval == 0x16 || dataLval == 0x22 || dataLval == 0x26) {
    disk_ok = 0x00;
  }
  if (dataLval == 0x71) {  // Note if door open
    disk_ok = 0x71;
  }
  do {  // Skip rest of packet
    readIDE(DataReg);
    readIDE(ComSReg);
  } while (dataLval & (1 << 3));
  return (disk_ok);
}

void unit_ready() {  // Reuests unit to report status
  idx = 112;         // used to check_unit_ready
  SendPac();
}

void req_sense() {  // Request Sense Command is used to check
  idx = 160;        // the result of the Unit Ready command.
  SendPac();        // The Additional Sense Code is used,
  delay(10);        // see table 71 in sff8020i documentation
  DRQ_set_wait();
  cnt = 0;
  do {
    readIDE(DataReg);
    if (cnt == 6) {
      asc = dataLval;  // Store Additional Sense Code
    }
    cnt++;
    readIDE(AStCReg);
    readIDE(ComSReg);
  } while (dataLval & (1 << 3));  // Skip rest of packet
}

void init_task_file() {
  writeIDE(ErrFReg, 0x00, 0xFF);  // Set Feature register = 0 (no overlapping and no DMA)
  writeIDE(CylHReg, 0x02, 0xFF);  // Set PIO buffer to max. transfer length (= 200h)
  writeIDE(CylLReg, 0x00, 0xFF);
  writeIDE(AStCReg, 0x02, 0xFF);  // Set nIEN, we don't care about the INTRQ signal
  BSY_clear_wait();               // When conditions are met then IDE bus is idle,
  DRQ_clear_wait();               // this check may not be necessary (???)
}

void checkDeviceStatus() {
  readIDE(ComSReg);
  byte status = dataLval;
  
  if(status & (1 << 0)) Serial.println("Error bit set");
  if(status & (1 << 7)) Serial.println("Busy bit set");
  if(status & (1 << 6)) Serial.println("Drive Ready");
  if(status & (1 << 3)) Serial.println("Data Request set");
  if(status & (1 << 4)) Serial.println("Seek complete");
  
  // Read error register if error bit is set
  if(status & (1 << 0)) {
    readIDE(ErrFReg);
    Serial.print("Error register: 0x");
    Serial.println(dataLval, HEX);
  }
}

// END ####################################################################################
