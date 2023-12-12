//Claire Gan Fall 2023 CPE 301 Final Project

//used library
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <DHT.h>
#include <Stepper.h>

//LED pins
#define LED_pinRed 24
#define LED_pinYellow 22
#define LED_pinGreen 23
#define LED_pinBlue 25

#define RDA 0x80
#define TBE 0x20

//motor pin
#define motor_pin 9

//sensor pin
#define DHT_pin 7
#define DHT_type DHT11

//stepper pins
#define STEPPER_PIN1 34
#define STEPPER_PIN2 35
#define STEPPER_PIN3 36
#define STEPPER_PIN4 37

//water pin
#define water_level 5

//button interpret
#define BUTTON_ONOFF 28
#define BUTTON_RESET 29

//controller pin setup
LiquidCrystal lcd(11,12,2,3,4,5);

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
#define water_threshold 300
#define temp_threshold 20

//all states
enum States{
  ID,
  DIS,
  RUN,
  ERR,
  START,
};

int lastTempPrint = 0;
float temp = 0;
float hum = 0;
int stepperRate = 2048;
bool fanOn = false;
int ledC = 0;
bool displayTempHum = false;
bool stepperAllowed = false;
bool monitorWater = false;
bool buttonOn = true;
States previous = START;
States current = DIS;

void setup() {
  PORTB |= (1 << PB0) | (1 << PB1);
  RTC.begin();
  DateTime now = DateTime(2023, 11, 30, 0, 0, 0);
  RTC.adjust(now);

  adc_init();
  U0init(9600);
  dht.begin();
  lcd.begin(16, 2);

  DDRA &= ~(0x01 << BUTTON_RESET | 0x01 << BUTTON_ONOFF);

  attachInterrupt(digitalPinToInterrupt(BUTTON_ONOFF), buttonOnOFF, RISING);
  stepper.setSpeed(10);

}

void loop() {
  DateTime now = RTC.now();
  
  if(displayTempHum){
    temp = dht.readTemperature();
    hum = dht.readHumidity();
  }

  current = changeState(temp, hum, current);

  if(readPin(BUTTON_ONOFF)){
    buttonOnOFF();
  }
  if(current != previous){
    
    writeTimeStampTransition(now, previous, current);
    
    // check the current state of the state machine
    switch (current) {
      case DIS:
        fanOn = false;
        ledC = 3;
        displayTempHum = false;
        stepperAllowed = true;
        monitorWater = false;
        break;

      case ID:
        fanOn = false;
        ledC = 2;
        displayTempHum = true;
        stepperAllowed = true;
        monitorWater = true;
        break;

      case RUN:
        fanOn = true;
        ledC = 1;
        displayTempHum = true;
        stepperAllowed = true;
        monitorWater = true;
        break;

      case ERR:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Error");
        lcd.setCursor(0, 1);
        lcd.print("Low Water Level");
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
  // set fan rate
  setupStepperMotor(fanOn);

  // set led
  turnOnLED(ledC);

  int timeCheck = abs(lastTempPrint - now.minute());

  if(displayTempHum && timeCheck >= 1){
    
  
    lastTempPrint = now.minute(); // update prev
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    lcd.clear();
    lcd.print("Temp:");
    lcd.print(temp); // write temp to lcd
    lcd.print("Humidity:");
    lcd.print(hum); // write hum to lcd
  }

  previous = current;

  if(monitorWater){
    int waterLvl = adc_read(water_level);
    if(waterLvl <= adc_read(water_level)){
      current = ERR;
    }
  }
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

unsigned char U0kbhit()
{
  return (RDA & *myUCSR0A);
}

unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while(!(TBE & *myUCSR0A));
  *myUDR0 = U0pdata;
}


void buttonOnOFF() {
  previous = current;
  bool button_pressed = readPin(BUTTON_ONOFF);
  if(buttonOn != button_pressed){
    current = ID;
    buttonOn = false;
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


//determine the state of the system
States changeState(float temp, int waterLevel, States current) {
  States state;
  if (temp <= temp_threshold && current == RUN){
    state = ID;
  }
  else if (temp > temp_threshold && current == ID) {
    state = RUN;
  }
  else if (current == ERR && readPin(BUTTON_RESET) && waterLevel > water_threshold) {
    state = ID;
  }
  else {
    state = current;
  }

  return state;
}

void writeTimeStampTransition(DateTime now, States prevState, States currentState){
  unsigned char pState = (prevState == DIS ? 'd' : (prevState == ID ? 'i' : (prevState == RUN ? 'r' : (prevState == ERR ? 'e' : 'u'))));
  unsigned char cState = (currentState == DIS ? 'd' : (currentState == ID ? 'i' : (currentState == RUN ? 'r' : (currentState == ERR ? 'e' : 'u')))); 
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


//turn on LED
void turnOnLED(int ledPin) {
  PORTG &= ~(0x01 << LED_pinBlue);
  PORTH &= ~(0x01 << LED_pinRed);
  PORTE &= ~(0x01 << LED_pinYellow);
  PORTE &= ~(0x01 << LED_pinGreen);

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