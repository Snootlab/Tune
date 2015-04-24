/**
	Tune.h - Library for Tune - MP3 Decoder Shield by Snootlab
	Coded by Laetitia Hardy-Dessources
	Inspired by SFEMP3Shield library by Bill Porter
	
	2015.04.24 - v1.1
	This version is based on Bill Greiman's SdFat lib instead of the Arduino IDE's SD lib.
	
	Licence CC-BY-SA 3.0
*/

#if (ARDUINO >= 100)
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif

#include <Tune.h>
#include <SdFat.h>

SdFat sd;
SdFile Tune::track;
unsigned char Tune::buffer[32];

/** 
	Initializes the shield : SPI & SD setup, reset of the VS1011e & clock setting
*/

void Tune::begin(void)
{
	// initializes used pins
	pinMode(DREQ, INPUT);
	pinMode(XCS, OUTPUT);
	pinMode(XDCS, OUTPUT);
	pinMode(SDCS, OUTPUT);
	
	// Pull-up resistor
	digitalWrite(DREQ, HIGH);
	// Deselect control & data ctrl
	digitalWrite(XCS, HIGH);
	digitalWrite(XDCS, HIGH);
	// Deselect SD's chip select
	digitalWrite(SDCS, HIGH);
	
	// Initialize SD card
	if (!sd.begin(SDCS, SPI_HALF_SPEED))
	{
		sd.initErrorHalt(); // stay there if there's a problem
		while(1);
	}
	
	SPI.begin();
	SPI.setDataMode(SPI_MODE0);
	// Both SCI and SDI read data MSB first
	SPI.setBitOrder(MSBFIRST);
	// From the datasheet, max SPI reads are CLKI/6. Here CLKI = 26MHz -> SPI max speed is 4.33MHz.
	// We'll take 16MHz/4 = 4MHz to be safe.
	// Plus it's the max speed advised when using a resistor-based lvl converter with an SD card.
	SPI.setClockDivider(SPI_CLOCK_DIV4);
	SPI.transfer(0xFF);

	delay(10);

	// Software reset
	Tune::writeSCI(SCI_MODE, 0x08, 0x04);
	delay(5);
	
	// Clock setting (default is 24.576MHz)
	Tune::writeSCI(SCI_CLOCKF, 0x32, 0xC8);
	delay(5);
	
	// Wait until the chip is ready
	while (!digitalRead(DREQ));
	delay(100);
	
	Serial.println("Tune ready !");
}

/**
	Reads from an SCI register
*/

unsigned int Tune::readSCI(unsigned char registerAddress)
{
	unsigned char hiByte, loByte;
	
	// DREQ high <-> VS1011 available
	while (!digitalRead(DREQ));
	// Deselect SD's chip select
	digitalWrite(SDCS, HIGH);
	// Deselect data ctrl & select control
	digitalWrite(XDCS, HIGH);
	digitalWrite(XCS, LOW);
  
	// Read instruction
	SPI.transfer(0x03);
	SPI.transfer(registerAddress);
	// MSB first
	hiByte = SPI.transfer(0x00); 
	loByte = SPI.transfer(0x00);
	
	while (!digitalRead(DREQ)); // wait 'til cmd is complete
  
	// Deselect control
	digitalWrite(XCS, HIGH);
  
	unsigned int response = word(hiByte, loByte);
	
	return response;
}

/**
	Writes to an SCI register
*/

void Tune::writeSCI(unsigned char registerAddress, unsigned char highbyte, unsigned char lowbyte)
{
	// DREQ high <-> VS1011 available
	while (!digitalRead(DREQ));
	// Deselect SD's chip select
	digitalWrite(SDCS, HIGH);
	// Deselect data ctrl & select control
	digitalWrite(XDCS, HIGH);
	digitalWrite(XCS, LOW);

	// Write instruction
	SPI.transfer(0x02);
	SPI.transfer(registerAddress);
	// MSB first
	SPI.transfer(highbyte); 
	SPI.transfer(lowbyte);
	
    // wait til cmd is complete
	while (!digitalRead(DREQ));
	
	// Deselect control
	digitalWrite(XCS, HIGH);
}

/**
	Feeds SDI data to the codec
*/

void Tune::writeSDI(unsigned char data)
{
	while (!digitalRead(DREQ));
    // Deselect SD's chip select
	digitalWrite(SDCS, HIGH);
	// Deselect ctrl & select data control
	digitalWrite(XCS, HIGH);
	digitalWrite(XDCS, LOW);

	SPI.transfer(data);
	
    // Deselect data ctrl
    digitalWrite(XDCS, HIGH);
}

/**
	Checks all SCI registers and prints their value (HEX) on serial monitor
	(see header file for register definitions)
*/

void Tune::checkRegisters(void)
{
	for (int i=0; i<16; i++)
	{
		Serial.print("Reg ");
		Serial.print(i);
		Serial.print(" = 0x");
		Serial.println(Tune::readSCI(i), HEX);
	}
}

/**
	Sets the volume, 2 separate channels
	For each channel, a value in the range of 0 to 254 may be defined to set its attenuation from the
	maximum volume level (in 0.5 dB steps). Thus max volume is 0 and minimum is 254.
	Reverse logic being more natural, here the user sets the volume from 0 (total silence) to 254 (max)
*/

void Tune::setVolume(char leftChannel, char rightChannel)
{
	// Convert values into proper register entries
	char leftAtt = 254 - leftChannel;
	char rightAtt = 254 - rightChannel;
	// Write that down to the register
	Tune::writeSCI(SCI_VOL, leftAtt, rightAtt);
}

/**
	Sets the volume, same level on both channels
	Same logic as above
*/

void Tune::setVolume(char volume) 
{
	Tune::setVolume(volume, volume);
}

/**
	Activates sine test (beeps @ given frequency for 2 secs)
	LOS @ 86.13Hz - STD @ 1KHz - HIS @ 11.625KHz
	See datasheet p.37 for other values and how they're calculated
*/

void Tune::sineTest(int freq)
{
	// Arrays to stock the sine wave test begin & end command
	byte sine[8] = {0x53, 0xEF, 0x6E, freq, 0x00, 0x00, 0x00, 0x00};	// see datasheet
	byte endSine[8] = {0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00};
	
	// Enable SDI tests
	Tune::writeSCI(SCI_MODE, 0x08, 0x20);
	
	// Activate sine test
	for (int i=0; i<8; i++)
	{
		Tune::writeSDI(sine[i]);
	}
	delay(2000);

	// Deactivate sine test
	for (int j=0; j<8; j++)
	{
		Tune::writeSDI(endSine[j]);
	}

	// Disable SDI tests
	Tune::writeSCI(SCI_MODE, 0x08, 0x00);
}

/** 
	Plays a track, given the name formatted 8.3
	http://en.wikipedia.org/wiki/8.3_filename
*/

int Tune::play(const char* trackName)
{	
	// skip if already playing a track
	if (isPlaying()) return 1;
	
	// exit if track not found
	if (!track.open(trackName, O_READ))
	{
		sd.errorHalt("Track not found !");
		return 2;
	}
	
	// set playState flag
	playState = playback;
	
	// Reset decode time & bitrate from previous playback
	Tune::writeSCI(SCI_DECODE_TIME, 0, 0);
	delay(100);
	
	// skip ID3v2 tag if there's one
	Tune::skipTag();
	
	// feed VS1011e
	Tune::feed();
	// let the interrupt handle the rest of the process
	attachInterrupt(0, feed, RISING);

	// return value if everything went ok
	return 0;
}


/** 
	Plays a track with the name formatted as "trackXXX.mp3"
	Where "XXX" is a number between 0 and 999.
*/

int Tune::playTrack(int trackNo)
{
	// storage place for track titles
	char songName[] = "track000.mp3";
	// print track number onto the file name
	sprintf(songName, "track%03d.mp3", trackNo);
	// play given track
	return Tune::play(songName);
}

/** 
	Plays a playlist defined by an iteration of tracks with names formatted as above
	Also handles track numbers from 0 to 999
*/

void Tune::playPlaylist(int startNo, int endNo)
{
	for (int z=startNo; z<=endNo; z++)
	{
		Tune::playTrack(z);
	}
}

/**
	Sets the bass : bassAmp is in dB (0 to 15) and bassFreq is in 10Hz steps (2 to 15)
	Frequencies below bassFreq will be amplified.
*/

void Tune::setBass(unsigned int bassAmp, unsigned int bassFreq)
{
	// Store new bass setting in a single variable
	unsigned int BASS = ((bassAmp << 4) & 0x00F0) + (bassFreq & 0x000F);
	  // Read and store current register value
	unsigned int oldBassReg = Tune::readSCI(SCI_BASS);
	// Set new register value
	unsigned int newBassReg = (oldBassReg & 0xFF00) + BASS;
	// Send new value
	Tune::writeSCI(SCI_BASS, highByte(newBassReg), lowByte(newBassReg));
}

/** 
	Sets the treble : trebAmp is in 1.5dB steps (-8 to 7) and trebFreq is in KHz (0 to 15)
	Frequencies above trebFreq will be amplified.
*/

void Tune::setTreble(unsigned int trebAmp, unsigned int trebFreq)
{// Store new treble setting in a single variable
	unsigned int TREB = ((trebAmp << 12) & 0xF000) + ((trebFreq << 8) & 0x0F00);
	// Read and store current register value
	unsigned int oldTrebReg = Tune::readSCI(SCI_BASS);
	// Set new register value
	unsigned int newTrebReg = (oldTrebReg & 0x00FF) + TREB;
	// Send new value
	Tune::writeSCI(SCI_BASS, highByte(newTrebReg), lowByte(newTrebReg));
}

/**
	Gets a tag from the current track, specified by constants TITLE, ARTIST or ALBUM
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackInfo(unsigned char frame, char* infobuffer)
{
	if (!Tune::getID3v1(frame, infobuffer))
	{
		if (!Tune::getID3v2(infobuffer))
		{
			return;
		}
	}
}

/**
	Gets the title tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackTitle(char* infobuffer)
{
	if (!Tune::getID3v1Title(infobuffer))
	{
		if (!Tune::getID3v2(infobuffer))
		{
			return;
		}
	}
}

/**
	Gets the artist tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackArtist(char* infobuffer)
{
	if (!Tune::getID3v1Artist(infobuffer))
	{
		if (!Tune::getID3v2(infobuffer))
		{
			return;
		}
	}
}

/**
	Gets the album tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackAlbum(char* infobuffer)
{
	if (!Tune::getID3v1Album(infobuffer))
	{
		if (!Tune::getID3v2(infobuffer))
		{
			return;
		}
	}
}

/**
	Pauses data stream
*/

void Tune::pauseMusic()
{
	if (playState == playback)
	{
		detachInterrupt(0);
		playState = pause;
	}
}

/**
	Resumes data stream
*/

void Tune::resumeMusic()
{
	if (playState == pause)
	{
		Tune::feed();
		playState = playback;
		attachInterrupt(0, feed, RISING);
	}
}

/**
	Stops current track and cancels interrupt - allows next track to be played
*/

void Tune::stopTrack()
{
	// Skip if not already playing
	if ((playState != pause) && (playState != playback)) 
	{
		return;
	}
	
	detachInterrupt(0);
	playState = idle;
	
	track.close();
}

/**
	Tells the loop if the Tune is currently playing a file (even if paused)
*/

int Tune::isPlaying()
{
	int result;
	
	if (getState() == playback)
	{
		result = 1;
	}
	else if (getState() == pause)
	{
		result = 1;
	}
	else
	{
		result = 0;
	}
	return result;
}

/**
	Returns current state of the Tune shield, i.e. "idle", "playing" or "paused"
*/

int Tune::getState()
{
	return playState;
}

/** 
	Searches for an ID3v2 tag and skips it so there's no delay for playback
*/

void Tune::skipTag()
{
	unsigned char id3[3]; // pointer to the first 3 characters we read in
		
	track.seekSet(0);
	track.read(id3, 3);
	
	// if the first 3 characters are ID3 then we have an ID3v2 tag
	// we now need to find the length of the whole tag
	
	if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3')
	{
		unsigned char pb[4];  // pointer to the last 4 characters we read in
		// skip 3 bytes we don't need and read the last 4 bytes of the header
		// that contain the tag's length
		
		track.read(pb, 3);
		track.read(pb, 4);
	
		// to combine these 4 bytes together into a single value, we have to
		// shift one other to get it into its correct position	
		// a quirk of the spec is that the MSb of each byte is set to 0
		unsigned long v21 =  (((unsigned long) pb[0] << (7*3)) + ((unsigned long) pb[1] << (7*2)) + ((unsigned long) pb[2] << (7)) + pb[3]);
		
		// go to end of tag
		track.seekSet(v21);
		return;
	}
	else
	{
		// if there's no tag, get back to start and playback
		track.seekSet(0);
		return;
	}
}

/** 
	Gets an ID3v1 tag from a track
*/

int Tune::getID3v1(unsigned char offset, char* infobuffer)
{
	// pause track if already playing
	if (playState == playback)
	{
		detachInterrupt(0);
	}
	// save current position for later
	unsigned long currentPosition = track.curPosition();
	
	// save the value corresponding to the needed frame for later 
	tagFrame = offset;
	
	unsigned char ID3v1[3]; // array to store the 'TAG' identifier of ID3v1 tags
		
	// skip to end of file
	unsigned long tagPosition = track.fileSize();
	
	// skip to tag start
	tagPosition -= 128;
	track.seekSet(tagPosition);
	
	// read the first 3 characters available
	track.read(ID3v1, 3);
	
	// if they're 'TAG'
	if (ID3v1[0] == 'T' && ID3v1[1] == 'A' && ID3v1[2] == 'G')
	{
		// we found an ID3v1 tag !
		// let's go find the frame we're looking for
		tagPosition = (track.curPosition() + offset);
		track.seekSet(tagPosition);
		
		// read the tag and store it
		track.read(infobuffer, 30);
		
		//go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (playState == playback)
		{
			attachInterrupt(0, feed, RISING);
		}
		return 1;
	}
	// if they're not
	else
	{
		//go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (playState == playback)
		{
			attachInterrupt(0, feed, RISING);
		}
		return 0;
	}
}

/** 
	Gets the ID3v1 title tag from a track
*/

int Tune::getID3v1Title(char* infobuffer)
{
	if (Tune::getID3v1(TITLE, infobuffer))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/** 
	Gets the ID3v1 artist tag from a track
*/

int Tune::getID3v1Artist(char* infobuffer)
{
	if (Tune::getID3v1(ARTIST, infobuffer))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/** 
	Gets the ID3v1 album tag from a track
*/

int Tune::getID3v1Album(char* infobuffer)
{
	if (Tune::getID3v1(ALBUM, infobuffer))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
	Routine to find an ID3v2 tag, following action depending on version found
	v2.2 and 2.3 supported ; exits loop if no tag found
*/

int Tune::getID3v2(char* infobuffer)
{  
	// pause track if already playing
	if (playState == playback)
	{
		detachInterrupt(0);
	}
	// save current position for later
	unsigned long currentPosition = track.curPosition();

	unsigned char ID3v2[3]; // pointer to the first 3 characters we read in
		
	track.seekSet(0);
	track.read(ID3v2, 3);
		
	// if the first 3 characters are ID3 then we have an ID3v2 tag
	// we now need to find the length of the whole tag
	if (ID3v2[0] == 'I' && ID3v2[1] == 'D' && ID3v2[2] == '3')
	{  
		// Read the version of the tag
		track.read(pb, 1);
		int tagVersion = pb[0];
			
		// skip 2 bytes we don't need and read the last 4 bytes of the header
		// that contain the tag's length
		track.read(pb, 2);
		track.read(pb, 4);
			
		// to combine these 4 bytes together into a single value, we have to
		// shift one other to get it into its correct position	
		// a quirk of the spec is that the MSb of each byte is set to 0
		length =  (((unsigned long) pb[0] << (7*3)) + ((unsigned long) pb[1] << (7*2)) + ((unsigned long) pb[2] << (7)) + pb[3]);
		
		if (tagVersion ==2)
		{
			Tune::findv22(infobuffer);
			//go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (playState == playback)
			{
				attachInterrupt(0, feed, RISING);
			}
			return 2;
		}
		else if (tagVersion == 3)
		{
			Tune::findv23(infobuffer);
			//go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (playState == playback)
			{
				attachInterrupt(0, feed, RISING);
			}
			return 3;
		}
		else
		{
			//go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (playState == playback)
			{
				attachInterrupt(0, feed, RISING);
			}
			return 9;
		}
	}
	else
	{
		//go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (playState == playback)
		{
			attachInterrupt(0, feed, RISING);
		}
		return 0;
	}
}

/** 
	Routine to find an ID3v2.2 tag
*/

void Tune::findv22(char* infobuffer)
{	
	switch (tagFrame)
	{
		case TITLE :
			tab[0] = '2'; tab[1] = 'T'; tab[2] = 'T'; break;
		case ARTIST : 
			tab[0] = '1'; tab[1] = 'P'; tab[2] = 'T'; break;
		case ALBUM : 
			tab[0] = 'L'; tab[1] = 'A'; tab[2] = 'T'; break;
		default : 
			break;
	}
		
	while (1)
	{
		track.read(&c, 1);
		
		// shift over previously read byte so we can search for an identifier
		pb[3] = pb[2];
		pb[2] = pb[1];
		pb[1] = pb[0];
		pb[0] = c;
					
		if (pb[2] == tab[2] && pb[1] == tab[1] && pb[0] == tab[0])
		{
			// found an id3v2.2 tag frame ! 
			// frame's length is in the next 3 bytes
			// but we read 4 bytes then skip the last which is text encoding
			track.read(pb, 4);
				  
			// we have to combine these bytes together into a single value
			unsigned long tl = ((unsigned long) pb[0] << (8 * 2)) + ((unsigned long) pb[1] << (8 * 1)) + pb[2]; 
			tl--; // because text encoding is included in frame size
				
			// start reading the actual tag and store it
			track.read(infobuffer, tl);
			break;
		}
	}
}

/** 
	Routine to find an ID3v2.3 tag
*/

void Tune::findv23(char* infobuffer)
{		
	switch (tagFrame)
	{
		case TITLE :
			tab[0] = '2'; tab[1] = 'T'; tab[2] = 'I'; tab[3] = 'T'; break;
		case ARTIST : 
			tab[0] = '1'; tab[1] = 'E'; tab[2] = 'P'; tab[3] = 'T'; break;
		case ALBUM : 
			tab[0] = 'B'; tab[1] = 'L'; tab[2] = 'A'; tab[3] = 'T'; break;
		default : 
			break;
	}
		
	while (1)
	{
		track.read(&c, 1);
			
		// shift over previously read byte so we can search for an identifier
		pb[3] = pb[2];
		pb[2] = pb[1];
		pb[1] = pb[0];
		pb[0] = c;
					
		if (pb[3] == tab[3] && pb[2] == tab[2] && pb[1] == tab[1] && pb[0] == tab[0])
		{
			// found an id3v2.3 tag frame ! 
			// frame's length is in the next 4 bytes
			track.read(pb, 4);
				  
			// we have to combine these bytes together into a single value
			unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) + ((unsigned long) pb[1] << (8 * 2)) + ((unsigned long) pb[2] << (8 * 1)) + pb[3]; 

			// skip two bytes we don't need
			track.read(pb, 2);
	  
			// start reading the actual tag and store it
			track.read(infobuffer, tl);
			break;
		}
	}
}

/** 
	Feeds MP3 encoded data to the codec
*/

void Tune::feed()
{
	while (digitalRead(DREQ))
	{
		//Go out to SD card and try reading 32 new bytes of the track
		if (!track.read(Tune::buffer, sizeof(Tune::buffer)))
		{
			track.close();
			playState = idle;	
			detachInterrupt(0);
			break;
		}
		// Deselect SD's chip select
		digitalWrite(SDCS, HIGH);
		// Deselect ctrl & select data control
		digitalWrite(XCS, HIGH);
		digitalWrite(XDCS, LOW);
		
		// Feed the chip
		for (int j=0; j<32; j++)
		{
			SPI.transfer(Tune::buffer[j]);
		}
		// Deselect data control
		digitalWrite(XDCS, HIGH);	
	}
}