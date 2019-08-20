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

#ifndef play_sd_wav_h_
#define play_sd_wav_h_

#include "Arduino.h"
#include "AudioStream.h"

#ifdef USE_SdFat_
#include "SdFat.h"
#else
#include "SD.h"
#endif

class AudioPlaySdWav : public AudioStream
{
public:
	AudioPlaySdWav(void) : AudioStream(0, NULL), dblock_left(NULL), dblock_right(NULL) { begin(); }
	void begin(void);
	bool play(const char *filename);
	bool play(const char *filename, float my_tone_incr);
	
	// https://github.com/zueblin/Polaron/blob/master/Software/Polaron/AudioPlayPitchedMemory.h
	//    void frequency(float t_freq) {
    //    //scales a value of 0.0 - 1024.0 to 0.125 - 4.0
    //    tone_incr = (t_freq) * (4.0 - 0.125) / (1024.0) + 0.125;
    //}
    
	void stop(void);
	bool isPlaying(void);
	bool pitch( float my_tone_incr );
	uint32_t positionMillis(void);
	uint32_t lengthMillis(void);
	virtual void update(void);
private:
	File wavfile;
	bool consume(uint32_t fsize);
	bool parse_format(void);
	uint32_t fheader[10];		// temporary storage of wav header data
	uint32_t fdata_length;		// number of bytes remaining in current section
	uint32_t ftotal_length;		// number of audio data bytes in file
	uint32_t bytes2millis;
	audio_block_t *dblock_left;
	audio_block_t *dblock_right;
	uint16_t dblock_offset;		// how much data is in block_left & block_right
	uint8_t  fbuffer[2048];		// 512 buffer one block of data
	uint16_t fbuffer_offset;		// where we're at consuming "buffer"
	uint16_t fbuffer_length;		// how much data is in "buffer" (512 until last read)

	// uint16_t buffer_length_out; // output-buffer 512
	// uint8_t buffer_out[512];    // output-buffer one block of data

	uint8_t lsb, msb, lsbr, msbr; // left and right Bytes
	 
	 
	uint8_t fheader_offset;		// number of bytes in header[]
	uint8_t state;
	uint8_t state_play;
	uint8_t leftover_bytes;
	
	float tone_incr;
    float sampleIndex;
    uint16_t sampleIndexBase;
};

#endif
