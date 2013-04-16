
// OPEN.THEREMIN.UNO Code
// V1.0
// by Urs Gaudenz, 2012 

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/delay.h>

#include "mcpDac.h"
#include "theremin_sintable.c"

#define F_CPU 16000000UL

int32_t pitch_init = 0; 			// Initial value of pitch
int32_t vol_init = 0;				// Initial value of volume
int32_t pitch_v,pitch_l;			// Last value of pitch
int32_t vol_v,vol_l;				// Last value of volume
uint8_t i;
uint8_t state = 0;
uint16_t val[8]; 

int16_t t;

/* volatile  - used in the ISR Routine*/ 

volatile uint16_t pitch = 0;			// Pitch value
volatile uint16_t pitch_counter = 0; 	// Pitch counter
volatile uint16_t pitch_counter_l = 0; 	// Last value of pitch counter
volatile uint16_t vol = 0;  			// Volume value
volatile uint16_t vol8;					// Volume byte
volatile uint16_t vol_counter = 0; 		// Volume counter
volatile uint16_t vol_counter_l = 0; 	// Last value of volume counter

volatile uint8_t flag_pwm = 0;			// PWM flag
volatile uint8_t flag_vol = 0;			// Volume read flag
volatile uint8_t flag_pitch = 0;		// Pitch read flag
volatile uint16_t pointer = 0;			// Table pointer
volatile uint16_t add_val = 0;			// Table pointer increment
volatile uint8_t mode = 4; 			// Mode value
volatile uint16_t timer = 0; 			// Timer value

SIGNAL (TIMER2_OVF_vect)                          // Timer 2 -  WAVE generator 34us
{
  TCNT2 = 192;
  
  volatile  int32_t temp_val;
  volatile  uint32_t temp2_val;
  temp_val = (signed int)pgm_read_word_near (sine_table + ((unsigned int)(pointer>>6) & 0x3ff));  //3.2us
  
   // PORTD |= (1<<PD3);
   
 if (temp_val>0){                   //13us
   temp2_val=(temp_val*vol8);
   temp2_val=temp2_val>>13; 
  //  temp2_val=(temp_val);
   //temp2_val=temp2_val>>3; 
   temp2_val=temp2_val+2048;  
 } else
 { 
   temp2_val=-(temp_val*vol8);
   temp2_val=temp2_val>>13;
   //temp2_val=-(temp_val);
   //temp2_val=temp2_val>>3;
   temp2_val=2048-temp2_val;
 }
 
//PORTD &= ~(1<<PD3);

  mcpDacSend(temp2_val);  //9.6 us

  //  mcpDacSend(500+((temp_val*vol8)>>3));
  pointer = pointer + add_val;				// increment table pointer (ca. 3us)
  timer++;


   
}


SIGNAL (TIMER1_CAPT_vect)                          // PITCH read - interrupt service routine for Input Capture
{
pitch_counter = ICR1;

//pitch_counter =OCR0A;                                // Read actual pitch counter value low byte
//pitch_counter |= ((unsigned int)OCR0B << 8);         // Read actual counter value high byte

pitch=(pitch_counter-pitch_counter_l);               // Counter change since last interrupt
pitch_counter_l=pitch_counter;                       // Set actual value as new last value

flag_pitch=1;                                        // Set new pitch value flag


   
};


SIGNAL (INT0_vect) // VOLUME read - interrupt service routine for comparator interrupt 
{	


  
vol_counter = TCNT1;

vol=(vol_counter-vol_counter_l);                     // Counter change since last interrupt 
vol_counter_l=vol_counter;                           // Set actual value as new last value

flag_vol=1;                                          // Set new volume value flag

};


void ticktimer (int ticks)
{
  timer=0;while(timer<ticks);
};

  
void setup() {                


pinMode(3, OUTPUT);    
digitalWrite(3, LOW);   // set the LED on


  
  
// Timer 0, 15.625 kHz Interrupt

TIMSK0 = 0; // Turn of Timer 0

// Set Timer 2 for Wave generatuor
TCCR2A =0; // Set to default
TCCR2B = (1<<CS21); // Set clkI/8
TIMSK2 = (1<<TOIE2); // Enable Timer/Counter0 Overflow Interrupt 

// Timer 1, 16 bit timer used to measure pitch and volume frequency 

TCCR1A = 0;
TCCR1B = (1<<ICES1)|(1<<CS10);// |(1<<ICNC0);  Input Capture Positiv Edge Select, Noise Canceler off
TIMSK1 = (1<<ICIE1); // Enable Timer 0 Input Capture Interrupt 

PORTD = (1<<PORTD4); //Set Pull-Up on BUT

EICRA = (1<<ISC00)|(1<<ISC01); // The rising edge of INT0 generates an interrupt request.
EIMSK = (1<<INT0); // Enable External Interrupt




sei (); // Enable Interrupts

mcpDacInit(); // Initialize Digital Analog Converter (DAC)


vol8=4095;

	add_val = 500;					// Play welcome sounds
	ticktimer(15000);
	add_val = 1000;
	ticktimer(15000);
	add_val = 2000;
	ticktimer(30000);

   	pitch_init=pitch;				// Set initial pitch value
   	vol_init=vol;					// Set initial volume value

}

void InitValues(void)

{
	vol8=4095;
	add_val = 700;					// Play init sounds
	ticktimer(15000);
	vol8=0;
	ticktimer(3000);
	vol8=4095;
	ticktimer(15000);
	vol8=0;
	ticktimer(3000);
	vol8=4095;				
	ticktimer(15000);
	vol8=0;
	ticktimer(3000);
	add_val = 2000;
	vol8=4095;
	ticktimer(3000);

   	pitch_init=pitch;				// Set initial pitch value
   	vol_init=vol;					// Set initial volume value
}


void loop() {


	

mloop: 								// Main loop avoiding the GCC "optimization"	

if ((state==0)&&((PIND&(1<<PORTD4))==0))
	{state=1;
	timer=0;}

if ((state==1)&&((PIND&(1<<PORTD4))!=0))
	{if (timer > 1500)
	{InitValues();
	state=0;
	mode=4;} else {state=0;}};
	

if ((state==1)&&(timer>20000))
	{
	state=0;
	mode++;

	if (mode>4){mode=1;};
	for (i=0;i<mode;i++)

	{
	vol8=4095;
	add_val = 3000;	
	ticktimer(1500);
	vol8=0;
	ticktimer(1500);
	vol8=4095;
	add_val = 0;	
	};

	

	while((PIND&(1<<PORTD4))==0)
	{};
	};


/* New VOLUME value */

if (flag_vol){

	vol_v=vol;							// Averaging volume values
	vol_v=vol_l+((vol_v-vol_l)>>2);
	vol_l=vol_v;

	//if ((vol_v>15000)&(vol_v>17000)) {PORTA |= (1<<PA1);} else {PORTA &= ~(1<<PA1);} // LED on if value in range

	//OCR1B=vol_v&0xff;							// Set volume CV value

	//OCR1D=(vol_v>>2)&0xff;

	switch (mode)

	{
		case 1:vol_v=4095;break;// Set pointer incerement 
		case 2:add_val=33554432/vol_v;vol_v=4095;break;	// frequence to add_val
		case 3:vol_v=4095;break;
		case 4:vol_v=4095-(vol_init-vol_v);break;
	};

	/* Limit and set value*/

	if (vol_v>4095) {vol8=4095;} else
	if (vol_v<0) {vol8=0;} else {
	vol8=vol_v;}


	flag_vol=0;							// Clear volume flag

}


/* New PITCH value */

if (flag_pitch){

	//if ((pitch>19000)&(pitch<21000)) {PORTA |= (1<<PA0);} else {PORTA &= ~(1<<PA0);}	// LED on if value in range
	//OCR1B=((pitch>>6)+200)&0xff;		// Set pitch CV Value


	pitch_v=pitch;						// Averaging pitch values
	pitch_v=pitch_l+((pitch_v-pitch_l)>>2);	
	pitch_l=pitch_v;	


	//OCR1D=0x80+(pitch_v>>4)%0x80;
	//OCR1B=(pitch)&0xff;


	switch (mode)
	{
		case 1:add_val=33554432/pitch_v;break; // frequence to add_val
		case 2:break;
		case 3:add_val=(pitch_init-pitch_v)/2+200;break;
		case 4:add_val=(pitch_init-pitch_v)/2+200;break;
	};

	flag_pitch=0;						// Clear pitch flag
}

goto mloop; 							// End of main loop	

}





