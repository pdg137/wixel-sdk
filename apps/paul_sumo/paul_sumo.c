#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <adc.h>
#include <stdio.h>

int32 CODE param_p = 40;
int32 CODE param_d = 170;
int32 CODE param_s = 255;

uint32 start_ms;

void resetTime()
{
  start_ms = getMs();
}

uint32 elapsedTime()
{
  return getMs() - start_ms;
}

uint8 buttonPressed()
{
  setPort0PullType(HIGH);
  setDigitalInput(00,PULLED);
  setDigitalOutput(03,LOW);
  return !isPinHigh(00);
}

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
    setDigitalOutput(16, 1);
  }
  else
  {
    T3CC1 = left;
    setDigitalOutput(16, 0);
  }
  if(right < 0)
  {
    T3CC0 = -right;
    setDigitalOutput(15, 1);
  }
  else
  {
    T3CC0 = right;
    setDigitalOutput(15, 0);
  }
}

typedef enum State
{
  WAIT_BUTTON,
  DELAY,
  RUN,
  SPIN,
  ATTACK,
  BACKUP
} State;

void updateState()
{
  static State state = WAIT_BUTTON;
  static uint32 state_start = 0;
  static uint32 last_seen = 0;
  static uint8 last_on_right = 0;
  static int32 reverse;

  if(leftSensor() && !rightSensor())
    last_on_right = 0;
  if(rightSensor() && !leftSensor())
    last_on_right = 1;
  if(rightSensor() || leftSensor())
    last_seen = getMs();

  LED_RED(leftSensor());
  LED_YELLOW(rightSensor());

  switch(state)
  {
  case WAIT_BUTTON:
    setMotors(0,0);
    if(buttonPressed())
    {
      state_start = getMs();
      state = DELAY;
    }
    break;
  case DELAY:
    setMotors(0,0);
    if(getMs() - state_start > 5000)
    {
      state_start = getMs();
      state = RUN;
    }
    LED_YELLOW((getMs() - state_start) & 0x80);
    LED_RED((getMs() - state_start) & 0x80);
    break;
  case RUN:
    setMotors(255,255);
    if(getMs() - state_start > 200)
    {
      state_start = getMs();
      state = SPIN;
    }
    break;
  case SPIN:
    reverse = -255 + (getMs() - state_start)/16;
    if(reverse > 50)
      reverse = 50;

    if(last_on_right)
      setMotors(255,reverse);
    else
      setMotors(reverse,255);

	if(leftSensor() || rightSensor())
    {
      state = ATTACK;
      state_start = getMs();
    }
    break;
  case ATTACK:
    if(getMs() - state_start > 750)
    {
      state_start = getMs();
      state = BACKUP;
    }
    else if(leftSensor() && rightSensor())
      setMotors(255,255);
	else if(leftSensor())
      setMotors(100,255);
	else if(rightSensor())
      setMotors(255,100);
	else if(getMs() - last_seen > 50)
    {
      state_start = getMs();
      state = SPIN;
    }
    break;
  case BACKUP:
    if(getMs() - state_start > 300)
    {
      state_start = getMs();
      state = ATTACK;
    }
    else if(getMs() - state_start > 100)
      setMotors(128,128);
    else
      setMotors(-255,-255);
    break;
  }
}

void main()
{
  systemInit();
  usbInit();
  timer3Init();
  resetTime();
  
  while(1)
  {
    boardService();
    usbShowStatusWithGreenLed();
    updateState();
    usbComService();
  }
}
	
