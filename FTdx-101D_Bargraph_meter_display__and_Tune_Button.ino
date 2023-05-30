
/*
  This is a sketch for 9 bargraph meters on a 240 x 320, 2.8 TFT display (touch is not used) with ILI9341 for the Yaesu FTdx-101D.
  For the FTdx-101MP there is a different .INO file.
  It uses CAT commands through the RS232 port of the radio. It measures all 9 meters of the radio.
  It checks with a green/red indication if there is connection with the radio with the correct baudrate.
  It also has an external tune button possibility. Just add a momentary pushbutton (if desired).
  On button press: it reads present power and mode setting, then sets PWR to a selected value, mode to FM-N, then enables the MOX.
  On button release: it disables the TX and restores original power and mode settings.
  This software is written by Eeltje Luxen, PA0LUX. With contributions of John, G4BEH and is in the public domain.
  Set FTdx-101 RS232 connection to 19200 bps.
  version 1.0 first release
  version 1.0.2, may 2023, only comment changes, no code change since v1.0. it has been tested on an Arduino UNO and NANO.
  This is version 1.1, may 2023,comment changes + red dots added to power scale and avaraging on the temperature reading.
*/


#include "SPI.h"                                          // make sure you have these in your library
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Smoothed.h"                                     // library used for smoothening the temp reading
#define cs   10                                           // these are the connections for the NANO, regarding display 
#define dc   9                                            // these are the connections for the NANO, regarding display 
#define rst  7                                            // these are the connections for the NANO, regarding display 
#define ILI9341_BACKGROUND  0x542E    // blue-grey color is not in library, see for codes http://www.barth-dev.de/online/rgb565-color-picker/ 
#define switchPin  2                                      // Tune button is connected to NANO pin 2
#define set_tune_pwr "PC020;"                             // set tune power, you can choose which PWR to use for tuning (005 - 100 W)
#define timeout_delay 10;                                 // delay to flag a data timeout from the radio, now set at 10 mSec, same as in radio
Smoothed <float> smoothed_temp;                           // Create an instance of the class to use.

byte X = 55;                   // starting point of meter bars from left
byte Y = 18;                   // row of complete meter: bar, outline and text
byte H = 6;                    // height of meter bars
byte Z = 15;                   // starting point of the meters text from left
byte SWR;                      // measured value of SWR
byte pSWR = 0;                 // previous measured value of SWR
byte Temp;                     // measured value of temperature
byte pTemp = 0;                // previous measured value of temperature
byte Comp;                     // measured value of Comp
byte pComp = 0;                // previous measured value of Comp
byte IDD;                      // measured value of IDD
byte pIDD = 0;                 // previous measured value of IDD
byte VDD;                      // measured value of VDD
byte pVDD = 0;                 // previous measured value of VDD
byte ALC;                      // measured value of VDD
byte pALC = 0;                 // previous measured value of VDD
byte SMM;                      // measured value of main S meter
byte pSMM = 0;                 // previous measured value of main S meter
byte SSM;                      // measured value of sub S meter
byte pSSM = 0;                 // previous measured value of sub S meter
byte PO;                       // measured value of PO meter meter
byte pPO = 0;                  // previous measured value of PO meter
byte delaycounter = 0;         // counter used for debounce of tune button

int val;                        // variable for reading the Tune button status
int val2;                       // variable for reading the delayed/debounced status of Tune button
bool buttonrelease = false;     // flag to send just one set of commands at Tune button release
bool buttonpress = false;       // flag to send just one set of commands at Tune button press
bool tune = true;               // flag, do not tune when false
String prevpwr;                 // this string stores the original power setting
String prevmode;                // this string stores the original mode setting
bool receiving;                 // variable for the receive radio data process
String CAT_buffer;              // string to hold the received CAT data from radio
char rx_char;                   // variable for each character received from the radio
char t;                         // used to clear input buffer
unsigned long timeout, current_millis;  // variables to handle communication timeout errors
bool constatus = false;         // status of connection to radio
bool sub_flag1 = false;         // flag to check sub receiver status
bool sub_flag2 = true;          // flag to check sub receiver status
bool main_flag1 = false;        // flag to check main receiver status
bool main_flag2 = true;         // flag to check main receiver status
bool txrx_flag1 = false;        // flag to check if in TX or RX
bool txrx_flag2 = true;         // flag to check if in TX or RX
bool in_tx = false;             // status of tx
String pwrsetting;              // used for displaying power setting
String prevpwrsetting;          // used for displaying power setting

Adafruit_ILI9341 tft = Adafruit_ILI9341(cs, dc, rst); // Invoke custom library


void setup() {

  pinMode(switchPin, INPUT);                 // set the Tune button pin 2 as input
  Serial.begin(19200, SERIAL_8N2);           // RS232 connection speed to FTdx101, NOTE 2 stopbits
  pinMode(switchPin, INPUT);                 // set the Tune button pin as input

  smoothed_temp.begin(SMOOTHED_EXPONENTIAL, 10);           // set the filter level to 10. Higher numbers will result in less filtering

  tft.begin();
  tft.setRotation(1);                                       // set display orientation
  tft.fillScreen(ILI9341_BLACK);                            // fill background with black
  tft.setCursor(70, 50);                                    // write the welcome screen
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.println("Meter display &");
  tft.setCursor(16, 80);
  tft.println("tune button for FTdx-101D");
  tft.setCursor(150, 110);
  tft.println("by");
  tft.setCursor(40, 140);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Eeltje Luxen - PA0LUX");
  tft.setCursor(240, 220);
  tft.println("v1.1");
  delay(2000);                                                    // show welcome screen for 2000 mS

  tft.fillScreen(ILI9341_BACKGROUND);                             // fill background with selected color
  tft.setTextSize(1);                                             // small text


  // now first draw most meter outlines with their scale text and colors


  //Draw SWR meter
  tft.fillRect((X - 44), (Y - 13), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.setTextColor(ILI9341_WHITE);                                // text colour to white
  tft.setCursor(Z, Y - 6);
  tft.println("SWR");                                             // print the meters text
  tft.drawRect((X - 1), (Y - 14), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y - 14), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setCursor((X + 2), Y - 9);
  tft.println("1");                                               // display SWR = 1 scale text white
  tft.setCursor((X + 48), Y - 9);
  tft.println("1.5");                                             // display SWR = 1.5 scale text white
  tft.setCursor((X + 90), Y - 9);
  tft.println("2");                                               // display SWR = 2 scale text white
  tft.setTextColor(ILI9341_ORANGE);                               // text colour to orange
  tft.setCursor((X + 136), Y - 9);
  tft.println("3");                                               // display SWR = 3 scale text orange
  tft.setTextColor(ILI9341_RED);                                  // text colour to red
  tft.setCursor((X + 223), Y - 9);
  tft.println("5");                                               // display SWR = 5 scale text red
  tft.setCursor((X + 238), Y - 9);
  tft.println("o");                                               // display SWR = oo scale text red
  tft.setCursor((X + 242), Y - 9);
  tft.println("o");


  //Draw COMP meter
  tft.fillRect((X - 44), (Y + 17), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.setTextColor(ILI9341_WHITE);                                // text colour to white
  tft.setCursor(Z, (Y + 24));
  tft.println("COMP");                                            // print the meters text
  tft.drawRect((X - 1), (Y + 16), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 16), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setCursor((X + 2), Y + 21);
  tft.println("0");                                               // display COMP = 0 scale text white
  tft.setCursor((X + 50), Y + 21);
  tft.println("5");                                               // display COMP = 5 scale text white
  tft.setTextColor(ILI9341_YELLOW);                               // text colour to yellow
  tft.setCursor((X + 101), Y + 21);
  tft.println("10");                                              // display COMP = 10 scale text yellow
  tft.setTextColor(ILI9341_ORANGE);                               // text colour to orange
  tft.setCursor((X + 152), Y + 21);
  tft.println("15");                                              // display COMP = 15 scale text orange
  tft.setTextColor(ILI9341_RED);                                  // text colour to red
  tft.setCursor((X + 203), Y + 21);
  tft.println("20");                                              // display COMP = 20 scale text red
  tft.setTextColor(ILI9341_WHITE);                                // text colour to white
  tft.setCursor((X + 242), Y + 21);
  tft.println("dB");                                              // display dB scale text white


  //Draw TEMP meter
  tft.fillRect((X - 44), (Y + 47), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.setCursor(Z, (Y + 54));
  tft.println("TEMP");                                            // print the meters text
  tft.drawRect((X - 1), (Y + 46), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 46), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setCursor((X + 2), Y + 51);
  tft.println("0");                                               // display TEMP = 0 scale text white
  tft.setCursor((X + 46), Y + 51);
  tft.println("20");                                              // display TEMP = 20 scale text white
  tft.setCursor((X + 98), Y + 51);
  tft.println("40");                                              // display TEMP = 40 scale text white
  tft.setTextColor(ILI9341_YELLOW);                               // text colour to yellow
  tft.setCursor((X + 143), Y + 51);
  tft.println("60");                                              // display TEMP = 60 scale text yellow
  tft.setTextColor(ILI9341_RED);                                  // text colour to red
  tft.setCursor((X + 193), Y + 51);
  tft.println("80");                                              // display TEMP = 80 scale text red
  tft.setTextColor(ILI9341_WHITE);                                // text colour to white
  tft.setCursor((X + 235), Y + 51);                               // display Celsius scale text (o)white
  tft.println(" \367");
  tft.setCursor((X + 248), Y + 51);                               // display Celsius scale text (C) white
  tft.println("C");


  //Draw IDD meter
  tft.fillRect((X - 44), (Y + 77), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.setCursor(Z, (Y + 84));
  tft.println("IDD");                                             // print the meters text
  tft.drawRect((X - 1), (Y + 76), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 76), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setCursor((X + 2), Y + 81);
  tft.println("0");                                               // display IDD = 0 scale text, all scale text in white
  tft.setCursor((X + 50), Y + 81);
  tft.println("5");                                               // display IDD = 5 scale text
  tft.setCursor((X + 101), Y + 81);
  tft.println("10");                                              // display IDD = 10 scale text
  tft.setCursor((X + 152), Y + 81);
  tft.println("15");                                              // display IDD = 15 scale text
  tft.setCursor((X + 203), Y + 81);
  tft.println("20");                                              // display IDD = 20 scale text
  tft.setCursor((X + 235), Y + 81);
  tft.println("25");                                              // display IDD = 25 scale text
  tft.setCursor((X + 248), Y + 81);
  tft.println("A");                                               // display A scale text


  //Draw VDD meter
  tft.fillRect((X - 44), (Y + 107), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.setCursor(Z, (Y + 114));
  tft.println("VDD");                                              // print the meters text
  tft.drawRect((X - 1), (Y + 106), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 106), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.fillRect(X, (Y + 107), 244, 4, ILI9341_WHITE);         // color to greenyellow
  tft.fillRect((X + 178), (Y + 107), (X - 25), 12, ILI9341_GREENYELLOW);   // bar indicating allowed tolerance of VDD in greenyellow
  tft.setTextColor(ILI9341_RED);                                   // textcolor set to red
  tft.setCursor((X + 180), Y + 109);
  tft.println("13.8");                                             // display VDD = 13.8 scale text in red
  tft.setTextColor(ILI9341_WHITE);                                 // make text  = V white
  tft.setCursor((X + 248), Y + 111);
  tft.println("V");                                                // display V scale text


  //Draw ALC meter - ALC meter has different dimensions
  tft.fillRect((X - 44), (Y + 152), (X + 244), 14, ILI9341_BLACK); // background of meter to black
  tft.setCursor(Z, (Y + 155));
  tft.println("ALC");                                              // print the meters text in white
  tft.drawRect((X - 1), (Y + 151), 257, 16, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 151), 44, 16, ILI9341_WHITE);        // draw rectangular around the text
  tft.fillRect(X, (Y + 152), 128, 5, ILI9341_WHITE);               // white ALC scale


  //Draw S Sub meter
  tft.fillRect((X - 44), (Y + 197), (X + 244), 21, ILI9341_BLACK);  // background of meter to black
  tft.setTextColor(ILI9341_YELLOW);                                 // set textcolor to yellow
  tft.drawRect((X - 1), (Y + 196), 257, 23, ILI9341_WHITE);         // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 196), 44, 23, ILI9341_WHITE);         // draw rectangular around the text
  tft.setCursor((X + 4), Y + 201);
  tft.println("1");                                                 // display S = 1 scale text, 1 S point = 14 pixels
  tft.setCursor((X + 4), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 11), Y + 201);
  tft.println("2");                                                 // display S = 2 scale text yellow
  tft.setCursor((X + 11), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 28), Y + 201);
  tft.println("3");                                                 // display S = 3 scale text yellow
  tft.setCursor((X + 28), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 48), Y + 201);
  tft.println("4");                                                 // display S = 4 scale text yellow
  tft.setCursor((X + 48), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 65), Y + 201);
  tft.println("5");                                                 // display S = 5 scale text yellow
  tft.setCursor((X + 65), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 80), Y + 201);
  tft.println("6");                                                 // display S = 6 scale text yellow
  tft.setCursor((X + 80), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 95), Y + 201);
  tft.println("7");                                                 // display S = 7 scale text yellow
  tft.setCursor((X + 95), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 113), Y + 201);
  tft.println("8");                                                 // display S = 8 scale text yellow
  tft.setCursor((X + 113), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 131), Y + 201);
  tft.println("9");                                                 // display S = 9 scale text yellow
  tft.setCursor((X + 131), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setTextColor(ILI9341_RED);                                    // set textcolor to red
  tft.setCursor((X + 170), Y + 201);
  tft.println("20");                                                // display S = 20 scale text red
  tft.setCursor((X + 170), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 207), Y + 201);
  tft.println("40");                                                // display S = 40 scale text red
  tft.setCursor((X + 207), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 240), Y + 201);
  tft.println("60");                                                // display S = 60 scale text red
  tft.setCursor((X + 250), Y + 211);
  tft.println(".");                                                 // display S = dot scale
  tft.setTextColor(ILI9341_WHITE);                                  // make text white

  // print the text- connection status & Power set to
  tft.setCursor((X - 45), 154);
  tft.println("Connection status:");
  tft.setCursor((X + 135), 154);
  tft.println("Power set to:");
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor((X + 240), 154);
  tft.println("W");
  tft.setTextColor(ILI9341_WHITE);

  draw_M_S_meter();                                                 // call routine draw_M_S_meter. S meter main differs, it can be substituted by the PO meter


}


void loop() {

  // check rs232 connection
  do
  { t = Serial.read();                                              // this is to empty the input buffer when starting reading data from radio,
  } while (Serial.available() > 0);                                 // buffer contains erratic data when the radio is switched on (fix power up sequence)
  Serial.print("TX;");                                              // send CAT command to the radio, ask for TX status
  get_radio_response();                                             // call routine to read from radio
  if (CAT_buffer.startsWith("TX")) {                                // if the answer is correct (it should start with TX)
    constatus = true;                                               // connection status is ok, green light must be turned on
  }
  else {                                                            // we do not get a (correct-) answer
    constatus = false;                                              // red light must be turned on, no connection with radio, bad data/baudrate or radio powered off
  }
  show_status();                                                    // call routine to show connected status, led in display reads red or green
  Tune();                                                           // call tune routine


  // read and display power setting from FTdx101, only write to screen when changed

  Serial.print("PC;");                                              // send CAT command to the radio, ask what power has been set
  get_radio_response();                                             // call routine to read from radio
  CAT_buffer.remove(0, 2);                                          // remove characters PC
  if (CAT_buffer.startsWith("0")) {                                 // remove leading zero if present
    CAT_buffer.remove(0, 1);
    if (CAT_buffer.startsWith("0")) {                               // remove leading zero if present
      CAT_buffer.remove(0, 1);
    }
  }
  pwrsetting = CAT_buffer;                                          // present setting to pwrsetting
  if  (prevpwrsetting !=  pwrsetting)                               // if setting has changed since last read
  {
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor((X + 216), 154);
    tft.fillRect((X + 214), 152, (X - 32), 12, ILI9341_BACKGROUND);  // clear previous display
    tft.println(pwrsetting);                                         // display power setting, it has changed
  }
  prevpwrsetting =  pwrsetting;                                      // present setting now becomes previous setting


  // check if in TX, if yes then draw PO meter, otherwise draw Main S meter, do these only once to avoid flicker

  Serial.print("TX;");                                              // send CAT command to the radio, ask for TX status
  get_radio_response();                                             // call routine to read from radio
  if ((((CAT_buffer.startsWith("TX2")) || (CAT_buffer.startsWith("TX1"))) && (txrx_flag1 == false))) {  // ask if the FTdx101 is in TX (PTT or CAT, for the first loop)
    draw_PO_meter();                                                // draw the PO meter outline, can be substituted by Main S meter
    in_tx = true;                                                   // set flag we are in TX
    txrx_flag1 = true;                                              // flag so that text and clear is only written once, toggle function
    txrx_flag2 = true;                                              // flag so that text and clear is only written once, toggle function
    main_flag1 = false;                                             // after TX reset flag to check main receiver status again, toggle function
    main_flag2 = true;                                              // after TX reset flag to check main receiver status again, toggle function
  }
  else {
    if ((txrx_flag2 == true) && (CAT_buffer == ("TX0"))) {          // we are in RX, check if this is the first RX loop
      in_tx = false;                                                // reset tx flag to false
      draw_M_S_meter();                                             // call draw main S meter outline, can be substituted by PO meter
      txrx_flag1 = false;                                           // flag so that text and clear is only written once, toggle function
      txrx_flag2 = false;                                           // flag so that text and clear is only written once, toggle function
    }
  }
  if (in_tx == false) {                                               // not in TX, now check if main receiver is selected, if so then read main S meter status & meter
    Serial.print("FR;");                                              // send CAT command to the radio, ask for receiver statusses
    get_radio_response();                                             // call routine to read from radio
    if ((CAT_buffer == ("FR00")) || (CAT_buffer == ("FR01"))) {       // if one of both is true then main receiver is selected on the radio
      Serial.print("RM1;");                                           // send CAT command to the radio, ask main S meter reading
      get_radio_response();                                           // call routine to read from radio
      convert_CAT_buffer();                                           // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
      SMM = CAT_buffer.toInt();                                       // store string as int in SSM to be displayed as sub S meter bar
      if (main_flag1 == false) {                                      // clear area and write the text S main if main is selected, but only once to reduce flicker
        tft.fillRect((X - 44), (Y + 172), 42, 20, ILI9341_BLACK);     // clear the text area
        tft.setCursor(Z, (Y + 178));
        tft.setTextColor(ILI9341_WHITE);                              // set textcolor to white
        tft.println("S Main");                                        // display text of main receiver meter bar
      }
      main_flag1 = true;                                              // flag so that text and clear is only written once, toggle function
      main_flag2 = true;                                              // flag so that text and clear is only written once, toggle function
    }
    else {                                                            // the main receiver is not selected on the radio
      if (main_flag2 == true) {                                       // first loop
        tft.fillRect(X, (Y + 184), (255), H, ILI9341_BLACK);          // clear main receiver meter bar, for receiver is off.
        tft.fillRect((X - 44), (Y + 172), 42, 20, ILI9341_BLACK);     // clear the text area
        tft.setCursor(Z, (Y + 178));
        tft.setTextColor(ILI9341_RED);                                // set textcolor to red
        tft.println("O F F");                                         // display the text O F F for the main receiver
        main_flag2 = false;                                           // flag so that text and clear is only written once, toggle function
        main_flag1 = false;                                           // flag so that text and clear is only written once, toggle function
      }
    }
  }



  // check if sub receiver is selected, if so then read Sub S meter

  Serial.print("FR;");                                              // send CAT command to the radio, ask if sub receiver is active
  get_radio_response();                                             // call routine to read from radio, check if sub receiver is operational
  if ((CAT_buffer == ("FR00")) || (CAT_buffer == ("FR10"))) {       // if one of both is true then sub receiver is selected on the radio
    Serial.print("RM2;");                                           // send CAT command to the radio, ask sub S meter reading
    get_radio_response();                                           // call routine to read from radio
    convert_CAT_buffer();                                           // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
    SSM = CAT_buffer.toInt();                                       // store string as int in SSM to be displayed as sub S meter bar
    if (sub_flag1 == false) {                                       // clear area and write the text S Sub if sub is active, but only once to reduce flicker
      tft.fillRect((X - 44), (Y + 198), 40, 20, ILI9341_BLACK);     // clear the text area
      tft.setCursor(Z, (Y + 204));
      tft.setTextColor(ILI9341_YELLOW);                             // set textcolor to yellow
      tft.println("S Sub");                                         // display text of Sub receiver meter bar
    }
    sub_flag1 = true;                                               // flag so that text and clear is only written once, toggle function
    sub_flag2 = true;                                               // flag so that text and clear is only written once, toggle function
  }
  else {                                                            // the sub receiver is not selected on the radio
    if (sub_flag2 == true) {
      tft.fillRect(X, (Y + 210), (255), H, ILI9341_BLACK);          // clear sub receiver meter bar, for receiver is off.
      tft.fillRect((X - 44), (Y + 198), 40, 20, ILI9341_BLACK);     // clear the text area
      tft.setCursor(Z, (Y + 204));
      tft.setTextColor(ILI9341_RED);                                // set textcolor to yellow
      tft.println("O F F");                                         // display the text OFF for the sub receiver
      sub_flag2 = false;                                            // flag so that text and clear is only written once, toggle function
      sub_flag1 = false;                                            // flag so that text and clear is only written once, toggle function
    }
  }


  // read comp meter
  Serial.print("RM3;");                                    // send CAT command to the radio, ask comp value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  Comp = CAT_buffer.toInt();                                // store string as int in Comp to be displayed as Comp meter bar

  // read ALC meter
  Serial.print("RM4;");                                    // send CAT command to the radio, ask ALC value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  ALC = CAT_buffer.toInt();                                // store string as int in ALC to be displayed as ALC meter bar

  //read power meter
  Serial.print("RM5;");                                  // send CAT command to the radio, ask PO value
  get_radio_response();                                  // call routine to read from radio
  convert_CAT_buffer();                                  // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  PO = CAT_buffer.toInt();                               // store string as int in PO to be displayed as power meter bar


  // read SWR meter
  Serial.print("RM6;");                                    // send CAT command to the radio, ask SWR value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  SWR = CAT_buffer.toInt();                                // store string as int in ALC to be displayed as SWR meter bar

  // read IDD meter
  Serial.print("RM7;");                                    // send CAT command to the radio, ask IDD value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  IDD = CAT_buffer.toInt();                                // store string as int in IDD to be displayed as IDD meter bar

  // read VDD meter
  Serial.print("RM8;");                                    // send CAT command to the radio, ask VDD value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  VDD = CAT_buffer.toInt();                                // store string as int in VDD to be displayed as VDD meter bar

  // read temp meter
  Serial.print("RM9;");                                    // send CAT command to the radio, ask TEMP value
  get_radio_response();                                    // call routine to read from radio
  convert_CAT_buffer();                                    // CAT_buffer holds received string in format: RMNVVV000; N=meternumber, VVV is wanted value
  Temp = CAT_buffer.toInt();                               // store string as int in Temp to be displayed as temperature meter bar
  smoothed_temp.add(float(Temp));                          // add the new value to the value stores
  Temp = (int(smoothed_temp.get()));                       // get the smoothed values

  // here the meter bargraphs are filled with the measured values which were received from the radio

  // Fill the SWR bar
  if (SWR != pSWR) {                                                      // if new SWR is same as previous value, do nothing, no need to draw graphics if they did not change
    if (SWR < pSWR) {
      tft.fillRect((X + SWR), Y, (255 - SWR), H, ILI9341_BLACK);          // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                     // if new value is higher than previous value, just add to the bar
    if (SWR <= 96) {
      tft.fillRect(X, Y, SWR, H, ILI9341_GREEN);                          // when SWR is equal or less than 96, make bar green
    }
    if ((SWR > 96) && (SWR <= 128)) {                                     // when SWR between 97 and 128, make first part of bar green and second part yellow
      tft.fillRect(X, Y, 96, H, ILI9341_GREEN);                           // fill 96 pixels green, start from X, row is Y
      tft.fillRect((96 + X), Y, (SWR - 96), H, ILI9341_YELLOW);           // fill 96 pixels green, add SWR minus green part as yellow
    }
    if ((SWR > 128) && (SWR <= 192)) {                                    // when SWR between 129 and 192, make this part of bar orange
      tft.fillRect(X, Y, 96, H, ILI9341_GREEN);                           // first 96 pixels green
      tft.fillRect((96 + X), Y, 32, H, ILI9341_YELLOW);                   // next 32 pixels yellow
      tft.fillRect((128 + X), Y, (SWR - 128), H, ILI9341_ORANGE);         // add SWR minus green and yellow part as orange
    }
    if (SWR > 192) {                                                      // when SWR higher than 192, make this part of bar red
      tft.fillRect(X, Y, 96, H, ILI9341_GREEN);                           // first 96 pixels green
      tft.fillRect((96 + X), Y, 32, H, ILI9341_YELLOW);                   // next 34 pixels yellow
      tft.fillRect((128 + X), Y, 64, H, ILI9341_ORANGE);                  // add SWR minus green and yellow part as orange
      tft.fillRect((192 + X), Y, (SWR - 192), H, ILI9341_RED);            // add SWR minus green, yellow and orange part as red
    }
    pSWR = SWR;                                                                // store measured SWR value as previous value
  }



  // Fill the COMP bar
  if (Comp != pComp) {                                                        // if new COMP value is same as previous value, do nothing, no need to draw graphics if they did not change
    if (Comp < pComp) {
      tft.fillRect((X + Comp), (Y + 30), (255 - Comp), H, ILI9341_BLACK);     // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                         // if new value is higher than previous value, just add to the bar
    if (Comp <= 102) {
      tft.fillRect(X, (Y + 30), Comp, H, ILI9341_GREEN);                      // when COMP is equal or less than 102 (10 db), make bar green
    }
    if ((Comp > 102) && (Comp <= 152)) {                                      // when COMP between 102 (10 dB) and 152 (15 dB), make first part of bar green and second part Yellow
      tft.fillRect(X, (Y + 30), 102, H, ILI9341_GREEN);                       // fill 102 pixels green, start from X, row is Y
      tft.fillRect((102 + X), (Y + 30), (Comp - 102), H, ILI9341_YELLOW);     // fill 102 pixels green, add COMP minus green part as yellow
    }
    if ((Comp > 152) && (Comp <= 203)) {                                      // when COMP between 152 (15 dB) and 203 (20 degr), make this part of bar orange
      tft.fillRect(X, (Y + 30), 102, H, ILI9341_GREEN);                       // first 102 pixels green
      tft.fillRect((102 + X), (Y + 30), 50, H, ILI9341_YELLOW);               // next 50 pixels yellow, 0.2 of scale
      tft.fillRect((152 + X), (Y + 30), (Comp - 152), H, ILI9341_ORANGE);     // add COMP minus green and yellow part as orange
    }
    if (Comp > 203) {                                                         // when COMP is higher than 203, (25 dB) make this part of bar red
      tft.fillRect(X, (Y + 30), 102, H, ILI9341_GREEN);                       // first 102  pixels green
      tft.fillRect((102 + X), (Y + 30), 50, H, ILI9341_YELLOW);               // next 50 pixels yellow
      tft.fillRect((152 + X), (Y + 30), 50, H, ILI9341_ORANGE);               // next 50 pixels orange
      tft.fillRect((203 + X), (Y + 30), (Comp - 203), H, ILI9341_RED);        // add COMP minus green, yellow and orange part as red
    }
    pComp = Comp;                                                             // store measured COMP value as previous value
  }



  // Fill the temperature bar
  if (Temp != pTemp) {                                                        // if new Temperature is same as previous value, do nothing, no need to draw graphics if they did not change
    if (Temp < pTemp) {
      tft.fillRect((X + Temp), (Y + 60), (255 - Temp), H, ILI9341_BLACK);     // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                         // if new value is higher than previous value, just add to the bar
    if (Temp <= 152) {
      tft.fillRect(X, (Y + 60), Temp, H, ILI9341_GREEN);                      // when temperature is equal or less than 152 (60 degrees), make bar green
    }
    if ((Temp > 152) && (Temp <= 177)) {                                      // when temperature between 152 (60 degr) and 177 (70 degr), make first part of bar green and second part Yellow
      tft.fillRect(X, (Y + 60), 152, H, ILI9341_GREEN);                       // fill 152 pixels green, start from X, row is Y
      tft.fillRect((152 + X), (Y + 60), (Temp - 152), H, ILI9341_YELLOW);     // fill 152 pixels green, add temperature minus green part as yellow
    }
    if ((Temp > 177) && (Temp <= 203)) {                                      // when temperature between (70 degr) and (80 degr), make this part of bar orange
      tft.fillRect(X, (Y + 60), 152, H, ILI9341_GREEN);                       // first 152 pixels green
      tft.fillRect((152 + X), (Y + 60), 24, H, ILI9341_YELLOW);               // next 24 pixels yellow
      tft.fillRect((176 + X), (Y + 60), (Temp - 176), H, ILI9341_ORANGE);     // add temperature minus green and yellow part as orange
    }
    if (Temp > 203) {                                                         // when temperature higher than 192, make this part of bar red
      tft.fillRect(X, (Y + 60), 152, H, ILI9341_GREEN);                       // first 152  pixels green
      tft.fillRect((152 + X), (Y + 60), 24, H, ILI9341_YELLOW);               // next 24 pixels yellow
      tft.fillRect((176 + X), (Y + 60), 27, H, ILI9341_ORANGE);               // add temperature minus green and yellow part as orange
      tft.fillRect((203 + X), (Y + 60), (Temp - 203), H, ILI9341_RED);        // add temperature minus green, yellow and orange part as red
    }
    pTemp = Temp;                                                             // store measured temperature value as previous value
  }



  // Fill the IDD bar
  if (IDD != pIDD) {                                                      // if new IDD is same as previous value, do nothing, no need to draw graphics if they did not change
    if (IDD < pIDD) {
      tft.fillRect((X + IDD), (Y + 90), (255 - IDD), H, ILI9341_BLACK);   // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                     // if new value is higher than previous value, just add to the bar
    tft.fillRect(X, (Y + 90), IDD, H, ILI9341_BLUE);                  // IDD, startpoint,row,value,height of bar
    pIDD = IDD;                                                           // store measured IDD value as previous value
  }



  // Fill the VDD bar
  if (VDD != pVDD) {                                                      // if new VDD is same as previous value, do nothing, no need to draw graphics if they did not change
    if (VDD < pVDD) {
      tft.fillRect((X + VDD), (Y + 120), (255 - VDD), H, ILI9341_BLACK);  // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                     // if new value is higher than previous value, just add to the bar
    tft.fillRect(X, (Y + 120), VDD, H, ILI9341_BLUE);                 // VDD, startpoint,row,value,height of bar
    pVDD = VDD;                                                           // store measured VDD value as previous value
  }



  // Fill the ALC bar
  if (ALC != pALC) {                                                         // if new ALC value is same as previous value, do nothing, no need to draw graphics if they did not change
    if (ALC < pALC) {
      tft.fillRect((X + ALC), (Y + 158), (255 - ALC), H, ILI9341_BLACK);     // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                        // if new value is higher than previous value, just add to the bar
    if (ALC <= 128) {
      tft.fillRect(X, (Y + 158), ALC, H, ILI9341_BLUE);                      // when ALC is equal or less than half scale, make this part of bar blue
    }
    if (ALC > 128) {
      tft.fillRect((X), (Y + 158), (ALC ), H, ILI9341_ORANGE);                // if ALC is more than half scale make complete bar orange
    }
    pALC = ALC;                                                               // store measured ALC value as previous value
  }


  // Fill the main S Meter bar

  if (in_tx ==  false) {                                                      // we are in RX
    if (SMM != pSMM) {                                                        // if new S meter value is same as previous value, do nothing, no need to draw graphics if they did not change
      if (SMM < pSMM) {
        tft.fillRect((X + SMM), (Y + 184), (255 - SMM), H, ILI9341_BLACK);    // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
      }                                                                       // if new value is higher than previous value, just add to the bar
      tft.fillRect(X, (Y + 184), SMM, H, ILI9341_GREEN);                      // S meter value, startpoint,row,value,height of bar
      pSMM = SMM;                                                             // store measured S meter value as previous value
    }
  }
  else {
    if (PO < pPO) {
      tft.fillRect((X + PO), (Y + 184), (255 - PO), H, ILI9341_BLACK);        // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                         // if new value is higher than previous value, just add to the bar
    tft.fillRect(X, (Y + 184), PO, H, ILI9341_BLUE);                          // PO, startpoint,row,value,height of bar
    pPO = PO;                                                                 // store measured IDD value as previous value
  }

  // Fill the sub S Meter bar

  if (SSM != pSSM) {                                                        // if new Sub S meter value is same as previous value, do nothing, no need to draw graphics if they did not change
    if (SSM < pSSM) {
      tft.fillRect((X + SSM), (Y + 210), (255 - SSM), H, ILI9341_BLACK);    // if new value < previous value, clear the bar partly. not whole bar but from value to 255, reduces flicker
    }                                                                       // if new value is higher than previous value, just add to the bar
    tft.fillRect(X, (Y + 210), SSM, H, ILI9341_YELLOW);                     // sub S meter value, startpoint,row,value,height of bar
    pSSM = SSM;                                                             // store measured sub S meter value as previous value
  }


}


void get_radio_response()                                         // this routine receives the meter values from the radio
{

  // set a timeout value for if we do not get an answer from the radio, time out is non blocking

  current_millis = millis();                                // get the current time
  timeout = current_millis + timeout_delay;                 // calculate the timeout time

  // check for millis() rollover condition - the Arduino millis() counter rolls over about every 47 days
  if (timeout < current_millis)                             // we've calculated the timeout during a millis() rollover event
  {
    timeout = timeout_delay;              // go ahead and calculate as if we've rolled over already (adds a few millis to the timeout delay)
  }

  // start to receive CAT response from the radio
  receiving = true;                                         // start receiving
  CAT_buffer = "";                                          // clear CAT buffer
  do
  {
    if (millis() > timeout)                                 // no data received within timeout delay
    {
      // there is a time out - exit thru break
      receiving = false;                                    // clear receive flag
      tune = false;                                         // do not start tuning, because no info received
      CAT_buffer = "";                                      // clear buffer
      break;                                                // no receive response, exit to loop
    }
    if (Serial.available() && receiving)                    // if there's a character in the rx buffer and we are receiving
    {
      rx_char = Serial.read();                              // get one character at the time from radio
      if (rx_char == ';')                                   // ";" indicates the end of the response from the radio
      {
        receiving = false;                                  // turn off the ok to receive flag, this was last character
        tune = true;                                        // a complete answer is received, now we are allowed to proceed,
      }  else
      {
        CAT_buffer = CAT_buffer + rx_char;                  // add the received character to the CAT rx string, build the received string
      }
    }
  } while (receiving);                                      // keep looping while ok to receive data from the radio
}


void convert_CAT_buffer()                                   // get the wanted data out of the received string ( wanted data is 0 - 255)
{
  CAT_buffer.remove(0, 3);                                  // delete the text RMN from string (string was RMNVVV000;), now VVV000;
  CAT_buffer.remove(3, 4);                                  // delete the text 000; from string (string was VVV000;), now VVV
}



void show_status() {                                      // this routine shows a red or green connection statusled on the display

  tft.drawCircle(75 + X, 157, 6, ILI9341_BLACK);         // outline of circle is black
  if (constatus == false) {
    tft.fillCircle(75 + X, 157, 5, ILI9341_RED);         // red led if status is false
  }
  else {
    tft.fillCircle(75 + X, 157, 5, ILI9341_GREEN);       // green led if status is true
  }
}


void draw_M_S_meter() {

  //Draw S Main meter, the Main S meter can be substituted by the PO meter


  tft.fillRect (X, (Y + 171), (X + 200), 21, ILI9341_BLACK); // background of meter to black
  tft.drawRect((X - 1), (Y + 170), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 170), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setTextColor(ILI9341_WHITE);                              // set textcolor to white
  tft.setCursor((X + 4), Y + 175);
  tft.println("1");                                                 // display S = 1 scale text, 1 S point = 14 pixels
  tft.setCursor((X + 4), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 11), Y + 175);
  tft.println("2");                                                 // display S = 2 scale text white
  tft.setCursor((X + 11), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 28), Y + 175);
  tft.println("3");                                                 // display S = 3 scale text white
  tft.setCursor((X + 28), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 48), Y + 175);
  tft.println("4");                                                 // display S = 4 scale text white
  tft.setCursor((X + 48), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 65), Y + 175);
  tft.println("5");                                                 // display S = 5 scale text white
  tft.setCursor((X + 65), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 80), Y + 175);
  tft.println("6");                                                 // display S = 6 scale text white
  tft.setCursor((X + 80), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 95), Y + 175);
  tft.println("7");                                                 // display S = 7 scale text white
  tft.setCursor((X + 95), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 113), Y + 175);
  tft.println("8");                                                 // display S = 8 scale text white
  tft.setCursor((X + 113), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 131), Y + 175);
  tft.println("9");                                                 // display S = 9 scale text white
  tft.setCursor((X + 131), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setTextColor(ILI9341_RED);                                    // set textcolor to red
  tft.setCursor((X + 170), Y + 175);
  tft.println("20");                                                // display S = 20 scale text red
  tft.setCursor((X + 170), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 207), Y + 175);
  tft.println("40");                                                // display S = 40 scale text red
  tft.setCursor((X + 207), Y + 185);
  tft.println(".");                                                 // display S = dot scale
  tft.setCursor((X + 240), Y + 175);
  tft.println("60");                                                // display S = 60 scale text red
  tft.setCursor((X + 250), Y + 185);
  tft.println(".");                                                 // display S = dot scale
}

void draw_PO_meter() {

  //Draw PO meter, the PO meter can be substituted by the Main S meter

  tft.fillRect((X - 44), (Y + 171), (X + 244), 21, ILI9341_BLACK); // background of meter to black
  tft.drawRect((X - 1), (Y + 170), 257, 23, ILI9341_WHITE);        // draw rectangular around the bar
  tft.drawRect((X - 45), (Y + 170), 44, 23, ILI9341_WHITE);        // draw rectangular around the text
  tft.setCursor(Z, (Y + 178));
  tft.setTextColor(ILI9341_BLUE);                                  // set textcolor to blue
  tft.println("PO");                                               // print meter text
  tft.setCursor((X + 4), Y + 175);
  tft.println("0");                                                 // display PO = 0 scale text, power 0W
  tft.setCursor((X + 32), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 5W
  tft.setCursor((X + 51), Y + 175);
  tft.println("10");                                                // display PO = 10 scale text

  tft.setCursor((X + 82), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 20W
  tft.setCursor((X + 104), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 30W
  tft.setCursor((X + 128), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 40W
  tft.setCursor((X + 147), Y + 175);
  tft.println("50");                                                 // display PO = 50W scale text
  tft.setCursor((X + 159), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 60W
  tft.setCursor((X + 169), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 70W
  tft.setCursor((X + 183), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 80W
  tft.setCursor((X + 193), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 90W
  tft.setCursor((X + 194), Y + 175);
  tft.println("100");                                               // display PO = 100W scale text
  tft.setTextColor(ILI9341_RED);
  tft.setCursor((X + 200), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 100W RED
  tft.setCursor((X + 54), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 10W
  tft.setCursor((X + 147), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 50W
  tft.setCursor((X + 248), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 150W
  tft.setTextColor(ILI9341_BLUE);
  tft.setCursor((X + 208), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 110W
  tft.setCursor((X + 218), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 120W
  tft.setCursor((X + 228), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 130W
  tft.setCursor((X + 238), Y + 185);
  tft.println(".");                                                 // display PO = dot scale 140W
  tft.setCursor((X + 248), Y + 175);
  tft.println("W");                                                 // display scale text W
}


void Tune() {

  // this routine is called to check if tune button is pressed, if so tune

  val = digitalRead(switchPin);                           // read Tune button and store it in val
  delaycounter = delaycounter + 1;                        // increment delaycounter
  if (delaycounter == 3) {                                // when counter has done 3 loops, keep loop speed, non blocking delay
    delaycounter = 0;                                     // reset counter
    val2 = digitalRead(switchPin);                          // read Tune button again for bounces
    if (val == val2) {                                      // when no bounce
      if (val == HIGH) {                                    // the Tune button is pressed, and no bounce
        if (buttonpress == false) {                         // input and output CAT commands only once when button pressed (it is in loop)
          Serial.print("PC;");                              // send the request Power Command to radio, call send routine
          get_radio_response();                             // receive the present (power) setting from radio
          CAT_buffer = CAT_buffer + ";";                    // add ";" to buffer, terminator is needed for restore command send to radio
          prevpwr = CAT_buffer;                             // prevpwr holds original power setting to restore later
          Serial.print("MD0;");                             // send the request Mode Command to radio, call send routine
          get_radio_response();                             // receive the present (mode) setting from radio
          CAT_buffer = CAT_buffer + ";";                    // add ";" to buffer, terminator is needed for restore command send to radio
          prevmode = CAT_buffer;                            // prevmode holds original mode setting to restore later
          if (tune == true) {                               // valid power/mode info is received from radio, we can set tune mode & power now
            Serial.print("MD0B;");                          // set mode to FM-N, call send routine
            Serial.print(set_tune_pwr);                     // send the tune power to the radio
            delay(35);                                      // wait before starting to transmit
            Serial.print("MX1;");                           // MOX on, transmitting, now do your tuning
            tune = false;                                   // tuning flag reset, be ready for next button press.
          }
        }
        buttonpress = true;                                 // Tune button was pressed and now we
        buttonrelease = true;                               // do Tune button release
      }
      if ((val == LOW) && (buttonrelease == true)) {          // now the Tune button has been released
        Serial.print("MX0;");                                 // MOX off, stop transmitting, stop tune signal
        delay(35);                                            // wait until TX is switched off before sending CAT commands.
        Serial.print(prevmode);                               // restore original mode setting to the radio, mode first then power
        Serial.print(prevpwr);                                // restore original power setting to the radio
        buttonrelease = false;                                // reset flag, Tune button was released
        buttonpress = false;                                  // reset flag, Tune button not pressed anymore
      }
    }
  }
}
