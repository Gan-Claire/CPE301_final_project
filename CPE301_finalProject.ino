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

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

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
//  Students are responsible for understanding
//  this initialization code for the ATmega2560 USART0
//  and will be expected to be able to intialize
//  the USART in differrent modes.
//
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