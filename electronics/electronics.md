#Electronics#

##Overview##
**TractorBot** electronics is contained on a main interface board.  The interface board features the following features:

- 20 x 2 shrouded header to allow connection to the Raspberry Pi with a 40 way ribbon cable
- 5 volt low dropout voltage regulator
- L293D motor driver
- two LED's and buttons to allow selection of mode - described in the software section
- terminal connectors to conect 7.2v RC car battery
- terminal connectors to connect four motors
- jumper to connect/disconnect power to the Pi
- various headers
    - serial debug connection
    - motor encoders
    - line follower sensors
    - ultra sonic sensor
    - IR object detection sensor
    - servo
    
We have provided the following information:

- A general description on how the the robot works (below)
- Interface board map, showing the features of the board, where they are and also Raspberry Pi pin numbers
- Fritzing file and image, this is not exactly the same as the board as entered into the compitition, mainly because we found it hard to find particualar components in Fritzing. We have also omitted the swithc to disable the rear motors, we did not use this feature in the competition, the robot ran in 4WD at all times.
- We also plan to provide Ki-CAD schematics, but these have not been produced yet.

It should be noted the this documentation for the electronics has been prepared approximately nine months after the interface boards were actually made.  The board got build over a number of weeks and was not planned in advace, therefore the layout is probably not optimum.

##Interface board##
The interface board is build on strip board, when using strip board is is often necessary to break the tracks to allow a single track to be used for two signals, particually the tracks connecting the 20 x 2 header has to be broken along the centre to prevent the two rows of pins from being connected.  All the breaks can be seen in the Fritzing file, but to make it clear I have provided a image of the strip board alone.


###Fritzing Stripboard layout###
showing broken tracks

![stripbaord](../images/TractorBot stripboard.png)

###Fritzing interface board layout###
complete with all wiring

![Interface board](../images/TractorBot_bb.png)

###Interface board map###
showing header and terminal connection pin outs
![Interface board map](../images/TractorBot instructions.png)

[Fritzing file](TractorBot.fzz), note this has been produced after the process of making the actual stripboard, I have done a thorough review to ensure as far as possible that it is correct but there may be a mistake which has been missed.  Please check carefully yourself before use.  I do not take any responsibility for a damaged Pi as a result of using this file.

##20 x 2 shrouded header##
I got this shrouded header and the ribbon cable from an Adafruit Pi cobbler, I think I got mine from [Pimoroni](http://shop.pimoroni.com/products/adafruit-pi-t-cobbler-plus-kit-breakout-for-raspberry-pi-b).  Pin one is shown on all images here to the top right, i.e the end closest to the motor driver is pin 1.  To break the tracks between the two rows of pins I used a stanley knife with a steel ruler and run the knife along the board several times to ensure I cut the tracks properly.  I also checked that each track had been cut using a multi meter on the continuity setting.  vThis should be done before the header is soldered, it would be very dificult to cur the tracks afterwards.

##5v voltage regulator##
To ensure we had enough power for the motors a 7.2 volt radio contorl car battery was used.  We got ours from [Rapid Electronics](http://www.rapidonline.com/Education/VEX-7-2V-Robot-Battery-NiMH-3000mAh-70-6237).  This ensured we had plenty of power for the motors but 7.2v would fry a Pi, to enable this battery to be powered from the same battery and to avoid putting an addition 5v battery on board we used a 5v voltage regulator.  This takes the 7.2v and puts out 5v for use by the Pi.  The voltage regulator is a thre pin device and can be seen adjacent to the right hand button and green LED.  

The part number we used was the National Semiconductor LM2940, data sheet can be found [here](7427.pdf), as can be seen on the data sheet this also requires two capacitors.  It will not work without the capacitors (trust me, I tried it). a 0.41 micro farrade is required between the Vin and ground and a 22 micro farrade capacitor is required between Vout and ground.

In our case, Vin will be the 7.2 volt supply and Vout will be the 5v coming out, which we then feed into the Pi.

##Motor driver##
We used a L293D motor driver to drive all four motors.  The current draw of the motors four motors is quite close to limits of the driver chip but it should be ok.  Pin 1 for the motor driver is to the top right as shown in the images, i.e. pin 1 is on the edge of the strip board.  The data sheet can be found [here](l293d.pdf).  There are many tutorials one the internet showing how to use these chips so I will not cover it here, sufice to say, it takes the low input from the Pi GPIO pins and out puts the 7.2v direct from the battery straight to the motor.

##LED's and buttons##
The interface board has two LED's and buttons.  These are used to navigate through the programs menu system.  The red button is pressed a number of times depending upon the mode required.  the red LED indicated how many times the button has been pressed by flashing.  The button adjacent to the green button selects the desired mode.  More about this will be in the software section.

##Terminals and headers##
All the pins are identified on the interface board map, shown above.  The only exeption being the jumper to contorl the power to the Pi.  I moved this after making the map to make it more accessable, however the Fritzing file and the label on the map are showin in the correct location.  To use this the 7.2v battery must be plugged in, then to power the Pi the jumper is placed on this header.  The Pi will be powered and boot up and automatically start the TractorBot program.

