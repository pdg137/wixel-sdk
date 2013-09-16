#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <adc.h>
#include <stdio.h>

int32 CODE param_p = 40;
int32 CODE param_d = 170;
int32 CODE param_s = 255;

void timer3Init()
{
    // Start the timer in free-running mode and set the prescaler.
    T3CTL = 0b01110000;   // Prescaler 1:8, frequency = (24000 kHz)/8/256 = 11.7 kHz
    //T3CTL = 0b01010000; // Use this line instead if you want 23.4 kHz (1:4)

    // Set the duty cycles to zero.
    T3CC0 = T3CC1 = 0;

    // Enable PWM on both channels.  We choose the mode where the channel
    // goes high when the timer is at 0 and goes low when the timer value
    // is equal to T3CCn.
    T3CCTL0 = T3CCTL1 = 0b00100100;

    // Configure Timer 3 to use Alternative 1 location, which is the default.
    PERCFG &= ~(1<<5);  // PERCFG.T3CFG = 0;

    // Configure P1_3 and P1_4 to be controlled by a peripheral function (Timer 3)
    // instead of being general purpose I/O.
    P1SEL |= (1<<3) | (1<<4);

    // After calling this function, you can set the duty cycles by simply writing
    // to T3CC0 and T3CC1.  A value of 255 results in a 100% duty cycle, and a
    // value of N < 255 results in a duty cycle of N/256.
}

int16 position = 0;
int16 last_position = 0;
int16 diff = 0;
int16 pid = 0;

int8 rightSensor()
{
	return !isPinHigh(10);
}

int8 leftSensor()
{
	return !isPinHigh(11);
}

void setMotors(int16 left, int16 right)
{
	if(left < 0)
	{
		T3CC1 = -left;
		setDigitalOutput(15, 1);
	}
	else
	{
		T3CC1 = left;
		setDigitalOutput(15, 0);
	}
	if(right < 0)
	{
		T3CC0 = -right;
		setDigitalOutput(16, 1);
	}
	else
	{
		T3CC0 = right;
		setDigitalOutput(16, 0);
	}
}

void updatePwm()
{	
	if(leftSensor() && rightSensor())
		setMotors(255,255);
	else if(leftSensor())
		setMotors(0,255);
	else if(rightSensor())
		setMotors(255,0);
	else
		setMotors(255,-255);
}

void main()
{
    systemInit();
    usbInit();
    timer3Init();

    while(1)
    {
        boardService();
        usbShowStatusWithGreenLed();
        updatePwm();
        usbComService();
    }
}
