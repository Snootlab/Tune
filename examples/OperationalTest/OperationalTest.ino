/*
 * Operational test for Tune shield by Snootlab
 * Copyleft Snootlab 2014
 */
 
// Library needed
#include <Tune.h>

// Object declaration
Tune player;

void setup()
{
  // Shield initialization
  player.begin();
  
  /*  Read/Write VS1011e test  */
  
  // Register checking (prints on serial monitor, hence the Serial.begin())
  Serial.begin(9600);
  player.checkRegisters();
  // Bass setting
  player.setBass(50, 5);
  // Checking again to see if changes have been taken in account
  player.checkRegisters();
  // SCI_BASS (Reg 6) should be 0x0 then 0x4035
  
  /* Audio output */
  
  // Mute left channel
  player.setVolume(0, 200);
  // Sine test @ 1 KHz
  player.sineTest(STD);
  // Mute right channel
  player.setVolume(200, 0);
  // Sine test @ 1 KHz
  player.sineTest(STD);
  delay(1000);
  
  // Enable both audio outputs
  player.setVolume(200);
  // Play an MP3 file (change track name to the one you want)
  // Comment line if no SD card is inserted in the shield 
  player.play("Song.mp3");
}

void loop()
{
  // nothing happens after setup
}
