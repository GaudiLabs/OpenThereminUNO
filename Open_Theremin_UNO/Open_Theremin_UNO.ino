/*
 *  Open.Theremin.UNO control software for Arduino.UNO
 *  Version 2.0
 *  Copyright (C) 2010-2013 by Urs Gaudenz
 *
 *  Open.Theremin.UNO control software is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Open.Theremin.UNO control software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  the Open.Theremin.UNO control software.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Version 1.2A - minor 'arduino-izing' - Michael Margolis 20 Oct 2013 
 */

#include "mcpDac.h"
#include "theremin_sintable.c"

//#define BAUD 115200 // uncomment if serial is implimented

//Altering the following pins requires altering the direct port I/O
#define buttonPin      4            // Button Pin on D4 
#define BUTTON_STATE  (PIND&(1<<PORTD4))
#define ledPin         5            // LED on D5
#define LED_ON  (PORTD |= (1<<PORTD5))
#define LED_OFF (PORTD &= ~(1<<PORTD5))

#define INT0_STATE  (PIND&(1<<PORTD2))
#define PC_STATE  (PINB&(1<<PORTB0))

/* General variables */
int32_t pitch_init       = 0;       // Initialization value of pitch
int32_t vol_init         = 0;       // Initialization value of volume
int32_t pitch_v,pitch_l;            // Last value of pitch (for filtering)
int32_t vol_v,vol_l;                // Last value of volume (for filtering)
uint16_t pitch           = 0;       // Pitch value
uint16_t pitch_counter   = 0;       // Pitch counter
uint16_t pitch_counter_l = 0;       // Last value of pitch counter
uint16_t vol             = 0;       // Volume value
uint16_t vol_counter_l   = 0;       // Last value of volume counter

uint8_t i;
uint8_t state     = 0;              // State in the calibration state machine
uint8_t mode      = 4;              // Calibration mode

/* volatile varables  - used in the ISR Routine*/
volatile uint8_t vol8;               // Volume byte
volatile bool flag_vol,flag_pitch        = 0;   // Volume read flag
volatile uint16_t vol_counter,vol_counter_i = 0;   // Volume counter
volatile uint16_t pointer     = 0;   // Table pointer
volatile uint16_t add_val     = 0;   // Table pointer increment
volatile uint16_t timer       = 0;   // Timer value
volatile uint8_t deb_p,deb_v       = 0;   // Counters vor debouncing


/* General Setup Routine */
void setup() {

  //Serial.begin(BAUD); // remove leading backslash if using serial
  pinMode(buttonPin,INPUT_PULLUP);            
  pinMode(ledPin, OUTPUT);

  /* Setup Timer 1, 16 bit timer used to measure pitch and volume frequency */
  TCCR1A = 0;                     // Set Timer 1 to Normal port operation (Arduino does activate something here ?)
  TCCR1B = (1<<ICES1)|(1<<CS10);  // Input Capture Positive edge select, Run without prescaling (16 Mhz)

TIMSK1 = (1<<ICIE1);            // Enable Input Capture Inrrupt


  /* Setup interrupts for Wave Generator and Volume read */
  EICRA = (1<<ISC00)|(1<<ISC01)|(1<<ISC11)|(1<<ISC10) ; // The rising edges of INT0 and INT1 generate an interrupt request.
  EIMSK = (1<<INT0)|(1<<INT1);    // Enable External Interrupt INT0 and INT1

  interrupts();            // Enable Interrupts (this is a macro for SEI)

  mcpDacInit();         // Initialize Digital Analog Converter (DAC)

  /*
   // Init PWM Generator for CV out (if used)
   TCCR0A = (1<<COM0A1)|(1<<WGM01)|(1<<WGM00); //Set PWM on OC0A
   TCCR0B = (1<<CS00); // Set Clock with No prescaling on Timer 0
   TIMSK0=0; // No interrups
   pinMode(6, OUTPUT);        // Set Pin D6 as output
   */

  vol8=255;                 // Set volume to max

  add_val = 500;            // Play welcome sounds
  ticktimer(15000);
  add_val = 1000;
  ticktimer(15000);
  add_val = 2000;
  ticktimer(30000);

  InitValues();            // Capture initial calibration values
}


/* Main Loop */
void loop() {

mloop:                          // Main loop avoiding the GCC "optimization"    

  if((state==0) && (BUTTON_STATE == LOW))       // Check if key released
  {
    state=1;
    timer=0;
  }

  if((state==1)&&(BUTTON_STATE != LOW))     // If key pressed
  {
    if (timer > 1500)
    {
      vol8=255;                 // Play calibration sounds
      add_val = 700;
      ticktimer(15000);
      vol8=0;
      ticktimer(3000);
      vol8=255;
      ticktimer(15000);
      vol8=0;
      ticktimer(3000);
      vol8=255;             
      ticktimer(15000);
      vol8=0;
      ticktimer(3000);
      add_val = 2000;
      vol8=255;
      ticktimer(3000);

      InitValues();                 // Capture calibration Values
      state=0;
      mode=4;
    } 
    else {
      state=0;
    }
  };

  if((state==1)&&(timer>20000))      // If key pressed for >64 ms switch calibration modes
  {
    state=0;
    mode++;

    if (mode>4){
      mode=1;
    };
    for (i=0;i<mode;i++)
    {
      vol8=255;
      add_val = 3000;   
      ticktimer(1500);
      vol8=0;
      ticktimer(1500);
      vol8=255;
      add_val = 0;  
    };

    while(BUTTON_STATE == LOW) // button pressed
    {
    };
  };

  //OCR0A=pitch&0xff;                   // Set CV value (if used)

  //if (timer>3125){
  // timer=0;
  //
  // uart_putchar(pitch&0xff);              // Send char on serial (if used)
  // uart_putchar((pitch>>8)&0xff);
  //
  //}


  /* New PITCH value */

  if (flag_pitch){                      // If capture event



    //if ((pitch>19000)&(pitch<21000)) {PORTA |= (1<<PA0);} else {PORTA &= ~(1<<PA0);}  // LED on if value in range

    pitch_v=pitch;                  // Averaging pitch values
    pitch_v=pitch_l+((pitch_v-pitch_l)>>2); 
    pitch_l=pitch_v;    

    switch (mode)                   // set wave frequency for each mode
    {
      case 1: add_val=33554432/pitch_v;           break; // pitch calibration mode
      case 2:                                     break; // volume calibration mode
      case 3: add_val=(pitch_init-pitch_v)/2+200; break; // mode without volume
      case 4: add_val=(pitch_init-pitch_v)/2+200; break; // normal operation
    };

    flag_pitch=false;                         // Clear pitch capture flag
  }

  /* New VOLUME value */

  if (flag_vol){



 if (vol<5000) vol=5000;
    vol_v=vol;                  // Averaging volume values
    vol_v=vol_l+((vol_v-vol_l)>>2);
    vol_l=vol_v;

    //if ((vol_v>15000)&(vol_v>17000)) {PORTA |= (1<<PA1);} else {PORTA &= ~(1<<PA1);} // LED on if value in range

    switch (mode)                   // set volume for each mode
    {
      case 1: vol_v=4095;                          break; // pitch calibration mode 
      case 2: add_val=33554432/vol_v;  vol_v=4095; break; // volume calibration mode
      case 3: vol_v=4095;                          break; // mode without volume
      case 4: vol_v=4095-(vol_init-vol_v);         break; // normal operation
    };

    if (vol_v>4095) {
      vol8=255;
    } 
    else            // Limit and set volume value
    if (vol_v<0) {
      vol8=0;
    } 
    else {
      vol8=vol_v>>4;
    }
    flag_vol=false;                     // Clear volume flag

  }

  goto mloop;                           // End of main loop 
}

/* 16 bit by 8 bit multiplication */
inline uint32_t mul_16_8(uint16_t a, uint8_t b)   //removed static for arduino 1.6.6.
{
  uint32_t product;
  asm (
    "mul %A1, %2\n\t"
    "movw %A0, r0\n\t"
    "clr %C0\n\t"
    "clr %D0\n\t"
    "mul %B1, %2\n\t"
    "add %B0, r0\n\t"
    "adc %C0, r1\n\t"
    "clr r1"
: 
    "=&r" (product)
: 
    "r" (a), "r" (b));
  return product;
}

/* Externaly generated 31250 Hz Interrupt for WAVE generator (32us) */
ISR (INT1_vect)                         
{
  // Interrupt takes up a total of max 25 us
  EIMSK &= ~ (1<<INT1);     // Disable External Interrupt INT1 to avoid recursive interrupts
  interrupts();             // Enable Interrupts to allow counter 1 interrupts

  int16_t temp_val;         // temporary variable 1
  uint32_t temp2_val;       // temporary variable 2

  temp_val = (signed int)pgm_read_word_near (sine_table + ((unsigned int)(pointer>>6) & 0x3ff));  //Read next wave table value (3.0us)

  //  LED_ON;
  if (temp_val>0){                      // multiply 16 bit wave number by 8 bit volume value (11.2us / 5.4us)
    temp2_val=mul_16_8(temp_val,vol8);
    temp2_val=temp2_val>>9; 
    temp2_val=temp2_val+1748;  
  } 
  else
  { 
    temp2_val=mul_16_8(-temp_val,vol8);
    temp2_val=temp2_val>>9; 
    temp2_val=1748-temp2_val;
  }

  //  LED_OFF;
  mcpDacSend(temp2_val);        //Send result to Digital to Analogue Converter (audio out) (9.6 us)

  pointer = pointer + add_val;  // increment table pointer (ca. 2us)
  timer++;                      // update 32us timer



if (PC_STATE) deb_p++;
if (deb_p==3) {
  cli();
      pitch_counter=ICR1;                      // Get Timer-Counter 1 value

        pitch=(pitch_counter-pitch_counter_l);   // Counter change since last interrupt -> pitch value
    pitch_counter_l=pitch_counter;           // Set actual value as new last value
    

  };
  
  if (deb_p==5) {

    flag_pitch=true;
  };
  

    
    
if (INT0_STATE) deb_v++; 
if (deb_v==3) {
  cli();
    vol_counter=vol_counter_i;                      // Get Timer-Counter 1 value
            vol=(vol_counter-vol_counter_l);        // Counter change since last interrupt 
    vol_counter_l=vol_counter;          // Set actual value as new last value

};
    
if (deb_v==5) {

    flag_vol=true;
};
    

  cli();                        // Turn of interrupts
  EIMSK |= (1<<INT1);           // Re-Enable External Interrupt INT1

}

/* VOLUME read - interrupt service routine for capturing volume counter value */
ISR (INT0_vect)             
{   
  vol_counter_i = TCNT1;
  deb_v=0;

};


/* PITCH read - interrupt service routine for capturing pitch counter value */
ISR (TIMER1_CAPT_vect) 
{
  deb_p=0;
};



void ticktimer (int ticks)      //Wait for ticks * 32 us
{
  timer=0;
  while(timer<ticks)
    ;
};

/* Initialize pitch and volume values for calibration */
void InitValues(void)           
{
  // Set initial pitch value
  pitch_counter_l=pitch_counter;             // Store actual Timer 1 counter value
  flag_pitch=false;                       // Clear volume flag
  timer=0;
  while(!flag_pitch && (timer < 312))     // Waite for new value (with exit after 10 ms)
    ;
  pitch_init=pitch;       // Counter change since last interrupt = init pitch value

  // Set initial volume value   
  vol_counter_l=vol_counter;            // Store actual counter value
  flag_vol=false;                       // Clear volume flag
  timer=0;
  while(!flag_vol && (timer < 312))     // Waite for new value (with exit after 10 ms)
    ;
  vol_init=vol; // Counter change since last interrupt = init volume value
}


