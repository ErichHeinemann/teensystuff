#define USE_SdFat_ // uncomment for SdFat.h, comment for SD.h lib.
// NOTE: uncomment~comment also in play_sd_raw.cpp & play_sd_wav.cpp


#ifdef USE_SdFat_
#include <SdFat.h>
 SdFatSdioEX SD; // SdFatSdioEX uses extended multi-block transfers without DMA.
 // SdFatSdio SD; // SdFatSdio uses a traditional DMA SDIO implementation.
// Note the difference is speed and busy yield time.
// Teensy 3.5 & 3.6 SDIO: 4 to 5 times the throughput using the 4-bit SDIO mode compared to the 1-bit SPI mode
#else
#include <SD.h>
#endif
#include <SerialFlash.h>


#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

SdFat sd;
File root;
File entry;

AudioPlaySdWav           playWav1;
AudioOutputUSB           audioOutput; // must set Tools > USB Type to Audio
// AudioOutputAnalog        dac;
AudioOutputI2S           dac;           //xy=815,198
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioConnection          patchCord3(playWav1, 0, dac, 0);
AudioConnection          patchCord4(playWav1, 1, dac, 1);

// Use these with the Teensy Audio Shield
//#define SDCARD_CS_PIN    10
//#define SDCARD_MOSI_PIN  7
//#define SDCARD_SCK_PIN   14

////////// For Sd.h lib. only /////////
// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

// Use these for the SD+Wiz820 or other adaptors
//#define SDCARD_CS_PIN    4
//#define SDCARD_MOSI_PIN  11
//#define SDCARD_SCK_PIN   13

elapsedMillis since_Update;
int SEC = 0;

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(20);

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);


#ifdef USE_SdFat_
  if (!(SD.begin())) { //<///////  SdFat.h  /////////
    delay(500);
    Serial.println(" SdFat.h ");
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
#else                 //<///////   Sd.h   /////////
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
#endif

#if defined(SdFat_h)
  Serial.print( "SdFat version "), Serial.println( SD_FAT_VERSION ), Serial.println("");
#elif defined(__SD_H__)
  Serial.println( "SD - a slightly more friendly wrapper for sdfatlib"), Serial.println("");
#endif

#ifdef USE_SdFat_
  char filename[255]; //
  root = SD.open("/"); // ROOT
  while (true) {
    File entry = root.openNextFile();
    entry.getName( filename, sizeof(filename) );
    if( filename[0]!='.' ){
        

      if (entry.isDirectory()) {
        Serial.print("Dir.  "), Serial.println(filename);
        if( filename[0]=='l' || filename[0]=='s' ){
           Serial.println(" Loop or Played Once");
         }
      } else {
        Serial.print("      "), Serial.println(filename);
      }
      if (! entry) {
        // no more files
        Serial.println(" **nomorefiles** ");
        break;
      }
    }
  }
  entry.close();
  root.close();
#endif
}

void playFile(const char *filename, float pitch)
{

  playWav1.play( filename, pitch);
  float pitch2 = pitch;
  // A brief delay for the library read WAV info
  delay(50);
  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
    if (since_Update > 999) {
      since_Update = 0;
      pitch2 = pitch2 *2;
      playWav1.pitch( pitch2 ); // Pitch raises
      Serial.print(" Pitch: "), Serial.println( pitch2 );
      
      Serial.print(" SEC: "), Serial.println(SEC);
      
      SEC++;
    }
  }
  SEC = 0;
  
}

float pitch;

void loop() {

  if (sd.exists("/l1")){
     Serial.print("  /l1 exists     ");
  }
  if (sd.exists("l1")){
     Serial.print("  l1 exists     ");
  }
  delay(500);
  Serial.println("  Speed 1.0");
  playFile("test1.WAV", 1.0);  // since SDFAT, Filenames could include all chars
  delay(50);
  
  Serial.println("  Speed 1.0");
  playFile("test1.WAV", 1.0);  
  delay(50);
  
  Serial.println("  Speed 0.5");
  playFile("test1.WAV", 0.5); 
  delay(50);
  

  playFile("test2.wav", 1.0 );
  playFile("test2.wav", 0.9 );
  playFile("test2.wav", 0.3 );

  for (int i = 0; i <= 12; i++) {
    pitch = pow( 1.05, i ) -0.5;
    Serial.print("  Speed");
    Serial.println(pitch);
    playFile("/l0/Lorraine Jackson - Take It Where You Found It.wav", pitch );
  } 
  
  Serial.println("  Speed 1.05 ");
  playFile("test1.WAV", 1.05 ); 
  delay(5);
  
  Serial.println("  Speed 3.0   ");
  playFile("test1.WAV", 3.0);  
  delay(5);
  
  Serial.println("  Speed 4.0  ");
  playFile("test1.WAV", 4.0 );
  delay(5);
  
  Serial.println("  Speed 0.5   ");
  playFile("SDTEST1.WAV", 0.5); 
  delay(500);
  /*
  playFile("SDTEST2.WAV");
    delay(500);
    playFile("SDTEST3.WAV");
    delay(500);
    playFile("SDTEST4.WAV");
    delay(1500);*/

    
}

// A known problem occurs on Macintosh computers, where the Mac's driver
// does not seem to be able to adapt and transmit horribly distorted
// audio to Teensy after a matter of minutes.  An imperfect workaround
// can be enabled by editing usb_audio.cpp.  Find and uncomment
// "#define MACOSX_ADAPTIVE_LIMIT".  More detailed info is available here:
// https://forum.pjrc.com/threads/34855-Distorted-audio-when-using-USB-input-on-Teensy-3-1?p=110392&viewfull=1#post110392
