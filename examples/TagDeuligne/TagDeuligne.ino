/*
 * LCD tag print test for Tune shield by Snootlab
 * This sketch is provided for use with Deuligne shield by Snootlab
 * Copyleft Snootlab 2015
 */

// Libraries needed
#include <Tune.h>
#include <SdFat.h>
#include <Wire.h>
#include <Deuligne.h>

// Objects declaration
Tune player;
Deuligne lcd;

// Arrays to stock tag frames
char title[30];
char artist[30];
char album[30];

void setup()
{
  // Tune shield initialization
  player.begin();
  player.setVolume(200);
  
  // Deuligne shield initialization
  Wire.begin();
  lcd.init();
  
  // Screen setting
  lcd.clear();
  lcd.backLight(true);
  lcd.print("MP3 Player ready");
  
  delay(1000);
  
  // Select the track you want to play
  player.play("Thievery.mp3");
}

void loop()
{
  // Repeat during playback
  while (1)
  {
    // Get the tags and print them on the LCD
    
    // Clear the screen and announce frame
    lcd.clear();
    lcd.print("Track : ");
    // Actually get tag frame
    player.getTrackTitle((char*)&title);
    // Go to second line
    lcd.setCursor(0, 1);
    // Print the tag and wait a bit
    lcd.print((char*)&title);
    delay(2000);
    
    // Same for next frames
    lcd.clear();
    lcd.print("By :");
    player.getTrackArtist((char*)&artist);
    lcd.setCursor(0, 1);
    lcd.print((char*)&artist);
    delay(2000);
    
    lcd.clear();
    lcd.print("Album : ");
    player.getTrackAlbum((char*)&album);
    lcd.setCursor(0, 1);
    lcd.print((char*)&album);
    delay(2000);
  }
}
