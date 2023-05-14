# Yaesu-FTdx-101-External-meters-and-tune-button
How to build an Arduino driven external meter display with a tune button for a Yaesu FTdx-101

See also You Tube: https://youtu.be/S7MPvK0BMc8

Note: this sketch was made for an FTdx-101D. When used on a FTdx-101MP, the power meter will not show the correct power output since it has double the power. You will have to adjust the scale (-numbers) in the .ino file.

This is a design of an Arduino (Nano) with a 2.8“ TFT acting as a simultaneous (bargraph-)display of all 9 meters available in the FTdx-101.

It reads all meter settings from the radio via CAT, through the Rs232 connector. 
The display will also show what power has been set in the radio. Besides that, it has a (momentary) pushbutton that can be used to tune the radio with 20 Watts (adjustable in .ino file).
The following data are displayed:

- SWR
- Comp
- Temp
- IDD
- VDD
- ALC
- S Main - is substituted by Power out when in TX,  S Main is set to OFF when receiver is off.
- S Sub  -  is set to OFF when receiver is off.
-Power out - instead of S Main, when in TX
 
- Present power setting
- Connection status

At tune button press, it stores the present power setting and the present mode. It will then engage an FM-N transmission. 
When released, the tuning signal will stop. The present power setting is restored after the button has been released. The tune power can be adapted in the sketch with the variable; set_tune_pwr "PC020;" The 020 means 20W. You can change it to anything between 005 and 100. 
