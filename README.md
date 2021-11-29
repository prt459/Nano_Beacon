# Micro_Beacon
A simple, general purpose multiband CW beacon for home-based receiver testing.

Designed to be powered up and run whilst receiver or antenna tests are being done. 
It provides a reliable off-air signal that can be attenuated by moving the beacon further away
or by placing it in a screened box or location.  

Targets an Arduino Nano, Uno or bare ATMega328, and an si5351 breakout board.
No display is necessary.  You can add one, and controls, if you like.  

The code includes a simple CW keyer for manual sending (not used but left in place for this application). 

This beacon transmits a hard-coded message in morse code on any frequency supported by the si5351 (10kHz to 160MHz).  
Any number of frequencies in the HF and VHF range can be specified by adding them to an array. 
The beacon iterates over the array and transmits the message on each frequency in sequence.  

- Beacon speed is configurable. 
- Sidetone is available as a 5v square wave on D7.
- No support for switched low pass filters but this is easy to add.  

29 Nov 2021 by Paul VK3HN.  
