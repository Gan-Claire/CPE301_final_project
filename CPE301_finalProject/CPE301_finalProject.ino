//Claire Gan Fall 2023 CPE 301 Final Project

//used library
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <DHT.h>
#include <Stepper.h>

//LED pins
#define LED_pinRed 3
#define LED_pinYellow 3
#define LED_pinGreen 5
#define LED_pinBlue 5

#define RDA 0x80
#define TBE 0x20

//buttons pins
#define button_top 2
#define button_bottom 3
#define button_up 4
#define button_down 5

//motor pin
#define motor_pin 6

//sensor pin
#define DHT_pin 7
#define DHT_type DHT11

//stepper pins
#define STEPPER_PIN1 10
#define STEPPER_PIN2 11
#define STEPPER_PIN3 12
#define STEPPER_PIN4 13

//water pin
#define water_level 5

//button interpret
#define BUTTON_ONOFF 0
#define BUTTON_RESET 1

//controller pin setup
LiquidCrystal lcd(30, 31, 32, 33, 34, 35);

//clock
RTC_DS1307 RTC;

//DHT setup
DHT dht(DHT_pin, DHT_type);

//stepper setup
Stepper stepper(2048, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4);
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//water threshold
#define water_threshold 320
#define temp_threshold 10

//all states
enum States{
  IDLE,
  DISABLED,
  RUNNING,
  ERROR,
  START,
};


States previous = START;
States current = DISABLED;

int lastTempPrint = 0;
float temp = 0;
float hum = 0;
int stepperRate = 2048;
bool fanOn = false;
int ledC = -1;
bool displayTempHum = false;
bool stepperAllowed = false;
bool monitorWater = false;

void setup() {
  // put your setup code here, to run once:
  PORTD |= (1 << PD0) | (1 << PD1);
  RTC.begin();
  DateTime now = DateTime(2023, 11, 30, 0, 0, 0);
  RTC.adjust(now);

  DDRE |= (0x01 << LED_pinGreen | 0x01 << LED_pinYellow);
  DDRG |= (0x01 << LED_pinBlue);
  DDRH |= (0x01 << LED_pinRed | 0x01 << water_level | 0x01 << motor_pin);

  adc_init();
  U0init(9600);
  dht.begin();
  lcd.begin(16, 2);
  lcd.print("System begin");

  DDRA &= ~(0x01 << BUTTON_RESET | 0x01 << BUTTON_ONOFF | 0x01 << button_top | 0x01 << button_bottom | 0x01 << button_up | 0x01 << button_down);

  attachInterrupt(digitalPinToInterrupt(BUTTON_ONOFF), buttonOnOFF, RISING);
  stepper.setSpeed(10);

}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime now = RTC.now();
  
  if(displayTempHum){
    // read the temperature and humidity from the DHT11 sensor
    temp = dht.readTemperature();
    hum = dht.readHumidity();
  }

  current = changeState(temp, hum, current);

  if(readPin(BUTTON_ONOFF)){
    buttonOnOFF();
  }
  // when state is switching exec
  if(current != previous){
    
    writeTimeStampTransition(now, previous, current);
    
    // check the current state of the state machine
    switch (current) {
      case DISABLED:
        fanOn = false;
        ledC = 3;
        displayTempHum = false;
        stepperAllowed = true;
        monitorWater = false;
        break;

      case IDLE:
        fanOn = false;
        ledC = 2;
        displayTempHum = true;
        stepperAllowed = true;
        monitorWater = true;
        break;

      case RUNNING:
        fanOn = true;
        ledC = 1;
        displayTempHum = true;
        stepperAllowed = true;
        monitorWater = true;
        break;

      case ERROR:
        lcd.clear();
        lcd.print("Error, low water level");
        fanOn = false;
        ledC = 0;
        displayTempHum = true;
        stepperAllowed = false;
        monitorWater = true;
        break;
      case START:
        break;
    }
  }
  if(stepperAllowed){
    //determine direction the user wants, the limits it can go, then set that speed
    int stepperDirection = stepperRate * (readPin(button_up) ? 1 : readPin(button_bottom) ? -1 : 0);
    //preform limit checks
    stepperDirection = (readPin(button_top) ? min(stepperDirection, 0) : (readPin(button_bottom) ? max(stepperDirection ,0) : stepperDirection));
    if(stepperDirection != 0){
      writeStepperPos(now, previous, current);
    }
    setupStepperMotor(stepperDirection);
  }
  // set fan rate
  setupStepperMotor(fanOn);

  // set led
  turnOnLED(ledC);

  // display temp hum, if we are allowed to, and if the time since the last one has been greater then one minute
  if(displayTempHum && abs(lastTempPrint - now.minute()) >= 1){
    
    lcd.clear();
    lastTempPrint = now.minute(); // update prev
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    lcd.print("Temp, Humidity");
    delay(1000);
    lcd.clear();
    lcd.print(temp); // write temp to lcd
    lcd.print(hum); // write hum to lcd
  }

  previous = current;

  if(monitorWater){
    int waterLvl = adc_read(water_level); // calc water lvl
    if(waterLvl <= water_threshold){
      current = ERROR;
    }
  }
  
  delay(500);
}


//Given functions from labs
//analog read
void adc_init(){
     ADCSRA = 0x80;
     ADCSRB = 0x00;
     ADMUX = 0x40;
}

unsigned int adc_read(unsigned char adc_channel){
     ADCSRB &= 0xF7; // Reset MUX5.
     ADCSRB |= (adc_channel & 0x08); // Set MUX5.
     ADMUX &= 0xF8; // Reset MUX2:0.
     ADMUX |= (adc_channel & 0x07); // Set MUX2:0.

     ADCSRA |= 0x40; // Start the conversion.
     while (ADCSRA & 0x40) {} // Converting...
     return ADC; // Return the converted number.
}

//serial functions
void U0init(unsigned long U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
//
// Read USART0 RDA status bit and return non-zero true if set
//
unsigned char U0kbhit()
{
  return (RDA & *myUCSR0A);
}
//
// Read input character from USART0 input buffer
//
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while(!(TBE & *myUCSR0A));
  *myUDR0 = U0pdata;
}

//funtions for the system
//set button to on
bool buttonOn = true;

//handle button state
void buttonOnOFF() {
  previous = current;
  bool button_pressed = readPin(BUTTON_ONOFF);
  if(buttonOn && button_pressed){
    current = IDLE;
    buttonOn = false;
  }
  else if (button_pressed){
    current = DISABLED;
    buttonOn = true;
  }
}

// read pin
int readPin(int pin){
  return PINA & (0x01 << pin);
}

//read water
int readWater(int pin){
  return PINH & (0x01 << pin);
}

void writeStepperPos(DateTime now, States prevState, States currentState){
  U0putchar('S');
  U0putchar('T');
  U0putchar('E');
  U0putchar('P');

  
  U0putchar(' ');
  writeTimeStampTransition(now, prevState, currentState);
}

//write simple msgs over serial one char at a time
void writeTimeStampTransition(DateTime now, States prevState, States currentState){
  unsigned char pState = (prevState == DISABLED ? 'd' : (prevState == IDLE ? 'i' : (prevState == RUNNING ? 'r' : (prevState == ERROR ? 'e' : 'u'))));
  unsigned char cState = (currentState == DISABLED ? 'd' : (currentState == IDLE ? 'i' : (currentState == RUNNING ? 'r' : (currentState == ERROR ? 'e' : 'u')))); 
  U0putchar(pState);
  U0putchar(':');
  U0putchar(cState);

  U0putchar(' ');

  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();

  //store current time in array and calculate their position in the array
  char numbers[10] = {'0','1','2','3','4','5','6','7','8','9'};
  int onesYear = year % 10;
  int tensYear = year / 10 % 10;
  int onesMonth = month % 10;
  int tensMonth = month / 10 % 10;
  int onesDay = day % 10;
  int tensDay = day / 10 % 10;
  int onesHour = hour % 10;
  int tensHour = hour / 10 % 10;
  int onesMinute = minute % 10;
  int tensMinute = minute / 10 % 10;
  int onesSecond = second % 10;
  int tensSecond = second / 10 % 10;
  
  U0putchar('M');
  U0putchar(':');
  U0putchar('D');
  U0putchar(':');
  U0putchar('Y');

  U0putchar(' ');
  
  U0putchar('H');
  U0putchar(':');
  U0putchar('M');
  U0putchar(':');
  U0putchar('S');

  U0putchar(' ');

  //print based on their position
  U0putchar(numbers[tensMonth]);
  U0putchar(numbers[onesMonth]);
  U0putchar(':');
  U0putchar(numbers[tensDay]);
  U0putchar(numbers[onesDay]);
  U0putchar(':');
  U0putchar(numbers[tensYear]);
  U0putchar(numbers[onesYear]);
  U0putchar(' ');
  U0putchar(numbers[tensHour]);
  U0putchar(numbers[onesHour]);
  U0putchar(':');
  U0putchar(numbers[tensMinute]);
  U0putchar(numbers[onesMinute]);
  U0putchar(':');
  U0putchar(numbers[tensSecond]);
  U0putchar(numbers[onesSecond]);
  U0putchar('\n');
}

//determine the state of the system
States changeState(float temp, int waterLevel, States current) {
  States state;
  if (temp <= temp_threshold && current == RUNNING){
    state = IDLE;
  }
  else if (temp > temp_threshold && current == IDLE) {
    state = RUNNING;
  }
  else if (current == ERROR && readPin(BUTTON_RESET) && waterLevel > water_threshold) {
    state = IDLE;
  }
  else {
    state = current;
  }

  return state;
}

//turn on LED
void turnOnLED(int ledPin) {
  // turn off all
  PORTH &= ~(0x01 << LED_pinRed);
  PORTG &= ~(0x01 << LED_pinBlue);
  PORTE &= ~(0x01 << LED_pinGreen);
  PORTE &= ~(0x01 << LED_pinYellow);

  //base on pin turn on corresponding LED
  switch (ledPin) {
    case 0:
      PORTH |= 0x01 << LED_pinRed;
      break;
    case 1:
      PORTG |= 0x01 << LED_pinBlue;
      break;
    case 2:
      PORTE |= 0x01 << LED_pinGreen;
      break;
    case 3:
      PORTE |= 0x01 << LED_pinYellow;
      break;
  }
}

//stepper speed
void setupStepperMotor(int distance){
  stepper.step(distance);
}

//set up motor
void setupMotor(bool on){
  if(on){
    PORTH |= (0x01 << motor_pin);
  }
  else {
    PORTH &= ~(0x01 << motor_pin);
  }
}