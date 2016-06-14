/**
	Tune.h - Library for Tune - MP3 Decoder Shield by Snootlab
	Coded by Laetitia Hardy-Dessources
	Inspired by SFEMP3Shield library by Bill Porter
	
	2016.06.14 - v1.3 - Rework again, improving track browsing management
	
	Changelog : added playPlaylist() to play tracks named for use with playTrack()
					  getNbTracks() so user can know how many playable files are available
					  playNext() & playPrev(), self-explanatory
				modif begin() to include listing of files for playlisting
	NOTE : now all .mp3 files HAVE TO be formatted EXACTLY 8.3
				> i.e. "MySong00.mp3" is valid, "MySong.mp3" isn't
				
	Licence CC-BY-SA 3.0
*/

#ifndef Tune_h
#define Tune_h

#if (ARDUINO >= 100)
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif

#include <SPI.h>
#include <SdFat.h>

/* Pin configuration */

#define DREQ 2
#define XDCS 4
#define XCS  8
#define SDCS 10

/* SCI registers */

#define SCI_MODE        0x00
#define SCI_STATUS      0x01
#define SCI_BASS        0x02
#define SCI_CLOCKF      0x03
#define SCI_DECODE_TIME 0x04
#define SCI_AUDATA      0x05
#define SCI_WRAM        0x06
#define SCI_WRAMADDR    0x07
#define SCI_HDAT0       0x08
#define SCI_HDAT1       0x09
#define SCI_AIADDR      0x0A
#define SCI_VOL         0x0B
#define SCI_AICTRL0     0x0C
#define SCI_AICTRL1     0x0D
#define SCI_AICTRL2     0x0E
#define SCI_AICTRL3     0x0F

/* SCI_MODE bits */

#define SM_DIFF_B             0
#define SM_LAYER12_B          1 
#define SM_RESET_B            2
#define SM_OUTOFWAV_B         3 
#define SM_TESTS_B            5
#define SM_STREAM_B           6 
#define SM_DACT_B             8
#define SM_SDIORD_B           9
#define SM_SDISHARE_B        10
#define SM_SDINEW_B          11

#define SM_DIFF           (1<< 0)
#define SM_LAYER12        (1<< 1) 
#define SM_RESET          (1<< 2)
#define SM_OUTOFWAV       (1<< 3) 
#define SM_TESTS          (1<< 5)
#define SM_STREAM         (1<< 6) 
#define SM_DACT           (1<< 8)
#define SM_SDIORD         (1<< 9)
#define SM_SDISHARE       (1<<10)
#define SM_SDINEW         (1<<11)

#define SM_ICONF_BITS 2
#define SM_ICONF_MASK 0x00c0

#define SM_EARSPEAKER_1103_BITS 2
#define SM_EARSPEAKER_1103_MASK 0x3000

/* SCI_STATUS bits */

#define SS_AVOL_B           0 
#define SS_APDOWN1_B        2
#define SS_APDOWN2_B        3
#define SS_VER_B            4

#define SS_AVOL          (1<< 0) 
#define SS_APDOWN1       (1<< 2)
#define SS_APDOWN2       (1<< 3)
#define SS_VER           (1<< 4)

#define SS_SWING_BITS     3
#define SS_SWING_MASK     0x7000
#define SS_VER_BITS       4
#define SS_VER_MASK       0x00f0
#define SS_AVOL_BITS      2
#define SS_AVOL_MASK      0x0003

#define SS_VER_VS1001 0x00
#define SS_VER_VS1011 0x10
#define SS_VER_VS1002 0x20
#define SS_VER_VS1003 0x30
#define SS_VER_VS1053 0x40
#define SS_VER_VS8053 0x40
#define SS_VER_VS1033 0x50
#define SS_VER_VS1063 0x60
#define SS_VER_VS1103 0x70

/* SCI_BASS bits */

#define ST_AMPLITUDE_B 12
#define ST_FREQLIMIT_B  8
#define SB_AMPLITUDE_B  4
#define SB_FREQLIMIT_B  0

#define ST_AMPLITUDE (1<<12)
#define ST_FREQLIMIT (1<< 8)
#define SB_AMPLITUDE (1<< 4)
#define SB_FREQLIMIT (1<< 0)

#define ST_AMPLITUDE_BITS 4
#define ST_AMPLITUDE_MASK 0xf000
#define ST_FREQLIMIT_BITS 4
#define ST_FREQLIMIT_MASK 0x0f00
#define SB_AMPLITUDE_BITS 4
#define SB_AMPLITUDE_MASK 0x00f0
#define SB_FREQLIMIT_BITS 4
#define SB_FREQLIMIT_MASK 0x000f

/* SCI_CLOCKF bits */

#define SC_MULT_BITS 3
#define SC_MULT_MASK 0xe000
#define SC_ADD_BITS 2
#define SC_ADD_MASK  0x1800
#define SC_FREQ_BITS 11
#define SC_FREQ_MASK 0x07ff

/* This macro will automatically set the clock doubler if XTALI < 16 MHz */
#define HZ_TO_SCI_CLOCKF(hz) ((((hz)<16000000)?0x8000:0)+((hz)+1000)/2000)

/* SCI_WRAMADDR bits */

#define SCI_WRAM_X_START	0x0000
#define SCI_WRAM_Y_START	0x4000
#define SCI_WRAM_I_START	0x8000
#define SCI_WRAM_IO_START	0xC000

#define SCI_WRAM_X_OFFSET	0x0000
#define SCI_WRAM_Y_OFFSET	0x4000
#define SCI_WRAM_I_OFFSET	0x8000
#define SCI_WRAM_IO_OFFSET	0x0000 /* I/O addresses are @0xC000 -> no offset */

/* SCI_VOL bits */

#define SV_LEFT_B	8
#define SV_RIGHT_B	0

#define SV_LEFT  (1<<8)
#define SV_RIGHT (1<<0)

#define SV_LEFT_BITS  8
#define SV_LEFT_MASK  0xFF00
#define SV_RIGHT_BITS 8
#define SV_RIGHT_MASK 0x00FF

/* Intruction codes */

#define VS_READ 0x03
#define VS_WRITE 0x02

/* Sine test frequencies */

#define LOS 0x61  // LOweSt   @ 86.13 Hz
#define STD1 0x24 // STanDard @ 1 KHz
#define STD2 0x28 // STanDard @ 2 KHz
#define STD3 0x18 // STanDard @ 3 KHz
#define HIS 0x1F  // HIgheSt  @ 5.625 KHz

/* Tune state while running */

static unsigned int playState;
#define idle		0
#define playback	1
#define pause		2

/* ID3v1 tag offsets */

#define TITLE   0
#define ARTIST 30
#define ALBUM  60

/* Tag search variables */

static unsigned char pb[4];  // pointer to the last 4 characters we read in
static unsigned char c;      // next character to be read
static unsigned long length; // total length of the tag (without header)

static unsigned char tagFrame;
static char tab[5]; 		 // array to stock frame identifier

extern SdFat sd;
static unsigned int nb_track;

class Tune
{
	public : 
		bool begin();
		unsigned int readSCI(byte registerAddress);
		void writeSCI(byte registerAddress, byte highbyte, byte lowbyte);
		void writeSCI(byte registerAddress, unsigned int data);
		void writeSDI(byte data);
		void checkRegisters();
		void setVolume(byte leftChannel, byte rightChannel);
		void setVolume(byte volume);
		void setBass(unsigned int bassAmp, unsigned int bassFreq);
		void setTreble(unsigned int trebAmp, unsigned int trebFreq);
		void sineTest(int freq = STD1);
		void setBit(byte regAddress, unsigned int bitAddress);
		void clearBit(byte regAddress, unsigned int bitAddress);
		int play(char* trackName);
		int playTrack(unsigned int trackNo);
		void playPlaylist(int start, int end);
		void playNext();
		void playPrev();
		unsigned int getNbTracks();
		int isPlaying();
		int getState();
		void getTrackInfo(unsigned char frame, char* infobuffer);
		void getTrackTitle(char* infobuffer);
		void getTrackArtist(char* infobuffer);
		void getTrackAlbum(char* infobuffer);
		void pauseMusic();
		void resumeMusic();
		bool stopTrack();
		
		
	private : 
		static SdFile track;
		static byte buffer[32];
		static void csLow();
		static void csHigh();
		static void dcsLow();
		static void dcsHigh();
		char** tracklist;
		int listFiles();
		bool isMP3(char* filename);
		void skipTag();
		int getID3v1(unsigned char offset, char* infobuffer);
		int getID3v1Title(char* infobuffer);
		int getID3v1Artist(char* infobuffer);
		int getID3v1Album(char* infobuffer);
		int getID3v2(char* infobuffer);
		void findv22(char* infobuffer);
		void findv23(char* infobuffer);
		static void feed();
		static void sendZeros();
		// TODO : byte LoadUserCode(char*); int readWRAM(int); void writeWRAM(int, int);
};

#endif