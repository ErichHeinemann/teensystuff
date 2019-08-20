/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define USE_SdFat_ // uncomment for SdFat.h, comment for SD.h lib.

#include <Arduino.h>
#include "play_sd_wav.h"
#include "spi_interrupt.h"

#define STATE_DIRECT_8BIT_MONO		0  // playing mono at native sample rate
#define STATE_DIRECT_8BIT_STEREO	1  // playing stereo at native sample rate
#define STATE_DIRECT_16BIT_MONO		2  // playing mono at native sample rate
#define STATE_DIRECT_16BIT_STEREO	3  // playing stereo at native sample rate
#define STATE_CONVERT_8BIT_MONO		4  // playing mono, converting sample rate
#define STATE_CONVERT_8BIT_STEREO	5  // playing stereo, converting sample rate
#define STATE_CONVERT_16BIT_MONO	6  // playing mono, converting sample rate
#define STATE_CONVERT_16BIT_STEREO	7  // playing stereo, converting sample rate
#define STATE_PARSE1			8  // looking for 20 byte ID header
#define STATE_PARSE2			9  // looking for 16 byte format header
#define STATE_PARSE3			10 // looking for 8 byte data header
#define STATE_PARSE4			11 // ignoring unknown chunk after "fmt "
#define STATE_PARSE5			12 // ignoring unknown chunk before "fmt "
#define STATE_STOP			13

void AudioPlaySdWav::begin(void)
{
	state = STATE_STOP;
	state_play = STATE_STOP;
	fdata_length = 0;
	if (dblock_left) {
		release(dblock_left);
		dblock_left = NULL;
	}
	if (dblock_right) {
		release(dblock_right);
		dblock_right = NULL;
	}
}

// Better Midinote or Pitch than t_freq
bool AudioPlaySdWav::play( const char *filename ){
 // float tone_incr = 1.0; 
 return play( filename ,1.0 );
}


bool AudioPlaySdWav::play( const char *filename, float my_tone_incr )
{
    tone_incr = my_tone_incr;
	stop();
#if defined(HAS_KINETIS_SDHC)	
	if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStartUsingSPI();
#else 	
	AudioStartUsingSPI();
#endif
	__disable_irq();
#ifdef USE_SdFat_
    wavfile.open(filename); // SdFat.h
#else
    wavfile = SD.open(filename); // SD.h
#endif
	__enable_irq();
	if (!wavfile) {
	#if defined(HAS_KINETIS_SDHC)	
		if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
	#else 	
		AudioStopUsingSPI();
	#endif			
		return false;
	}
	fbuffer_length = 0;
	fbuffer_offset = 0;
	state_play = STATE_STOP;
	fdata_length = 20;
	fheader_offset = 0;
	state = STATE_PARSE1;
	return true;
}



void AudioPlaySdWav::stop(void)
{
	__disable_irq();
	if (state != STATE_STOP) {

		audio_block_t *b1 = dblock_left;
		dblock_left = NULL;
		
		audio_block_t *b2 = dblock_right;
		dblock_right = NULL;
		state = STATE_STOP;
		__enable_irq();
		if (b1) release(b1);
		if (b2) release(b2);
		wavfile.close();
	#if defined(HAS_KINETIS_SDHC)	
		if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
	#else 	
		AudioStopUsingSPI();
	#endif	
	} else {
		__enable_irq();
	}
}




void AudioPlaySdWav::update(void)
{
	int32_t n;

	// only update if we're playing
	if (state == STATE_STOP) return;

	// allocate the audio blocks to transmit
	dblock_left = allocate();
	
	
	if (dblock_left == NULL) return;
	if (state < 8 && (state & 1) == 1) {
		// if we're playing stereo, allocate another
		// block for the right channel output
		dblock_right = allocate();
		if (dblock_right == NULL) {
			release(dblock_left);
			return;
		}
	} else {
		// if we're playing mono or just parsing
		// the WAV file header, no right-side block
		dblock_right = NULL;
	}
	dblock_offset = 0;

	//Serial.println("update");

	// is there buffered data?
	n = fbuffer_length - fbuffer_offset;
	if (n > 0) {
		// we have buffered data
		if( consume(n) ) return; // it was enough to transmit audio
	}

	// we only get to this point when buffer[512] is empty
	if (state != STATE_STOP && wavfile.available()) {
		// we can read more data from the file...
		
		// Daten werden in buffer temporaer abgelegt...
		readagain:
		// bufferlength is the amount of bytes from sample or is simply 0 or 1
		fbuffer_length = wavfile.read(fbuffer, 2048 ); // (int)(256 * tone_incr) *2
		if (fbuffer_length == 0) goto end;
		fbuffer_offset = 0;
		bool parsing = (state >= 8);
		
		bool txok = consume(fbuffer_length);
		if (txok) {
			if (state != STATE_STOP) return;
		} else {
			if (state != STATE_STOP) {
				if (parsing && state < 8) goto readagain;
				else goto cleanup;
			}
		}
		
	}
end:	// end of file reached or other reason to stop
	wavfile.close();
#if defined(HAS_KINETIS_SDHC)	
	if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
#else 	
	AudioStopUsingSPI();
#endif	
	state_play = STATE_STOP;
	state = STATE_STOP;
	
cleanup:
	if( dblock_left ) {
		if( dblock_offset > 0 ) {
			for (uint32_t i=dblock_offset; i < AUDIO_BLOCK_SAMPLES; i++) {
				dblock_left->data[i] = (0 << 8); // 0;
			}
			transmit(dblock_left, 0);
			if (state < 8 && (state & 1) == 0) {
				transmit(dblock_left, 1);
			}
		}
		release(dblock_left);
		dblock_left = NULL;
	}
	if (dblock_right) {
		if (dblock_offset > 0) {
			for (uint32_t i=dblock_offset; i < AUDIO_BLOCK_SAMPLES; i++) {
				dblock_right->data[i] = (0 << 8); // // 0;
			}
			transmit(dblock_right, 1);
		}
		release(dblock_right);
		dblock_right = NULL;
	}
}




// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
// Consume already buffered data.  Returns true if audio transmitted.
bool AudioPlaySdWav::consume(uint32_t fsize)
{
    unsigned int i, indexInt;
	uint32_t len;
	// uint8_t lsb=0, msb=0, lsbr=0, msbr=0; // left and right
	const uint8_t *p;

	p = fbuffer + fbuffer_offset;

start:
	if (fsize == 0) return false;

#if 0
    Serial.print( "tone_incr: " );
    Serial.println( tone_incr );
#endif

#if 0
	Serial.print( "consume, ");
	Serial.print( "size: ");
	Serial.print( fsize);
	Serial.print( ", buffer_offset: ");
	Serial.print( fbuffer_offset);
	Serial.print( ", data_length: ");
	Serial.print( fdata_length);
	Serial.print( ", space: ");
	Serial.print( ( AUDIO_BLOCK_SAMPLES - dblock_offset ) * 2 );
	//Serial.print( ", state: " );
	//Serial.println( state );
#endif


	switch (state) {

	  // parse wav file header, is this really a .wav file?
	  case STATE_PARSE1:
#if 1
    Serial.print( "State1 parse - tone_incr: " );
    Serial.println( tone_incr );
#endif	  
	    sampleIndexBase = 0;
		sampleIndex=0;
	    lsb=0;
	    msb=0;
	    lsbr=0; 
	    msbr=0; // left and right

	  
		len = fdata_length;
		if (fsize < len) len = fsize;
		memcpy((uint8_t *)fheader + fheader_offset, p, len);
		fheader_offset += len;
		fbuffer_offset += len;
		fdata_length -= len;
		if (fdata_length > 0) return false;
		// parse the header...
		if (fheader[0] == 0x46464952 && fheader[2] == 0x45564157) {
			//Serial.println("is wav file");
			if (fheader[3] == 0x20746D66) {
				// "fmt " header
				if (fheader[4] < 16) {
					// WAV "fmt " info must be at least 16 bytes
					break;
				}
				if (fheader[4] > sizeof(fheader)) {
					// if such .wav files exist, increasing the
					// size of header[] should accomodate them...
					//Serial.println("WAVEFORMATEXTENSIBLE too long");
					break;
				}
				//Serial.println("header ok");
				fheader_offset = 0;
				state = STATE_PARSE2;
			} else {
				// first chuck is something other than "fmt "
				//Serial.print("skipping \"");
				//Serial.printf("\" (%08X), ", __builtin_bswap32(header[3]));
				//Serial.print(header[4]);
				//Serial.println(" bytes");
				fheader_offset = 12;
				state = STATE_PARSE5;
			}
			p += len;
			fsize -= len;
			fdata_length = fheader[4];
			goto start;
		}
		//Serial.println("unknown WAV header");
		break;

	  // check & extract key audio parameters
	  case STATE_PARSE2:
		len = fdata_length;
		if (fsize < len) len = fsize;
		memcpy((uint8_t *)fheader + fheader_offset, p, len);
		fheader_offset += len;
		fbuffer_offset += len;
		fdata_length -= len;
		if(fdata_length > 0 ) return false;
		if( parse_format() ) {
			//Serial.println("audio format ok");
			p += len;
			fsize -= len;
			fdata_length = 8;
			fheader_offset = 0;
			state = STATE_PARSE3;
			goto start;
		}
		//Serial.println("unknown audio format");
		break;

	  // find the data chunk
	  case STATE_PARSE3: // 10
		len = fdata_length;
		if( fsize < len) len = fsize;
		memcpy((uint8_t *)fheader + fheader_offset, p, len);
		fheader_offset += len;
		fbuffer_offset += len;
		fdata_length -= len;
		if (fdata_length > 0) return false;
		//Serial.print("chunk id = ");
		//Serial.print(header[0], HEX);
		//Serial.print(", length = ");
		//Serial.println(header[1]);
		p += len;
		fsize -= len;
		fdata_length = fheader[1];
		if (fheader[0] == 0x61746164) {
			Serial.print("wav: found data chunk, len=");
			Serial.println( fdata_length );
			// TODO: verify offset in file is an even number
			// as required by WAV format.  abort if odd.  Code
			// below will depend upon this and fail if not even.
			leftover_bytes = 0;
			state = state_play;
			if (state & 1) {
				// if we're going to start stereo
				// better allocate another output block
				dblock_right = allocate();
				if (!dblock_right) return false;
			}
			ftotal_length = fdata_length;
		} else {
			state = STATE_PARSE4;
		}
		goto start;

	  // ignore any extra unknown chunks (title & artist info)
	  case STATE_PARSE4: // 11
		if (fsize < fdata_length) {
			fdata_length -= fsize;
			fbuffer_offset += fsize;
			return false;
		}
		p += fdata_length;
		fsize -= fdata_length;
		fbuffer_offset += fdata_length;
		fdata_length = 8;
		fheader_offset = 0;
		state = STATE_PARSE3;
		//Serial.println("consumed unknown chunk");
		goto start;

	  // skip past "junk" data before "fmt " header
	  case STATE_PARSE5:
		len = fdata_length;
		if (fsize < len) len = fsize;
		fbuffer_offset += len;
		fdata_length -= len;
		if (fdata_length > 0) return false;
		p += len;
		fsize -= len;
		fdata_length = 8;
		state = STATE_PARSE1;
		goto start;

	  // playing mono at native sample rate
	  case STATE_DIRECT_8BIT_MONO:
		return false;

	  // playing stereo at native sample rate
	  case STATE_DIRECT_8BIT_STEREO:
		return false;

	  // playing mono at native sample rate
	  case STATE_DIRECT_16BIT_MONO:
		if (fsize > fdata_length) fsize = fdata_length;
		fdata_length -= fsize;
		
		lsb = 0;
		msb = 0;
		while (1) {
		    // Erich Heinemann
		    // mit *p++ wird vermmutlich der naechste Datensatz aus dem Array gezogen ...
		    // wenn die naechsten 2 Zuordnungen bei jedem zweiten Mal ausfallen,
		    //  dann wird das WAV langsamer bzw. bei 50% abgespielt.
		    // wenn die naechsten 2 Zuordnungen zweimal durchlaufen werden, dann kann man aus beiden Werten a
		    // einen Mittelwert berechnen und diesen Zuordnen.
		    //  if( tone_incr != 1.0 ){
		      sampleIndex += tone_incr;		    
              indexInt = (int)sampleIndex;
		      // loope so lange, bis sampleIndexBase genauso groß ist wie indexInt ist
              for( i = sampleIndexBase; i < indexInt; i++) { 
              
  		        // if( data_length > 2 ){
			      lsb = *p++;
			      msb = *p++;
			      fsize -= 2;
			    //}
               
              }
              if( indexInt > 5000 ){
                sampleIndex = sampleIndex - indexInt;
                indexInt = 0;                 
              }
              sampleIndexBase = indexInt;
              
              
           // }else{
           //   lsb = *p++;
		   //   msb = *p++;
		   //   size -= 2;            
           // }
            // bei 0.5:0  -> 1 Sample, 1:1 -> kein Sample , 1.5 
            
			dblock_left->data[dblock_offset++] = (msb << 8) | lsb;
			if (dblock_offset >= AUDIO_BLOCK_SAMPLES) {
				transmit( dblock_left, 0 );
				transmit( dblock_left, 1 );
				release( dblock_left );
				dblock_left = NULL;
				fdata_length += fsize;
				fbuffer_offset = p - fbuffer;
				if( dblock_right ) release( dblock_right );
				if( fdata_length == 0 ) state = STATE_STOP;
				return true;
			}
			if( fsize == 0 ) {
				if( fdata_length == 0 ) break;
				return false;
			}
		}
		//Serial.println("end of file reached");
		// end of file reached
		if (dblock_offset > 0) {
			// TODO: fill remainder of last block with zero and transmit
		}
		state = STATE_STOP;
		return false;

	  // playing stereo at native sample rate
	  case STATE_DIRECT_16BIT_STEREO:
	  /*
		msb = 128;
		lsb = 0;	  
		msbr = 128;
		lsbr = 0;
	  */
		if (fsize > fdata_length) fsize = fdata_length;
		fdata_length -= fsize;
		// if (leftover_bytes) {
		//	dblock_left->data[dblock_offset] = fheader[0];
//PAH fix problem with left+right channels being swapped
		//	goto right16;
		// }
		while (1) {
		
	      sampleIndex += tone_incr;		     
	      indexInt = (int)sampleIndex;
	      
	      // E.Heinemann
	      // loop while  sampleIndexBase is smaller than the indexInt
	       if( tone_incr >= 1.0 ){  
             for( i = sampleIndexBase; i < indexInt; i++) { 
               // left
               if(fsize > 0){
  			     lsb = *p++;
			     msb = *p++;
			     fsize -= 2;
			   }  
               // right
               if(fsize > 0){
			     lsbr = *p++;
			     msbr = *p++;
			     fsize -= 2;
			   } 
               sampleIndexBase +=1;
		     }	
		   }
		   
		   // Slow down the Sample
	       if( tone_incr < 1.0 ){  
	         /*
	         if( sampleIndexBase == 0 ) {
	         // msb==0 && msbr==0){
               if(fsize > 0){
  			     lsb = *p++;
			     msb = *p++;
			     fsize -= 2;
			   }  
               // right
               if(fsize > 0){
			     lsbr = *p++;
			     msbr = *p++;
			     fsize -= 2;
			   }
               sampleIndexBase += 1;
	         }else{
	        */
	       
               if( indexInt == 0 || indexInt > sampleIndexBase  ) { 
                 // left
                 if(fsize > 0){
  			       lsb = *p++;
			       msb = *p++;
			       fsize -= 2;
			     }  
                 // right
                 if(fsize > 0){
			       lsbr = *p++;
			       msbr = *p++;
			       fsize -= 2;
			     } 
			     sampleIndexBase = indexInt;

		       // }
		     } 
 	
		   }		  
		  
            if(indexInt > 5000){
              sampleIndex = sampleIndex - indexInt;
              indexInt = 0;      
              sampleIndexBase = indexInt;           
            }
                        
            // sampleIndexBase = indexInt;
			
			//right16:
			//if (1==2 && leftover_bytes && fsize>0){
			//  leftover_bytes = 0;
			//  lsbr = *p++;
			//  msbr = *p++;
			//  fsize -= 2;
			//}
			dblock_left->data[dblock_offset] = (msb << 8) | lsb;
			dblock_right->data[dblock_offset++] = (msbr << 8) | lsbr;


			// if (fsize == 0) {
			//	if( fdata_length == 0 ) break;
			//	fheader[0] = (msb << 8) | lsb;
			//	leftover_bytes = 2;
			//	return false;
			//}
			
			if( dblock_offset >= AUDIO_BLOCK_SAMPLES) {
			
				transmit( dblock_left, 0 );
				release( dblock_left );
				dblock_left = NULL;
				
				transmit(dblock_right, 1);
				release(dblock_right);
				dblock_right = NULL;
				
				fdata_length += fsize;
				fbuffer_offset = p - fbuffer;
				if( fdata_length == 0 ) state = STATE_STOP;
				return true;
			}
			if (fsize == 0) {
				if( fdata_length == 0) break;
				leftover_bytes = 0;
				return false;
			}
		}
		// end of file reached
		if( dblock_offset > 0) {
			// TODO: fill remainder of last block with zero and transmit
		}
		state = STATE_STOP;
		return false;

	  // playing mono, converting sample rate
	  case STATE_CONVERT_8BIT_MONO :
		return false;

	  // playing stereo, converting sample rate
	  case STATE_CONVERT_8BIT_STEREO:
		return false;

	  // playing mono, converting sample rate
	  case STATE_CONVERT_16BIT_MONO:
		return false;

	  // playing stereo, converting sample rate
	  case STATE_CONVERT_16BIT_STEREO:
		return false;

	  // ignore any extra data after playing
	  // or anything following any error
	  case STATE_STOP:
		return false;

	  // this is not supposed to happen!
	  //default:
	  //Serial.println("AudioPlaySdWav, unknown state");
	}
	state_play = STATE_STOP;
	state = STATE_STOP;
	return false;
}


/*
00000000  52494646 66EA6903 57415645 666D7420  RIFFf.i.WAVEfmt 
00000010  10000000 01000200 44AC0000 10B10200  ........D.......
00000020  04001000 4C495354 3A000000 494E464F  ....LIST:...INFO
00000030  494E414D 14000000 49205761 6E742054  INAM....I Want T
00000040  6F20436F 6D65204F 76657200 49415254  o Come Over.IART
00000050  12000000 4D656C69 73736120 45746865  ....Melissa Ethe
00000060  72696467 65006461 746100EA 69030100  ridge.data..i...
00000070  FEFF0300 FCFF0400 FDFF0200 0000FEFF  ................
00000080  0300FDFF 0200FFFF 00000100 FEFF0300  ................
00000090  FDFF0300 FDFF0200 FFFF0100 0000FFFF  ................
*/





// SD library on Teensy3 at 96 MHz
//  256 byte chunks, speed is 443272 bytes/sec
//  512 byte chunks, speed is 468023 bytes/sec

#define B2M_44100 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT) // 97352592
#define B2M_22050 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 2.0)
#define B2M_11025 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 4.0)

bool AudioPlaySdWav::parse_format(void)
{
	uint8_t num = 0;
	uint16_t format;
	uint16_t channels;
	uint32_t rate, b2m;
	uint16_t bits;

	format = fheader[0];
	//Serial.print("  format = ");
	//Serial.println(format);
	if (format != 1) return false;

	rate = fheader[1];
	//Serial.print("  rate = ");
	//Serial.println(rate);
	if (rate == 44100) {
		b2m = B2M_44100;
	} else if (rate == 22050) {
		b2m = B2M_22050;
		num |= 4;
	} else if (rate == 11025) {
		b2m = B2M_11025;
		num |= 4;
	} else {
		return false;
	}

	channels = fheader[0] >> 16;
	Serial.print("  channels = ");
	Serial.println(channels);
	if (channels == 1) {
	} else if (channels == 2) {
		b2m >>= 1;
		num |= 1;
	} else {
		return false;
	}

	bits = fheader[3] >> 16;
	Serial.print("  bits = ");
	Serial.println(bits);
	if (bits == 8) {
	} else if (bits == 16) {
		b2m >>= 1;
		num |= 2;
	} else {
		return false;
	}

	bytes2millis = b2m;
	Serial.print("  bytes2millis = ");
	Serial.println( b2m );

	// we're not checking the byte rate and block align fields
	// if they're not the expected values, all we could do is
	// return false.  Do any real wav files have unexpected
	// values in these other fields?
	state_play = num;
	return true;
}


bool AudioPlaySdWav::isPlaying(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	return (s < 8);
}

bool AudioPlaySdWav::pitch( float my_tone_incr ){
   if( my_tone_incr>=0.25 && my_tone_incr <= 4.0){
     return false;
   }
   tone_incr = my_tone_incr;
   return true;
}

uint32_t AudioPlaySdWav::positionMillis(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	if (s >= 8) return 0;
	uint32_t tlength = *(volatile uint32_t *)&ftotal_length;
	uint32_t dlength = *(volatile uint32_t *)&fdata_length;
	uint32_t offset = tlength - dlength;
	uint32_t b2m = *(volatile uint32_t *)&bytes2millis;//  / tone_incr;
	return ((uint64_t)offset * b2m) >> 32;
}


uint32_t AudioPlaySdWav::lengthMillis(void)
{
	uint8_t s = *(volatile uint8_t *)&state;
	if (s >= 8) return 0;
	uint32_t tlength = *(volatile uint32_t *)&ftotal_length;// / tone_incr;
	uint32_t b2m = *(volatile uint32_t *)&bytes2millis;
	return ((uint64_t)tlength * b2m) >> 32;
}
