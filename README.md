# teensystuff
Collected Stuff around Teensy


This is my temporary storage to provide access to my example how to pitch up or down a sample-file from a SD Card with Teensy 3.6.

I have tested it with Teensiduino 1.47 and Arduino 1.8.9 on a MAC.
It works only with the SDFat-Extension!
If You need it for the normal Library which is included in the Teensy Audio-Lib, 
then You have to compare both versions and change it in Ypur prefered version.

The current version could only pitch files in the format WAV 16Bit Stereo 44.1KHz!!! No other Formats are supported.
.. I have developed it today and only tested a bit today.

Sources for SDFat and Teensy:
SDFAT for Audio WAV-Player
https://forum.pjrc.com/threads/55180-long-file-names-and-play-audio-from-SD-card
https://www.muffwiggler.com/forum/viewtopic.php?t=118378&postdays=0&postorder=asc&start=375&sid=42f5d15096c5dc4013798c8ee8942c79

Another solution to pitch WAV from ProgMemory
https://github.com/zueblin/Polaron/blob/master/Software/Polaron/AudioPlayPitchedMemory.cpp

Some others which were some kind of inspiration
Teensy Based Sample-Player
https://github.com/cutlasses?tab=repositories
Sample Player with extra AudioLibraries
https://github.com/TomWhitwell/RadioMusic/tree/master/RadioMusic

I did comperable things before with the ESP32 .. check my other Repos...
