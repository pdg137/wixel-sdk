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

int16 left_sensor_avg = 0;
int16 right_sensor_avg = 0;

void checkSensors()
{
  static uint16 index = 0;
  static uint8 history_left[100];
  static uint8 history_right[100];

  left_sensor_avg -= history_left[index];
  right_sensor_avg -= history_right[index];
  history_left[index] = leftSensor();
  history_right[index] = rightSensor();
  left_sensor_avg += history_left[index];
  right_sensor_avg += history_right[index];

  index += 1;
  if(index >= sizeof(history_left))
    index = 0;
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

typedef enum State
{
  SPIN,
  LOCK,
  ATTACK
} State;

void updatePwmFight()
{	
  // 0 SPIN
  // 1 LOCK
  // 2 ATTACK

  static State state = SPIN;
  static uint32 state_start = 0;
  static uint8 last_on_right = 0;

  if(leftSensor() && !rightSensor())
    last_on_right = 0;
  if(!leftSensor() && rightSensor())
    last_on_right = 1;

  switch(state)
  {
  case SPIN:
    if(last_on_right)
      setMotors(255,-255);
    else
      setMotors(-255,255);

	if(left_sensor_avg > 2 && right_sensor_avg > 2)
    {
      state = LOCK;
      state_start = getMs();
      setMotors(0,0);
    }
    break;
  case LOCK:
    setMotors(0,0);
    if(getMs() - state_start > 100)
    {
      state_start = getMs();
      state = ATTACK;
    }
    break;
  case ATTACK:
    if(left_sensor_avg > 2 && right_sensor_avg > 2)
      setMotors(255,255);
	else if(left_sensor_avg > 2)
      setMotors(0,255);
	else if(right_sensor_avg > 2)
      setMotors(255,0);
	else if(getMs() - state_start > 100)
    {
      state_start = getMs();
      state = SPIN;
    }
    else
      setMotors(100,100);
    break;
  }
}

void updatePwm()
{
  if(elapsedTime() < 5000)
    setMotors(0,0);
  else if(elapsedTime() < 5250)
    setMotors(255,255);
  else
    updatePwmFight();
}

void main()
{
  systemInit();
  usbInit();
  timer3Init();
  resetTime();
  
  LED_YELLOW(1);

  while(1)
  {
    boardService();
    usbShowStatusWithGreenLed();
    checkSensors();
    updatePwm();
    usbComService();
  }
}
	
