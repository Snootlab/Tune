/*
 * MP3 tag print test for Tune shield by Snootlab
 * Copyleft Snootlab 2015
 */

// Library needed
#include <Tune.h>
#include <SdFat.h>

// Object declaration
Tune player;

// Arrays to stock tag frames
char title[30];
char artist[30];
char album[30];

void setup()
{
  // Shield initialization 
  player.begin();
  player.setVolume(170);
  
  // Start serial comunication
  Serial.begin(9600);
  
  // Select the track you want to play
  player.play("YourSong.mp3");
  
  // Get the tags and print them on serial monitor
  Serial.print("Currently playing : ");
  player.getTrackTitle((char*)&title);
  Serial.write((byte*)&title, 30);
  Serial.print(" by ");
  player.getTrackArtist((char*)&artist);
  Serial.write((byte*)&artist, 30);
  Serial.print(" - Album : ");
  player.getTrackAlbum((char*)&album);
  Serial.write((byte*)&album, 30);
  Serial.println();
}

void loop()
{
}
