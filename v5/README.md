
This version of WiFi Manager includes:

- System file editor added : provide an interface (base on ACE open source editor) to manage files stored on ESP8266 spiffs system
- Various improvement on the interface applied.


### Installation Procedures

- install node.js

- open command line to install npm golbally by running:	npm install gulp -g

- in command line change to current working project folder

- create html folder and put all required files (html, js, stylesheet, etc.) in it

- copy and put package.json in current working project folder

- copy and put gulp.js file in current working project folder

- in command line install gulp locally by running: npm install

- in command line run the gulp.js file by typing: gulp

=> at the end of the process, all files in html folder will be zipped and copy to new 
created "data" folder. Following files should be presented in working project folder:
data folder (auto created), node_modules (auto created), html folder, package.json, gulp.js, arduino project file,


##### note:

* all command run in window command line and cd to working folder, in this
case it is home-automation

* if get error: gulp not regconized in command line. Fix by adding:
PATH = %PATH%;%APPDATA%\npm or re-install node.js and repeat all above steps

* in gulpfile.js, *.html syntax will not be recognized but *.htm, this error can causing "data" folder cannot be created

### User Manual

- Server always spinned up when module start

- if not connect to local network, server only accessed on ip: 192.168.4.1

- after using interface provided by server and connect to local nework. Server now can be accessed on both ip 192.168.4.1 and local ip provided by router dhcp. But only local ip can provide internet and more stable when accessing the module. So after connect module to local network, user better disconnect from module's wifi and connect browsing device(phone, laptop, etc.) to local network and access the server by its local ip

- the indicator led onboard will keep blinking when the module running in access point(AP) mode, it only turns off when user connect the module to local network

- server provides simple interface that can control GPIO5, GPIO12 and GPIO14 of ESP8266 module. It means 3 ac devices can be controlled simultaneous by server

- The hardware using triac + opto-coupler that can handle 100-400 vac, but should add heat sink if use 2-3 amp or above for safety. Be careful dont touch these components when ac plugged in

- If something goes wrong, the fuse on board will blowed up in order to protect the circuit

- the button onboard can be used to reset the module (normal reset). Factory reset(erase wifi credential) can be performed by press and hold the button for 5 seconds then release. After factory reset module cannot auto join local network, user can access server only on ip 192.168.4.1

- Since version 2.0, the esp8266's firmware can be updated using wifi (OTA), it means user does not need physically hand on the wifi module to update new firmware to it instead with the existing wifi connection, only new bin file need to loaded using the provided interface to update the module that located somewhere else

- The software also included NTP client built-in to provides the UTC time as an interface for other application such as real-time clock, etc
