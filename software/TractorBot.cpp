#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <math.h>

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

// Support for Wii controller
#include <bluetooth/bluetooth.h>
#include <cwiid.h>

// Wiring Pi a nice C lib for accessing the GPIO 
#include <wiringPi.h>
#include <softPwm.h>

using namespace std;

// Our motor control pins
#define LEFT_MOTOR_FWD   12
#define RIGHT_MOTOR_FWD  10
#define LEFT_MOTOR_BWD   13
#define RIGHT_MOTOR_BWD  11

// The servo pin ( not used directly but defined to reserve use with ServoBlaster )
#define SERVO 			7

// Red button and LED for human feed back
#define BUTTON 			5
#define LED 			6

// Line follower sensors
#define LINE_FARLEFT 	21
#define LINE_NEARLEFT 	22
#define LINE_CENTER 	23
#define LINE_NEARRIGHT  24
#define LINE_FARRIGHT 	25

// Pin for Ping and Pong from ultra sonic sensor
#define SONAR 			14

// Slotted encoders on left and right drive wheels
#define ENCODER_LEFT 	2
#define ENCODER_RIGHT 	3

//Backup IR proximity sensor
#define SENSOR 0

// As close (mm) as we dare ( including the part of the bot in front of the sensor )
#define TARGETDISTANCE 80.0

#define ALLSTOP -99999
#define INVALID 99999

#define WIDTH 320
#define SAMPLEWIDTH 160
#define HEIGHT 50
#define SAMPLEHEIGHT 25

#define TARGETAREA 800

// States used by proximity challenge, ultra sonic detector is checked 4 times to ensure a reasonable reading
enum class State
{
		Moving,
		FirstMeasure,
		SecondMeasure,
		ThirdMeasure,
		Done	
};

// Variables used in call backs and accessed in the main thread.
static volatile int leftCount = 0;
static volatile int rightCount = 0;

static volatile int startTime = 0;
static volatile int endTime = 0; 
static volatile int pulseLength = INVALID;
static volatile bool runProximity = false;

static volatile bool runThreePointTurn = false;

static volatile bool launch = false;
static volatile bool running = true; 
static volatile int presses = 0;
static volatile bool inMenu = true;

// Basic motor speeds 
unsigned int highSpeed = 255;
unsigned int lowSpeed = 70;

int imageWidth = SAMPLEWIDTH;

void remoteControl();
void lineFollower();
void proximity();
void threePointTurn();
void leaveMenu();
void shutdown();


void setLed( int l1 )
{
	digitalWrite (LED, l1);	
}

int CalcSpeed(double diff) 
{
	const double lowLimit = lowSpeed;
	const double highLimit = highSpeed;
	
	diff = std::abs((long)diff);
	
    int speed =  int( (highLimit * ( diff / imageWidth )) + lowLimit );


    return speed;    
}

char getKey() 
{
	fd_set set;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 10;

	FD_ZERO( &set );
	FD_SET( fileno( stdin ), &set );

	int res = select( fileno( stdin )+1, &set, NULL, NULL, &tv );

	if( res > 0 )
	{
		char c;
		printf( "Input available\n" );
		read( fileno( stdin ), &c, 1 );
		return c;
	}
	else if( res < 0 )
	{
		perror( "select error" );
		return 0;
	}
	return 0;
}

void TurnRight(double diff) 
{       
    int speed = CalcSpeed( diff );     
    softPwmWrite (RIGHT_MOTOR_FWD, 0);
    softPwmWrite (RIGHT_MOTOR_BWD, 0);
    softPwmWrite (LEFT_MOTOR_FWD, speed);
    softPwmWrite (LEFT_MOTOR_BWD, 0); 
}

void TurnLeft(double diff) 
{    
    int speed = CalcSpeed( diff ); 
    softPwmWrite (RIGHT_MOTOR_FWD, speed);
    softPwmWrite (RIGHT_MOTOR_BWD, 0);
    softPwmWrite (LEFT_MOTOR_FWD, 0);
    softPwmWrite (LEFT_MOTOR_BWD, 0);    
}

void SharpRight(double diff)
{
    int speed = CalcSpeed( diff );     
    softPwmWrite (RIGHT_MOTOR_FWD, 0);
    softPwmWrite (RIGHT_MOTOR_BWD, speed);
    softPwmWrite (LEFT_MOTOR_FWD, speed);
    softPwmWrite (LEFT_MOTOR_BWD, 0);    
}

void SharpLeft(double diff) 
{
    int speed = CalcSpeed( diff ); 
    softPwmWrite (RIGHT_MOTOR_FWD, speed);
    softPwmWrite (RIGHT_MOTOR_BWD, 0);
    softPwmWrite (LEFT_MOTOR_FWD, 0);
    softPwmWrite (LEFT_MOTOR_BWD, speed);    
}

void Straight( int speed )
{    
    softPwmWrite (RIGHT_MOTOR_FWD, speed );
    softPwmWrite (RIGHT_MOTOR_BWD, 0);
    softPwmWrite (LEFT_MOTOR_FWD, speed );
    softPwmWrite (LEFT_MOTOR_BWD, 0);    
}

void Reverse( int speed )
{    
    softPwmWrite (RIGHT_MOTOR_FWD, 0);
    softPwmWrite (RIGHT_MOTOR_BWD, speed);
    softPwmWrite (LEFT_MOTOR_FWD, 0);
    softPwmWrite (LEFT_MOTOR_BWD, speed);    
}

void Stop() 
{    
    softPwmWrite (RIGHT_MOTOR_FWD, 0);
    softPwmWrite (RIGHT_MOTOR_BWD, 0);
    softPwmWrite (LEFT_MOTOR_FWD, 0);
    softPwmWrite (LEFT_MOTOR_BWD, 0); 
}

void SetRightMotor(int speed)
{
	if( speed < 0 ) 
	{
		speed = max(speed * -1, 255);
		softPwmWrite (RIGHT_MOTOR_FWD, 0 );
		softPwmWrite (RIGHT_MOTOR_BWD, speed );	
	}
	else
	{
		speed = max(speed, 255);
		softPwmWrite (RIGHT_MOTOR_FWD, speed );
		softPwmWrite (RIGHT_MOTOR_BWD, 0 );
	}
}

void SetLeftMotor(int speed)
{
	if( speed < 0 ) 
	{
		speed = max(speed * -1, 255);
		softPwmWrite (LEFT_MOTOR_FWD, 0);
		softPwmWrite (LEFT_MOTOR_BWD, speed );
	}
	else
	{
		speed = max(speed, 255);
		softPwmWrite (LEFT_MOTOR_FWD, speed );
		softPwmWrite (LEFT_MOTOR_BWD, 0);	
	}
}

// Flash an LED a given number of times with the given delay
void flashLED(int times, int speed)
{		
	for(int i = 0; i < times; i++)
	{
		setLed(1);
		delay(speed);
		setLed(0);
		delay(speed);
	}	
}

// If the number of times has changed then flash the new number of times
// (Used to confirm actions)
void blinkLED(int times, int speed)
{	
	static int previous = 0;
	if( times != previous )
	{
		setLed(0);
		delay(500);	
		flashLED(times, speed);
	}
	else
		setLed(0);
		
	previous = times;		
}

// Wait for a pin for a given number of micro-seconds to be either HIGH or LOW depending on the value of level 
long waitforpin(int pin, int level, long timeout)  
{  
   struct timeval now, start;  
   int done;  
   long micros;  
   gettimeofday(&start, NULL);  
   micros = 0;  
   done=0;  
   while (!done)  
   {  
	   gettimeofday(&now, NULL);      
	   micros = ((now.tv_sec - start.tv_sec)*1000000L) + (now.tv_usec - start.tv_usec);  
	   if (micros > timeout) done = 1;	    
	   if (digitalRead(pin) == level) done = 1;  
   }  
   return micros;  
}  

// Test for a press of a button and respond depending on duration
// A press of less than one second will increment the counter
// A press of between two and five seconds with reset the counter
// A press of more than 5 seconds will start a specific challenge function
void countPresses()
{
	setLed(1);
	// Button pulls pin low so we wait for it to go high again.
	long micros = waitforpin(BUTTON,HIGH,30000000L);
	setLed(0);
	
	// A short press might be noise or double bounce	
	if (micros < 10 )
		return;
	// Less than a second is a good press
	if ( micros < 1000000L )
		presses++;
	// More than two seconds but less than five reset the counter
	else if ( micros > 2000000L && micros < 5000000L)
		presses = 0;
	// More than five seconds and the function can be launched
	else if ( micros > 5000000L )
		launch = true;
		
	printf("countPresses: %d\n",presses);
}	

// Start the function in response to a sepcific number of presses
void launchFunction(int presses)
{
	printf("launchFunction: %d\n",presses);
	switch(presses)
	{
		case 1:
			proximity();			
			break;
		case 2:
			lineFollower();
			break;
		case 3:
			remoteControl();
			break;
		case 4:
			threePointTurn();
			break;			
		case 5:
			leaveMenu();
			break;
		case 6:
			shutdown();
		default:
			fprintf(stderr, "Warn: Application not defined\n"); 
	}	
}

// ****************************************************************************
//
// Main function to Shutdown the Pi 
//
// ****************************************************************************
void shutdown()
{	
	// sync the file system and start the shutdown process
	system("sync;halt");
	// Flash the LED as we shutdown we may or may not get to 10 flashes
	flashLED( 10, 250 );
	exit(0);
}

// ****************************************************************************
//
// Main function to exit main menu
//
// ****************************************************************************
void leaveMenu()
{
	inMenu = false;
}

// ****************************************************************************
//
// Main function for three point turn challenge
//
// ****************************************************************************
void threePointTurn()
{
	runThreePointTurn = true;
	
	runThreePointTurn = false;
}

//
// A call back for the Wii controller using cwidd library
//
void cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count,
                    union cwiid_mesg mesg[], struct timespec *timestamp)
{
	int i;
	int leftStick;
	int rightStick;
			
	for (i=0; i < mesg_count; i++)
	{
		switch (mesg[i].type) {
		case CWIID_MESG_NUNCHUK:			
		case CWIID_MESG_STATUS:
		case CWIID_MESG_BTN:
		case CWIID_MESG_ACC:
		case CWIID_MESG_IR:
			break;			
		case CWIID_MESG_CLASSIC:
			printf("button: %4X\n",mesg[i].classic_mesg.buttons);
			// Wii classic controller uses two proportional sticks and many buttons
			if( mesg[i].classic_mesg.buttons == 0x0001 )
			{
				// Use UP on the dpad to open ball catcher
				//printf("Servo forward\n");
				system("echo 0=94% > /dev/servoblaster");
			}
			else if( mesg[i].classic_mesg.buttons == 0x4000 )
			{
				// Use DOWN on the dpad to close ball catcher
				//printf("Servo back\n");
				system("echo 0=62% > /dev/servoblaster");
			}
			else if( mesg[i].classic_mesg.buttons == 0x800 )
			{
				// Use HOME stop and return to main menu
				printf("Stop and return to main menu\n");
				running = false;
			}
			
			
			// Left stick Left = 5, Right = 54, Centre = 30
			// Right stick Up = 29, Down = 2, Centre = 16
			// Proportional joy sticks for turning, forward and reverse
			leftStick = mesg[i].classic_mesg.l_stick[CWIID_X];
			rightStick = mesg[i].classic_mesg.r_stick[CWIID_Y];
			if(leftStick < 14 )
				SharpRight( SAMPLEWIDTH );
			else if( leftStick < 28 )
				TurnRight( SAMPLEWIDTH );
			else if( leftStick > 42 )
				SharpLeft( SAMPLEWIDTH );
			else if( leftStick > 32 )
				TurnLeft( SAMPLEWIDTH );
			else if( rightStick < 14 )							
				Reverse( int(255 * (16 - rightStick)/14));			
			else if( rightStick > 18 )
				Straight( int( 255 * (rightStick-18)/11)) ;
			else	
				Stop();
			break;
			
		case CWIID_MESG_BALANCE:
		case CWIID_MESG_MOTIONPLUS:
		case CWIID_MESG_ERROR:
		case CWIID_MESG_UNKNOWN:
			break;
		default:
			printf("Unknown Report");
			break;
		}
	}
}	
	
// ****************************************************************************
//
// Main function for the remote control challenges
//
// ****************************************************************************
void remoteControl()
{	
	cwiid_wiimote_t *wiimote;	/* wiimote handle */
	//struct cwiid_state state;	/* wiimote state */
	bdaddr_t bdaddr = {{0, 0, 0, 0, 0, 0}};   //* BDADDR_ANY;	/* bluetooth device address */

	char button_state;
	// First pair with the Wiimote
	printf("Put Wiimote in discoverable mode now (press 1+2)...\n");
	flashLED( 1, 1000 );  
	if (!(wiimote = cwiid_open(&bdaddr, 0))) {
		fprintf(stderr, "Unable to connect to wiimote\nReturn to main menu\n");
		flashLED( 5, 250 );
		return;
	}

	// Buzz the Wiimote to confirm pairing 	
	cwiid_set_rumble(wiimote, 1);
	sleep(1);
	cwiid_set_rumble(wiimote, 0);  

	// Set a call back for messages from the Wiimote
	if (cwiid_set_mesg_callback(wiimote, cwiid_callback))
	{
		fprintf(stderr, "Unable to set message callback\n");
		flashLED( 5, 500 );
		return;
	}

	// Set the reporting mode from the Wimote
	if (cwiid_set_rpt_mode(wiimote, CWIID_RPT_EXT))
	{
		fprintf(stderr, "Error setting report mode\n");
		flashLED( 5, 500 );
		return;	
	}

	// Finally enable messages from the Wiimote
	if (cwiid_enable(wiimote, CWIID_FLAG_MESG_IFC))
	{
		fprintf(stderr, "Error enabling messages\n");
		flashLED( 5, 500 );
		return;
	}

	running = true;
	// Enter loop and listen for a key press which will cause the function to exit
	// or "running" may be set false by the Wii call back
	while( running )
	{
		button_state = getKey();		
		if(button_state == 'x') 
		{  	  
			Stop();    
			running = false;
		}		
		sleep(1);
	}

	// Disable our Wii functions
	if(cwiid_disable(wiimote, CWIID_FLAG_MESG_IFC)) 
	{
		fprintf(stderr, "Error disabling messages\n");
	}

	// Finally shutdown the Wii controller
	if (cwiid_close(wiimote)) 
	{
		fprintf(stderr, "Error on wiimote disconnect\n");		
	}
}

//The sensors values are read in and converted to a binary value to help visualize the line position under the sensors.
//00100 represents the robot centered over the line
//10000 means the robot is to the right of the line
//00001 means the robot is to the left of the line
//
// The above value is mapped to a range 5 to -5, the maximum and minimum value depending on the previous position.
int GetPosition(int prev) 
{	
	int farLeft = digitalRead(LINE_FARLEFT);
	int nearLeft = digitalRead(LINE_NEARLEFT);
	int center = digitalRead(LINE_CENTER);
	int nearRight = digitalRead(LINE_NEARRIGHT);
	int farRight = digitalRead(LINE_FARRIGHT);
	
	printf("raw %d %d %d %d %d\n",farLeft,nearLeft,center,nearRight,farRight);
	
	int raw = (16 * farLeft) + (8 * nearLeft) + (4 * center) + (2 * nearRight) + farRight;
	
	//printf("raw %d \n",raw);
	
	// 10000 16 -4
	// 11000 24 -3
	// 01000 8	-2
	// 01100 12	-1
	// 00100 4	 0
	// 00110 6	+1
	// 00010 2	+2
	// 00011 3	+3
	// 00001 1	+4
	
	
	switch(raw) 
	{
		case 16:
		return -4;
		case 24:
		return -3;
		case 8:
		return -2;
		case 12:
		return -1;
		case 4:
		return 0;
		case 6:
		return 1;
		case 2:
		return 2;
		case 3:
		return 3;
		case 1:
		return 4;
	}
	
	if( raw == 0 && prev > 0 )
		return 5;
	else if	( raw == 0 && prev < 0 )
		return -5;		
		
	return 5;
}

// ****************************************************************************
//
// Main function for the line follower challenge
//
// ****************************************************************************
void lineFollower()
{
	flashLED( 2, 1000 );
	
	int previous = 0;
	int prevDiff = 0;
								//tried 2 0 0,3 0 0,1.5 0 1.5,1.5 0 1.8,1.5 0 2,1.2 0 2,1.2 0 2.4,1.2 0 2.8,1.2 0 2.2,1.2 0 2.4,1.0 0 2.4,0.8 0 2.4*, speed at 150
	const double Kp = 0.9;    	//speed 100, 1.8 0 0*, 1.8 0 1, 1.8 0 3.0, 1.8 0 4.0, 1.8 0 5.0, 0.9 0 0.9
	const double Ki = 0;		//speed 80, 0.9 0.0 1.4, 0.9 0.0 1.0, 0.9 0.0 1.4, 0.9 0.0 2.2, 0.9 0.0 4.4
	const double Kd = 0.9;

	double proportional = 0;

	double integral = 0;        

	double rateOfChange = 0;

	double derivative = 0;

	double control = 0;

	running = true;
	while (running) 
	{
		// PID control of motors 
		int position = GetPosition(previous);    
		previous = position;
			   
		int target = 0;    
		int diff = target - position;    

		proportional = diff * Kp;

		integral = (integral + diff) * Ki;        

		rateOfChange = diff - prevDiff;

		derivative = rateOfChange * Kd;    

		control = proportional + integral + derivative;
			
		//printf("Control %f\n",control);

		//if(control < 0)
			//printf("Will turn left\n");
		//else if(control > 0)
			//printf("Will turn right\n");
		//else
			//printf("Will go straight\n");

		int leftSpeed = int(80 + (control * +20));
		int rightSpeed = int(80 + (control * -20));

		printf("Left %d, Right %d\n",leftSpeed,rightSpeed);
			
		SetRightMotor(rightSpeed);
		SetLeftMotor(leftSpeed);		

		char button_state = getKey();

		if(button_state == 'x' || digitalRead(BUTTON) == LOW ) {  	  
			Stop();    
			running = false;
		}
	}
}

// Handler for the echo pulse
void echoStart()
{	
	//A rising edge has been seen so wait for it to go low
	pulseLength = waitforpin(SONAR, LOW, 60000L); /* 60 ms timeout */  
	if( pulseLength > 59999 ) pulseLength = INVALID;		
}

// Handler to increment the left counter on each left encoder pulse
void countLeftEncoder()
{
		leftCount++;
}

// Handler to increment the right counter on each right encoder pulse
void countRightEncoder()
{
		rightCount++;
}

// Each time we get a good reading increase the feel good factor about the reading
State IncreaseState( State state)
{
	switch(state)
	{
		case State::Moving:
			return State::FirstMeasure;
		case State::FirstMeasure:
			return State::SecondMeasure;
		case State::SecondMeasure:
			return State::ThirdMeasure;
		case State::ThirdMeasure:
			return State::Done;	
		case State::Done:
			return State::Done;	
	}
	return state;
}

// Each time we get a poor reading decrease the feel good factor about the reading
State DecreaseState( State state)
{
	switch(state)
	{		
		case State::ThirdMeasure:
			return State::SecondMeasure;
		case State::SecondMeasure:
			return State::FirstMeasure;
		case State::FirstMeasure:
			return State::Moving;
		case State::Moving:
			return State::Moving;
		// We never reach the state but the complier likes us to cover all the bases (states!)
		case State::Done:
			return State::Done;	
	}
	return state;
}

// Create a ping pulse
void ping()
{
	pulseLength = INVALID;
	// Ping the sensor
	pinMode(SONAR, OUTPUT);  
	digitalWrite(SONAR,1);
	usleep(10);
	digitalWrite(SONAR,0);  
	//Listening, wait for the echo
	pinMode(SONAR, INPUT);	
}

// Send out a ping and wait until our handler responds
double findDistance()
{
	pulseLength = INVALID;	
	while(true)
	{
		// Send out a ping and wait for a valid pulse length to be set by the call back
		if( pulseLength == INVALID )
		{
			ping();	
		}
		// Once the pulse length is valid compute the distance to the target
		else
		{	
			// Distance the pulse travelled there and back in that time is: 
			// time multiplied by the speed of sound (mm/s) 
			double distance = (pulseLength * 0.340290) / 2;
			if( distance < 10 )
				return INVALID;
			return distance;
		}
		usleep(10);		
	}	
}

// Stop and check the distance to the target, if its less then 
// increase our feel good factor about the measurement
State GetDistance( State state, double target )
{
	double distance = findDistance();	
	
	if( distance != INVALID )
	{
		//double leftDistance = leftCount * 10.52;
		//double rightDistance = rightCount * 10.52;
		//printf("Distance: %6.2fmm\t  %d secs\t", distance, pulseLength );
		//printf("Left: %6.2fmm\tRight: %6.2fmm\n",leftDistance, rightDistance);			
		if( distance < target )
		{			
			Stop();
			printf("IR Sensor %d\n",digitalRead(SENSOR));
			printf("Target %6.2fmm\tDistance: %6.2fmm\tState %d\n",target, distance, state );
			sleep(1);							
			return IncreaseState(state);
		}
	}	
	return DecreaseState(state);
}

// Advance to target state machine, when the bot reaches a target distance it stops 
// and checks its distance if 3 distances are within target then it assumes its arrived
// and exits the state machine.
bool advanceToTarget(double target)
{	
	printf("Target %f\n",target);
	State state = State::Moving;
	bool running = true;
	while (running) {
		
		char button_state = getKey();		
		if( button_state == 'x' )
			return false;
		
		// Ensure we are moving 
		if( state == State::Moving )
			Straight(lowSpeed);
			
		// Then while moving take our first reading
		if( state == State::Moving )			
			state = GetDistance(state, target);
			
		if( state == State::FirstMeasure )			
			state = GetDistance(state, target);
			
		if( state == State::SecondMeasure )			
			state = GetDistance(state, target);
			
		if( state == State::ThirdMeasure )
			state = GetDistance(state, target);
			
		if( state == State::Done )	
			running = false;					
	}	
	return true;
}

void done()
{
	if (runProximity && digitalRead(SENSOR) == LOW)  
	{
		Stop();		
		printf("IR Sensor fired closing down\n");				
	}
	return;
}	

// ****************************************************************************
//
// Main function for the proximity challenge
//
// ****************************************************************************
void proximity()
{
	runProximity = true;

	bool success = true;

	success = runProximity && success && advanceToTarget(300);

	success = runProximity && success && advanceToTarget(TARGETDISTANCE);
	Stop();
	printf("Waiting for next run\n");

	runProximity = false;
}

int main(int argc, char** argv) 
{
	// GPIO access needs to be root 
	if(geteuid() != 0)
	{
		fprintf(stderr, "Error: requires root to run (sudo?).\n");   
		exit(-1);
	}    	

	// Set up Wiring Pi to use native GPIO numbering
	wiringPiSetup();

	// Setup our tell-tale LEDs
	pinMode (LED, OUTPUT);
	//pullUpDnControl (LED, PUD_DOWN);

	// Setup our control buttons
	pinMode (BUTTON, INPUT);
	pullUpDnControl (BUTTON, PUD_UP);  

	// Create PWM objects for our motors
	int p = softPwmCreate(RIGHT_MOTOR_FWD, 0, 255);
	int q = softPwmCreate(RIGHT_MOTOR_BWD, 0, 255);
	int a = softPwmCreate(LEFT_MOTOR_FWD,  0, 255);
	int b = softPwmCreate(LEFT_MOTOR_BWD,  0, 255); 
	 
	if( (p + q + a + b) != 0 ) {
	  fprintf(stderr, "Error setting PWM.\n");
	  flashLED( 5, 250 ); 
	  exit(-1);
	}
	
	// Set call back for left encoder
	wiringPiISR (ENCODER_LEFT, INT_EDGE_RISING,  &countLeftEncoder);
	// Set call back for right encoder
	wiringPiISR (ENCODER_RIGHT, INT_EDGE_RISING,  &countRightEncoder); 	
	// Set call back for echoStart seen
	wiringPiISR (SONAR, INT_EDGE_RISING,  &echoStart);   	
	// Set call back for ir sensor switching off
	wiringPiISR (SENSOR, INT_EDGE_FALLING,  &done);   

	pinMode (SENSOR, INPUT); 

	// To allow us to simply press a key without Enter we set our terminal to RAW
	// and we save the original settings to restore them on exit
	struct termios oldSettings, newSettings;
	tcgetattr( fileno( stdin ), &oldSettings );
	newSettings = oldSettings;
	newSettings.c_lflag &= (~ICANON & ~ECHO);
	tcsetattr( fileno( stdin ), TCSANOW, &newSettings );   
	
	// Disable all tell-tale LEDs
	setLed( 0 );

	// This is our main menu from which we launch our challenge functions
	presses = 0;	
	while (inMenu) 
	{	 
		blinkLED(presses, 500);  
	
		if (digitalRead(BUTTON) == LOW)
			countPresses( );
			
		if (launch)
		{
			// Launch the function and reset the number of presses for the menu and launch flag
			launchFunction(presses);
			launch = false;
			presses = 0;
		}
	  
		char key_state = getKey();  
		if(key_state == 'x' ) 
			inMenu = false;

	}

	// Say "good night" Lucy
	flashLED( 2, 1000 );
	// Put back the terminal settings as they were
	tcsetattr( fileno( stdin ), TCSANOW, &oldSettings );
	exit(0);
}

