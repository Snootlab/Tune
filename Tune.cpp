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

#if (ARDUINO >= 100)
	#include <Arduino.h>
#else
	#include <WProgram.h>
#endif

#include <Tune.h>
#include <SdFat.h>
#include <SPI.h>

SdFat sd;
SdFile Tune::track;
byte Tune::buffer[32];

/** 
	Initializes the shield : SPI & SD setup, reset of the VS1011e & clock setting
*/

bool Tune::begin()
{
	// Pin configuration
	pinMode(DREQ, INPUT_PULLUP);
	pinMode(XDCS, OUTPUT);
	pinMode(XCS, OUTPUT);
	pinMode(SDCS, OUTPUT);
	
	// Deselect control & data ctrl
	digitalWrite(XCS, HIGH);
	digitalWrite(XDCS, HIGH);
	// Deselect SD's chip select
	digitalWrite(SDCS, HIGH);
	
	// SD card initialization
	if (!sd.begin(SDCS, SPI_HALF_SPEED))
	{
		sd.initErrorHalt(); // describe problem if there's one
		return 0; 
	}
	
	// Tracklisting also return the number of playable files
	Serial.print(listFiles());
	Serial.print(" tracks found, ");
	
	// SPI bus initialization
	SPI.begin();
	SPI.setDataMode(SPI_MODE0);
	// Both SCI and SDI read data MSB first
	SPI.setBitOrder(MSBFIRST);
	// From the datasheet, max SPI reads are CLKI/6. Here CLKI = 26MHz -> SPI max speed is 4.33MHz.
	// We'll take 16MHz/4 = 4MHz to be safe.
	// Plus it's the max recommended speed when using a resistor-based lvl converter with an SD card.
	SPI.setClockDivider(SPI_CLOCK_DIV4);
	SPI.transfer(0xFF);
	delay(10);
	
	// Codec initialization
	// Software reset
	setBit(SCI_MODE, SM_RESET);
	delay(5);
	// VS1022 "New mode" activation
	setBit(SCI_MODE, SM_SDINEW);
	// Clock setting (default is 24.576MHz)
	writeSCI(SCI_CLOCKF, 0x32, 0xC8);
	delay(5);
	
	// Wait until the chip is ready
	while (!digitalRead(DREQ));
	delay(100);
	
	// Set playState flag
	playState = idle;
	
	// Set volume to avoid hurt ears ;)
	setVolume(150);
	
	Serial.println("Tune ready !");
	return 1;
}

/**
	Reads from an SCI register
*/

unsigned int Tune::readSCI(byte registerAddress)
{
	byte hiByte, loByte;
	
	while (!digitalRead(DREQ)); // DREQ high <-> VS1011 available
	csLow(); // Select control
  
	SPI.transfer(VS_READ); // Read instruction
	SPI.transfer(registerAddress);
	
	// MSB first
	hiByte = SPI.transfer(0x00); 
	while (!digitalRead(DREQ)); // wait 'til cmd is complete
	loByte = SPI.transfer(0x00);
	while (!digitalRead(DREQ));

	csHigh(); // Deselect control
  
	unsigned int response = word(hiByte, loByte);
	return response;
}

/**
	Writes to an SCI register
*/

void Tune::writeSCI(byte registerAddress, byte highbyte, byte lowbyte)
{
	while (!digitalRead(DREQ)); // DREQ high <-> VS1011 available
	csLow(); // Select control

	SPI.transfer(VS_WRITE); // Write instruction
	SPI.transfer(registerAddress);
	
	// MSB first
	SPI.transfer(highbyte); 
	while (!digitalRead(DREQ)); // wait 'til cmd is complete
	SPI.transfer(lowbyte);
	while (!digitalRead(DREQ));
	
	csHigh(); // Deselect control
}

/**
	Writes to an SCI register
*/

void Tune::writeSCI(byte registerAddress, unsigned int data)
{
	byte hiByte = highByte(data);
	byte loByte = lowByte(data);
	writeSCI(registerAddress, hiByte, loByte);
}

/**
	Feeds SDI data to the codec
*/

void Tune::writeSDI(byte data)
{
	while (!digitalRead(DREQ)); // DREQ high <-> VS1011 available
	dcsLow(); // Select data control

	SPI.transfer(data);
	
	dcsHigh(); // Deselect data control
}

/**
	Checks all SCI registers and prints their value (HEX) on serial monitor
	(see header file for register definitions)
*/

void Tune::checkRegisters()
{
	for (int i=0; i<16; i++)
	{
		Serial.print("Reg ");
		Serial.print(i);
		Serial.print(" = 0x");
		Serial.println(readSCI(i), HEX);
	}
}

/**
	Sets the volume, 2 separate channels
	For each channel, a value in the range of 0 to 254 may be defined to set its attenuation from the
	maximum volume level (in 0.5 dB steps). Thus max volume is 0 and minimum is 254.
	Reverse logic being more natural, here the user sets the volume from 0 (total silence) to 254 (max)
*/

void Tune::setVolume(byte leftChannel, byte rightChannel)
{
	// Convert values into proper register entries
	byte leftAtt = 254 - leftChannel;
	byte rightAtt = 254 - rightChannel;
	
	// Avoid off-range values
	constrain(leftAtt, 0, 254);
	constrain(rightAtt, 0, 254);
	
	writeSCI(SCI_VOL, leftAtt, rightAtt);
}

/**
	Sets the volume, same level on both channels
	Same logic as above
*/

void Tune::setVolume(byte volume)
{
	setVolume(volume, volume);
}

/**
	Sets the bass : bassAmp is in dB (0 to 15) and bassFreq is in 10Hz steps (2 to 15)
	Frequencies below bassFreq will be amplified.
*/

void Tune::setBass(unsigned int bassAmp, unsigned int bassFreq)
{
	// Avoid off-range values
	constrain(bassAmp, 0, 15);
	constrain(bassFreq, 2, 15);
	
	// Store new bass setting in a single variable
	unsigned int BASS = ((bassAmp << 4) & 0x00F0) + (bassFreq & 0x000F);

	unsigned int oldBassReg = readSCI(SCI_BASS); 		// Read and store current register value
	unsigned int newBassReg = (oldBassReg & 0xFF00) + BASS; // Set new register value
	
	writeSCI(SCI_BASS, highByte(newBassReg), lowByte(newBassReg));
}

/** 
	Sets the treble : trebAmp is in 1.5dB steps (-8 to 7) and trebFreq is in KHz (0 to 15)
	Frequencies above trebFreq will be amplified.
*/

void Tune::setTreble(unsigned int trebAmp, unsigned int trebFreq)
{
	// Avoid off-range values
	constrain(trebAmp, -8, 7);
	constrain(trebFreq, 0, 15);
	
	// Store new treble setting in a single variable
	unsigned int TREB = ((trebAmp << 12) & 0xF000) + ((trebFreq << 8) & 0x0F00);
	
	unsigned int oldTrebReg = readSCI(SCI_BASS); // Read and store current register value
	unsigned int newTrebReg = (oldTrebReg & 0x00FF) + TREB; // Set new register value

	writeSCI(SCI_BASS, highByte(newTrebReg), lowByte(newTrebReg));
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
	setBit(SCI_MODE, SM_TESTS);
	
	// Activate sine test
	for (int i=0; i<8; i++)
	{
		writeSDI(sine[i]);
	}
	delay(2000);

	// Deactivate sine test
	for (int j=0; j<8; j++)
	{
		writeSDI(endSine[j]);
	}
	// Disable SDI tests
	clearBit(SCI_MODE, SM_TESTS);
}

/**
	Positions a bit to '1' in a specific register
	See Tune.h & datasheet for register & bit description.
*/

void Tune::setBit(byte regAddress, unsigned int bitAddress)
{
	unsigned int value = readSCI(regAddress);
	value |= bitAddress;
  
	writeSCI(regAddress, value);
}

/**
	Positions a bit to '0' in a specific register
	See Tune.h & datasheet for register & bit description.
*/

void Tune::clearBit(byte regAddress, unsigned int bitAddress)
{
	unsigned int value = readSCI(regAddress);
	value &= ~bitAddress;
  
	writeSCI(regAddress, value);
}

/** 
	Plays a track, given the name formatted 8.3
	http://en.wikipedia.org/wiki/8.3_filename
*/

int Tune::play(char* trackName)
{
	if (isPlaying()) return 1;
	
	// Exit if track not found
	if (!track.open(trackName, O_READ))
	{
		sd.errorHalt("Track not found !");
		return 3;
	}
	
	playState = playback;
	
	// Reset decode time & bitrate from previous playback
	writeSCI(SCI_DECODE_TIME, 0);
	delay(100);
	
	skipTag(); // Skip ID3v2 tag if there's one
	
	feed(); // Feed VS1011e
	attachInterrupt(0, feed, RISING); // Let the interrupt handle the rest of the process
	
	return 0;
}

/** 
	Plays a track with the name formatted as "trackXXX.mp3"
	Where "XXX" is a number between 0 and 999.
*/

int Tune::playTrack(unsigned int trackNo)
{	
	// Storage place for track titles
	char songName[] = "track000.mp3";
	
	// Avoid off-range values
	constrain(trackNo, 0, 999);
	
	// Print track number onto the file name
	sprintf(songName, "track%03d.mp3", trackNo);
	return play(songName);
}

/** 
	Plays a combo of tracks with names formatted like above
	Caution : playback won't stop until you reached the end of the playlist
*/

void Tune::playPlaylist(int start, int end)
{
	for (int i=start; i<=end; i++)
	{
		playTrack(i);
		while(isPlaying()) delay(1); // experimentally found that the small delay made it work
	}
}

/** 
	Stops current track and plays next available track
	Loops around if it reaches the end of the tracklist
*/

void Tune::playNext()
{
	char filename[13] = "";
	int charCount = 0;
	int i;
	
	track.getName(filename, 13); // get current filename
		
	for (i=0; i<=nb_track; i++) // look in the tracklist
	{
		for (int j=0; j<13; j++)
		{
			if (filename[j] == tracklist[i][j]) charCount++;
			else break;  // move on if a single character differs
		}
		if (charCount == 13) break; // found current track in the list, exit loop
		else charCount = 0; // reset for next turn
	}
	stopTrack(); // stop current track
	
	if (i < nb_track-1) i++; // search for next track
	else i = 0; // wrap around
	
	for (int j=0; j<13; j++)
		filename[j] = tracklist[i][j];

	play(filename); // and play said file
}

/** 
	Stops current track and plays previous available track
	Loops around if it reaches the beginning of the tracklist
*/

void Tune::playPrev()
{
	char filename[13] = "";
	int charCount = 0;
	int i;
	
	track.getName(filename, 13); // get current filename
		
	for (i=0; i<=nb_track; i++) // look in the tracklist
	{
		for (int j=0; j<13; j++)
		{
			if (filename[j] == tracklist[i][j]) charCount++;
			else break; // move on if a single character differs
		}
		if (charCount == 13) break; // found current track in the list, exit loop
		else charCount = 0; // reset for next turn
	}
	stopTrack(); // stop current track
	
	if (i > 0) i--; // search for next track
	else i = nb_track-1; // wrap around
	
	for (int j=0; j<13; j++)
		filename[j] = tracklist[i][j];
	
	play(filename); // and play said file
}

/**
	Tells the loop if the Tune is currently playing a file (even if paused)
*/

int Tune::isPlaying()
{
	if (getState() == playback || getState() == pause) return 1;
	else return 0;
}

/**
	Returns current state of the Tune shield, i.e. 
	0 for "idle", 1 for "playing" or 2 for "paused"
*/

int Tune::getState()
{
	return playState;
}

/**
	Gets a tag from the current track, specified by constants TITLE, ARTIST or ALBUM
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackInfo(unsigned char frame, char* infobuffer)
{
	if (!getID3v1(frame, infobuffer))
	{
		if (!getID3v2(infobuffer)) return;
	}
}

/**
	Gets the title tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackTitle(char* infobuffer)
{
	if (!getID3v1Title(infobuffer))
	{
		if (!getID3v2(infobuffer)) return;
	}
}

/**
	Gets the artist tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackArtist(char* infobuffer)
{
	if (!getID3v1Artist(infobuffer))
	{
		if (!getID3v2(infobuffer)) return;
	}
}

/**
	Gets the album tag of the current track
	Handles ID3v1, ID3v2.2 & ID3v2.3 tags
*/

void Tune::getTrackAlbum(char* infobuffer)
{
	if (!getID3v1Album(infobuffer))
	{
		if (!getID3v2(infobuffer)) return;
	}
}

/**
	Pauses data stream, does nothing if not playing
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
	Resumes data stream, does nothing if not playing
*/

void Tune::resumeMusic()
{
	if (playState == pause)
	{
		feed();
		playState = playback;
		attachInterrupt(0, feed, RISING);
	}
}
/**
	Stops current track and cancels interrupt - allows next track to be played
*/

bool Tune::stopTrack()
{
	if (!isPlaying()) return 0; // Skip if not already playing
	
	detachInterrupt(0);
	playState = idle;
	
	if (!track.close()) return 0; // close track
	
	sendZeros(); // clear codec's buffer
	return 1;
}

/** 
	Selects SCI interface
*/

void Tune::csLow()
{
	// Make sure the other CSs are high before activating SCI
	digitalWrite(SDCS, HIGH);
	digitalWrite(XDCS, HIGH);
	digitalWrite(XCS, LOW);
}

/** 
	Deselects SCI interface
*/

void Tune::csHigh()
{
	digitalWrite(XCS, HIGH);
}

/** 
	Selects SDI interface
*/

void Tune::dcsLow()
{
	// Make sure the other CSs are high before activating SDI
	digitalWrite(SDCS, HIGH);
	digitalWrite(XCS, HIGH);
	digitalWrite(XDCS, LOW);
}

/** 
	Deselects SDI interface
*/

void Tune::dcsHigh()
{
	digitalWrite(XDCS, HIGH);
}

/** 
	Makes a list of the playable files on the SD card
	Returns how many of them are available
*/

int Tune::listFiles()
{
	nb_track = 0;
	char filename[13] = "";
	char firstfilename[13] = "";
	
	// Save the name of the first file to rewind the directory later
	track.openNext(sd.vwd(), O_READ);
	track.getName(firstfilename, 13);
	if (isMP3(firstfilename)) nb_track++;
	track.close();
	
	// open next file in root.  The volume working directory, vwd, is root
	while (track.openNext(sd.vwd(), O_READ)) 
	{
		track.getName(filename, 13);
		if (isMP3(filename)) nb_track++;
		track.close();
	}
	
	// now that we know how many available tracks we have
	// we can itinialize the 2D-array with the correct dimensions
	tracklist = (char**) malloc (nb_track*sizeof(char*));
	for (int k=0; k<nb_track; k++)
	{
		tracklist[k] = (char*) malloc(13 * sizeof(char));
	}
	
	nb_track = 0; // reset for rerun

	// rewind for first run then loop again
	track.open(firstfilename);
	track.getName(filename, 13);
	
	if (isMP3(filename)) 
	{
		for (int j=0; j<13; j++)
		{
			tracklist[nb_track][j] = filename[j]; // actually save the filename
		}
		nb_track++;
	}
	track.close();
	
	// save as above...
	while (track.openNext(sd.vwd(), O_READ))
	{
		track.getName(filename, 13);
		
		if (isMP3(filename)) 
		{
			for (int j=0; j<13; j++)
			{
				tracklist[nb_track][j] = filename[j];
			}
			nb_track++;
		}
		track.close();
	}
	return nb_track;
}

/** 
	Checks if the file is an .mp3
*/

bool Tune::isMP3(char* filename)
{
	for (int i=0; i<10; i++)
	{
		// search for file extension
		if (filename[i] == '.')
		{
			// and check it
			if( (filename[i+1] == 'm' || 'M') && (filename[i+2] == 'p' || 'P') && (filename[i+3] == '3') ) return 1; // valid file !
			else return 0;
		}
	}
	return 0;
}

/** 
	Searches for an ID3v2 tag and skips it so there's no delay for playback
*/

unsigned int Tune::getNbTracks()
{
	return nb_track;
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
		
		track.seekSet(v21); // go to end of tag
		return;
	}
	else
	{
		track.seekSet(0); // if there's no tag, get back to start and playback
		return;
	}
}

/** 
	Gets an ID3v1 tag from a track
*/

int Tune::getID3v1(unsigned char offset, char* infobuffer)
{
	// pause track if already playing
	if (isPlaying())
	{
		pauseMusic();
		playState = pause;
	}
	// save current position for later
	unsigned long currentPosition = track.curPosition();
	
	// save the value corresponding to the needed frame for later 
	tagFrame = offset;
	
	unsigned char ID3v1[3]; // array to store the 'TAG' identifier of ID3v1 tags

	unsigned long tagPosition = track.fileSize(); // skip to end of file
	
	// skip to tag start
	tagPosition -= 128;
	track.seekSet(tagPosition);
	
	track.read(ID3v1, 3); // read the first 3 characters available
	
	// if they're 'TAG'
	if (ID3v1[0] == 'T' && ID3v1[1] == 'A' && ID3v1[2] == 'G')
	{
		// we found an ID3v1 tag !
		// let's go find the frame we're looking for
		tagPosition = (track.curPosition() + offset);
		track.seekSet(tagPosition);
		
		track.read(infobuffer, 30); // read the tag and store it
		
		// go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (isPlaying())
		{
			resumeMusic();
			playState = playback;
		}
		return 1;
	}
	// if they're not
	else
	{
		// go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (isPlaying())
		{
			resumeMusic();
			playState = playback;
		}
		return 0;
	}
}

/** 
	Gets the ID3v1 title tag from a track
*/

int Tune::getID3v1Title(char* infobuffer)
{
	if (getID3v1(TITLE, infobuffer)) return 1;
	else return 0;
}

/** 
	Gets the ID3v1 artist tag from a track
*/

int Tune::getID3v1Artist(char* infobuffer)
{
	if (getID3v1(ARTIST, infobuffer)) return 1;
	else return 0;
}

/** 
	Gets the ID3v1 album tag from a track
*/

int Tune::getID3v1Album(char* infobuffer)
{
	if (getID3v1(ALBUM, infobuffer)) return 1;
	else return 0;
}

/**
	Routine to find an ID3v2 tag, following action depending on version found
	v2.2 and 2.3 supported ; exits loop if no tag found
*/

int Tune::getID3v2(char* infobuffer)
{  
	// pause track if already playing
	if (isPlaying())
	{
		pauseMusic();
		playState = pause;
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
		
		if (tagVersion == 2)
		{
			findv22(infobuffer);
			// go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (isPlaying())
			{
				resumeMusic();
				playState = playback;
			}
			return 2;
		}
		else if (tagVersion == 3)
		{
			findv23(infobuffer);
			// go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (isPlaying())
			{
				resumeMusic();
				playState = playback;
			}
			return 3;
		}
		else
		{
			// go back to where we stopped and enable playback again
			track.seekSet(currentPosition);
			if (isPlaying())
			{
				resumeMusic();
				playState = playback;
			}
			return 9;
		}
	}
	else
	{
		// go back to where we stopped and enable playback again
		track.seekSet(currentPosition);
		if (isPlaying())
		{
			resumeMusic();
			playState = playback;
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
			track.read(pb, 2); // skip two bytes we don't need
	  
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
	sei();
	while (digitalRead(DREQ))
	{
		// Go out to SD card and try reading 32 new bytes of the track
		if (!track.read(buffer, sizeof(buffer)))
		{
			// exit if end of file reached
			track.close();
			detachInterrupt(0);
			sendZeros();
			playState = idle;
			
			break;
		}
		cli();
		dcsLow(); // Select data control
		
		// Feed the chip
		for (int i=0; i<32; i++)
		{
			SPI.transfer(buffer[i]);
		}
		dcsHigh(); // Deselect data control
		sei();
	}
}

/** 
	Sends zeros to the codec to make sure nothing's left unplayed
*/

void Tune::sendZeros()
{
	dcsLow(); // Select data control
	for (int i=0; i<2052; i++)
	{
		while (!digitalRead(DREQ)); // wait until chip is ready
		SPI.transfer(0); 			// send zero	
	}
	dcsHigh(); // Deselect data control
}