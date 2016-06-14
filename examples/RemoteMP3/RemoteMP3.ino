/*
 * IR test for Tune shield by Snootlab
 * Copyleft Snootlab 2015
 */

#include <IRremote.h>
// TSOP 32138 > D3
#define RECV_PIN 3

// Definition of the codes 
// Remote can be found at http://snootlab.com/composants/175-telecommande-infrarouge.html
#define POWER 0x00FFA25D
#define MENU  0x00FFE21D
#define TEST  0x00FF22DD
#define PLUS  0x00FF02FD
#define BACK  0x00FFC23D
#define PREV  0x00FFE01F
#define PLAY  0x00FFA857
#define NEXT  0x00FF906F
#define ZERO  0x00FF6897
#define LESS  0x00FF9867
#define C     0x00FFB04F
#define ONE   0x00FF30CF
#define TWO   0x00FF18E7
#define THREE 0x00FF7A85
#define FOUR  0x00FF10EF
#define FIVE  0x00FF38C7
#define SIX   0x00FF5AA5
#define SEVEN 0x00FF42BD
#define EIGHT 0x00FF4AB5
#define NINE  0x00FF52AD

// Variables needed for IR reception
IRrecv irrecv(RECV_PIN);
decode_results results;

// Tune library inclusion and object declaration
#include <Tune.h>
#include <SdFat.h>
Tune player;

// Variable to store the current volume value
int volume = 180;

void setup()
{
  irrecv.enableIRIn(); // Start receiver
  
  // Start serial communication (needed for register check)
  Serial.begin(9600);
  
  // Initialize shield
  player.begin();
  player.setVolume(volume);
}

void loop() 
{
  // If IR message is received
  if (irrecv.decode(&results))
  {
    // do corresponding action
    switch (results.value)
    {
      // Check all registers
      case MENU : player.checkRegisters(); break;
      // Sine test @ 1 KHz
      case TEST : player.sineTest(); break;
      // Set volume up or down
      case PLUS : volume += 5; player.setVolume(volume); break;
      case LESS : volume -= 5; player.setVolume(volume); break;
      // Play track according to given number
      case ONE : player.playTrack(1); break;
      case TWO : player.playTrack(2); break;
      case THREE : player.playTrack(3); break;
      case FOUR : player.playTrack(4); break;
      case FIVE : player.playTrack(5); break;
      case SIX : player.playTrack(6); break;
      case SEVEN : player.playTrack(7); break;
      case EIGHT : player.playTrack(8); break;
      case NINE : player.playTrack(9); break;
      // Play another track (change track name to fit the one you want)
      case PLAY : player.play("YourSong.mp3"); irrecv.resume(); break;
      // exit loop
      default : break;
    }
    irrecv.resume();
  }
}

