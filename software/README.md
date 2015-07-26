
###Dependences: cwiid - A Wii controller lib

```
git clone https://github.com/abstrakraft/cwiid.git
```

```
sudo apt-get install gawk
sudo apt-get install bison
sudo apt-get install flex
sudo apt-get install libbluetooth-dev
sudo apt-get install libgtk2.0-dev 
sudo apt-get install libgtk2.0 
sudo apt-get install autoconf
sudo apt-get install autogen
sudo apt-get install automake
sudo apt-get install bluetooth
sudo apt-get install pkg-config
sudo apt-get install python-dev
```

**After installing the above you should run the following in the cwiid directory**
```
autoreconf
./configure
make
sudo make install
```

###Dependences: wiringPi - A neat lib for accessing the GPIO in a sane manner
```
git clone git://git.drogon.net/wiringPi
```
```
cd wiringPi
./build
```

###Build the code

Git clone the repo and use the buildme script in the software directory to build the TractorBot code.
```
git clone https://github.com/ipswichmakerspace/TractorBot.git
cd TractorBot/software
./buildme TractorBot
```

The execute with root permissions to be able to access the GPIO
```
sudo ./TractorBot
```
