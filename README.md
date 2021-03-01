# ricino

What's the code does?:
It's a highly modified code from Ricino-Lap-Counter emitter/receiver (add github ref). For the moment the receiver code should be compatible with Zround.
and if you're not happy with the UI of zround, test this very simple WebUI :)

Next version of the API may/could/should be incompatible with current Zround/ricino API communication. Why... so much technical issues (why 5sec ping when racing, 2sec to make a connection verification?, no check alive status, no options (like relay/light/sound) directly integred in the receiver and controllable with zround, etc...).

Why so much code and headache?
Because I haven't found open-source lap counter with open-source hardware working on open-source OS.. in 2019... neither in 2021.
Seriouly, I have more than 20 rc cars, so put a commercial product (ir emitter) on each will cost me to much money, and the software will work only on windows/android product.
So first, I decided to try ricino, but code was (sorry...) (hum...) like trash ... but almost working and a good base for me :).
Zround was the only option, but I haven't any Windows at home, and Qemu/vmware each time was no more cool in 2019. Only plenty of BSD/Linux servers, so my first technical review was running a web server on docker with REMI (python web framework). But as I have plenty of ESP8266 and ESP01 keeping all around, start ESP WEBUI. Maybe change to ESP32 or samd21+ESP01 later...

Quick wiki:
1) Put Ir emitter on each RC car (mini-z for my case).
2) connect IR receive LED into a simple Arduino (uno/micro/uno/mega/nano), I have tested only with a simple arduino nano etc...
3) connect an esp8266 wemos mini (or check 3.3V<->5v) (webui code) to the arduino nano(receiver code) for serial connection.
4) connect to 192.168.4.1 to configure your AP wifi (maybe add an option to let the esp be the AP...)
5) connect you prefered recent/updated browser (firefox/brave/chromium/chrome/edge/safari/etc....).
6) Test/debug/enjoy that Lap counter!

To do:
- Add manual/wiki
- bug cleaning
- sexier HTML/CSS ! because world need pretty interface!
- waiting a little bit before removing/changing ugly but working algo/functions ...
- add record (store in eeprom/sd card/sql db)
- add setup (racer parameter/photo/color/name...)
- add solo mode
- add voice ... in javascript... or develop another UI with QT ... ?, or just use the buzzer with cool sound, or use a simple speaker connected to another arduino? need to thinking about that...
- break the code to change maximum player, maybe 8 player or even 16players is possible without much break (except WebUI), check free ram available on the ESP side, and cycle time for sort...



FIY, the debug mode work for me, not tested with real emitter for the moment.
