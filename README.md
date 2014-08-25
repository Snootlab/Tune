# Tune - MP3 decoder shield for Arduino

Copyleft Snootlab 2014

This library allows playing MP3 files from the SD card of the Tune shield simply by telling their name.
It also simplifies volume, bass and treble setting, and MP3 tag searching.


# Basic use

* Declare shield object like this : 
Tune player;

* Initialize player & set volume :
player.begin();
player.setVolume(200);

* Play file : 
player.play("myTrack.mp3");


See _forum.snootlab.com_ > _Tune_ for detailed explanations on the methods used and other examples.
