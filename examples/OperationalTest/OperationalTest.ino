/*
 * Operational test for Tune shield by Snootlab
 * Copyleft Snootlab 2015
 *
 * Circuit : Arduino Uno/Mega, Tune shield 
 *  + pushbutton on D5 with 10k pull-down resistor
 * Code : reads & writes VS1011e codec registers
 *  + tests audio output with sine test & actual song
 *  + tests pause/resume ability
 */
 
// Libraries needed
#include <Tune.h>
#include <SdFat.h>

// Object declaration
Tune player;

// Variables for pause/resume routine
const int pushButton = 5;
unsigned int counter = 0;
boolean state = LOW;
boolean prevState = LOW;

void setup()
{
  /* Initialization */
  
  Serial.begin(9600);
  
  player.begin();
  
  pinMode(pushButton, INPUT);
  
  /*  Read/Write VS1011e test  */
  
  // Register checking (prints on serial monitor, hence the Serial.begin())
  player.checkRegisters();
  // Bass setting
  player.setBass(5, 5);
  // Checking again to see if changes have been taken in account
  Serial.println("New register values :");
  player.checkRegisters();
  // SCI_BASS (Reg 6) should be 0x0 then 0x4035
  
  /* Audio output */
  
  // Mute left channel
  player.setVolume(0, 180);
  // Sine test @ 1 KHz
  player.sineTest();
  // Mute right channel
  player.setVolume(180, 0);
  // Sine test @ 1 KHz
  player.sineTest();
  delay(1000);
  
  // Enable both audio outputs
  player.setVolume(180);
  // Play an MP3 file (change track name to the one you want)
  // Comment line if no SD card is inserted in the shield 
  player.play("MySong.mp3");
}

/* The loop checks if someone pressed the button to pause/resume the music */

void loop()
{
  // Read and store current button state
  state = digitalRead(pushButton);
  
  // Compare to its previous state :
  // If the state is different than the last time we checked...
  if (state != prevState)
  {
    // ...and it's HIGH then the button has been pressed
    if (state == HIGH)
    {
      // Increment counter
      counter++;
    }
  }
  // Save current state for next loop
  prevState = state;
  
  // Turn on music if it has been pressed 0, 2, 4... times
  if (counter % 2 == 0)
  {
    player.resumeMusic();
  }
  // Or turn off music if it has been pressed 1, 3, 5... times
  else
  {
    player.pauseMusic();
  }
}
