//****************************************************************************/
//   Name: Jesse Harris
//   Date: 25OCT2017
//Program: lab4.c
//Purpose: 
//****************************************************************************/


// Expected Connections:
// Bargraph board           Mega128 board 
// --------------      ----------------------    
//     reglck            PORTB bit 0 (ss_n)                      
//     srclk             PORTB bit 1 (sclk)
//     sdin              PORTB bit 2 (mosi)
//     oe_n                   ground
//     gnd2                   ground
//     vdd2                     vcc
//     sd_out               no connect
//
// Encoder board            Mega128 board
// -------------       ----------------------
//    sh/ld_n 					PORTE bit 6 (enbl)
//      clk             PORTB bit 1 (sclk)
//      qh              PORTB bit 3 (miso)    
//    clk_ihn									ground
//      ser                   ground
//      gnd                   ground
//      vdd                   vcc_1
//      qh_n								no connect

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define clr_bit(x,y) (x&=~(1<<y)) //clears a bit
#define set_bit(x,y) (x|=(1<<y))  //sets a bit 
#define minute_mode 0x01
#define hour_mode 0x10

struct time {
		uint16_t second;
		uint16_t minute;
		uint16_t hour;
};

volatile struct time my_time = {0, 0, 0};
volatile uint16_t disp_value = 0; //value to be displayed on the 7seg
volatile uint8_t time_mode = minute_mode;

//holds data to be sent to the segments. logic zero turns segment o

volatile uint8_t segment_data[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12] = {
	0b11000000, //number 0
	0b11111001, //number 1
	0b10100100, //number 2
	0b10110000, //number 3
	0b10011001, //number 4
	0b10010010, //number 5
	0b10000010, //number 6
	0b11111000, //number 7
	0b10000000, //number 8
	0b10011000, //number 9
	0b11111111, //all segments off
	0b11111100  //all segments on (used for testing)
};

//******************************************************************************/
//                            chk_buttons                                      
//Checks the state of the button number passed to it. It shifts in ones till   
//the button is pushed. Function returns a 1 only once per debounced button    
//push so a debounce and toggle function can be implemented at the same time.  
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
//Expects active low pushbuttons on PINA port.  Debounce time is determined by 
//external loop delay times 12. 
//******************************************************************************/
uint8_t chk_buttons(uint8_t button) {
	static uint16_t state[8] = {0}; //holds present state
	state[button] = (state[button] << 1) | (! bit_is_clear(PINA, button)) | 0xE000; //establishes state of button
	if (state[button] == 0xF000) return 1; //if 4 MSB's are high return 1
	return 0;
}
//*****************************************************************************

//                            spi_init                               
//Initalizes the SPI port on the mega128. Does not do any further   
//external device specific initalizations.  Sets up SPI to be:                        
//master mode, clock=clk/2, cycle half phase, low polarity, MSB first
//interrupts disabled, poll SPIF bit in SPSR to check xmit completion
/***********************************************************************/
void spi_init(void){
	DDRB  |= 0x07;   //Turn on SS, MOSI, SCLK
	SPCR  |= (1 << SPE) | (1 << MSTR);  //enable SPI, master mode 
	SPSR  |= (1 << SPI2X); // double speed operation
}//spi_init

/***********************************************************************/
//                              tcnt0_init                             
//Initalizes timer/counter1 (TCNT1). TCNT1 is running in async mode
//with external 32khz crystal.  Runs in normal mode with no prescaling.
//Interrupt occurs at overflow 0xFF.
//
void tcnt0_init(void){
	TIMSK |= (1 << TOIE0); //enable TCNT0 overflow interrupt
	TCCR0 |= (1 << CS02) | (1 << CS00); //normal mode, prescale of 128
	ASSR  |= _BV(AS0);
}

/***********************************************************************/
//                              tcnt2_init                             
//Initalizes timer/counter0 (TCNT1). TCNT1 is running in async mode
//Runs in normal mode with no prescaling.
//Interrupt occurs at overflow 0xFF.
//*********************************************************************/
void tcnt2_init(void){
	TIMSK  |=  (1 << TOIE2); //enable TCNT2 overflow interrupt
	//Normal mode, 128 pre-scale, Set OC2 on compare, clear OC2 at BOTTOM
	TCCR2 |= (1 << CS21)|(1 << CS20)|(1 << COM21)|(1 << COM20);
	TCCR2 |= (1 << WGM21)|(1 << WGM20);
}

/************************************************************************************/



/************************************************************************************/
void adc_init() {
	ADMUX |= (1 << REFS0)|(1 << MUX2)|(1 << MUX0);
	ADCSRA |= (1 << ADEN)|(1 << ADPS2)|(1 << ADPS1)|(1 << ADPS0)|(1 << ADIE);
  ADCSRA |= (1 << ADSC);
}
//***********************************************************************************
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
//***********************************************************************************
void segsum(uint16_t sum) {
	//break up decimal sum into 4 digit-segments
	uint8_t digit3 = dec_to_7seg[(sum/1000)];     //thousands place
	uint8_t digit2 = dec_to_7seg[(sum/100) % 10]; //hundreds place
	uint8_t digit1 = dec_to_7seg[(sum/10)  % 10]; //tens place
	uint8_t digit0 = dec_to_7seg[(sum/1)   % 10]; //ones place 

//	//blank out leading zero digits
//	if (sum < 1000) { 
//		digit3 = dec_to_7seg[10]; //blank out thousands place
//		if (sum < 100)  {  //blank out hundreds place      
//			digit2 = digit3;
//			if (sum < 10) {digit1 = digit2;} //blank out tens place
//		}
//	} 
	//now move data to right place for misplaced colon position
	segment_data[0] = digit0;
	segment_data[1] = digit1;
	segment_data[3] = digit2;
	segment_data[4] = digit3;
}//segment_sum

/*************************************************************************/
//                              left_encoder
//Takes the past encoder value and the present encoder value and shifts                                                             
//them into an eight bit integer. This value is represented as a case in
//a switch statement. Each case is either skipped or increments the 
//display value in accord with the selected state. 
/*************************************************************************/
//void left_encoder(uint8_t past_encoder, uint8_t  encoder) {
//	uint8_t direc = (past_encoder << 2 | encoder);
//	switch (direc) { //Determine which transistions must be skipped in order to comply with disply mode
//		case 0x01: 
//		case 0x0E:	  
//		case 0x08: 
//		case 0x07: OCR2++;
//							 break;
//		//Decrement
//		case 0x04: 
//		case 0x0B: 
//		case 0x02: 
//		case 0x0D: OCR2--;
//		default: break;
//	}//switch
//}//right_encoder

/******************************************************************************/
//                             right_ encoder
//Takes the past encoder value and the present encoder value and shifts                                                             
//them into an eight bit integer. This value is represented as a case in
//a switch statement. Each case is either skipped or increments the 
//display value in accord with the selected state. 
/*****************************************************************************/
void right_encoder(uint8_t past_encoder, uint8_t  encoder) {
	uint8_t direc = (past_encoder << 2 | encoder);
	switch (direc) { //Determine which transistions must be skipped in order to comply with disply mode
		case 0x07: if (time_mode == minute_mode) {my_time.minute++;} //increment if in quad mode
							 else if (time_mode == hour_mode) {my_time.hour++;}	 
							  break;
		//Decrement
		case 0x0D: if (time_mode == minute_mode) {my_time.minute--;} //increment in quad
							 else if (time_mode == hour_mode) {my_time.hour--;}
							  break;
		default: break;
	}//switch
}//right_encoder
/*************************************************************************/
//                           timer/counter0 ISR                          
//When the TCNT0 overflow occurs the second is incremented. If 60 seconds
//is hit the clock rolls over and one minute is added. Likewise, if 60 
//minutes is hit the clock rolls over and the hour increments. This 
//function is also responsible for flashing the colon every second. 
/*************************************************************************/

ISR(TIMER0_OVF_vect) {	
  static uint8_t j = 0; //variable for colon display
	
	my_time.second++; 
	if (my_time.second > 59) { //add one minute
		my_time.minute++;
		if (my_time.minute > 59) { //roll over minutes and add one hour
			my_time.hour++;
			my_time.minute = 0;
		}
		my_time.second = 0; //roll over seconds
	}

	//Blink the colon
	if (j == 0)
 		segment_data[2] = dec_to_7seg[11]; //colon is illuminated
	else
    segment_data[2] = dec_to_7seg[10]; //colon is off
	j++; 
	
	if (j > 1) {j = 0;}
}


void display_time () {
	disp_value = (my_time.hour * 100) + my_time.minute;
}

/******************************************************************************/
//                           timer/counter2 ISR                          
//When the TCNT0 overflow interrupt occurs, the count_7ms variable is    
//incremented. Every 7680 interrupts the minutes counter is incremented.
//TCNT0 interrupts come at 7.8125ms internals. write to bg, read from encoders
// 1/32768         = 30.517578uS
//(1/32768)*256    = 7.8125ms
//(1/32768)*256*64 = 500mS
//*************************************************************************/
ISR(TIMER2_OVF_vect) {
	static uint8_t encoder = 0; //stores current encoder state value (11, 10, 00, 01)
	static uint8_t past_encoder = 0; //stores previous encoder state value
	static uint8_t j = 0; //segment display variable
	//Load data from the encoders
	clr_bit(PORTE, PE6); //load data in the 165
	set_bit(PORTE, PE6); //shift data out the 165
	
	DDRA = 0x00;  //intitialize PORTA to inputs
	PORTA = 0xFF; //enable pull-ups
	PORTB = 0x70;
	//Check the buttons (0 and 1)
	for(uint8_t i=0; i < 3; i++) {
		if(chk_buttons(i)) { //if button is pressed
			switch(i) { //cases for buttons pressed
				case 2: time_mode = minute_mode; //button 0 pressed
								break;	
				case 1: time_mode = hour_mode; //button 1 pressed
								break;
			}//switch
		}//if			
	}//for

	SPDR = my_time.second;
	while (bit_is_clear(SPSR, SPIF)){} //wait until the end of the load
	PORTB |= 0x01;  //rising edge for ss_n pin on 595
	PORTB &= ~0x01; //falling edge for ss_n pin on 595

	//Shift data from MISO into SPDR
	encoder = SPDR; //set encoder equal to the SPI data register
	encoder &= (0x0F); //set all encoder bits (3:0) high

	segsum(disp_value); //call segsum
	right_encoder((past_encoder & 0x0C) >> 2, (encoder & 0x0C) >> 2); //encoder 1 first pair of bits
 // left_encoder((past_encoder & 0x03), (encoder & 0x03));

  if (my_time.hour > 24) {my_time.hour = 0;}
	if (my_time.minute > 59) {my_time.minute = 0;}
  display_time();
	past_encoder = encoder; //remember current state for next interrupt

	//Loop through segments
	if (j > 4) {j = 0;}
	DDRA = 0xFF;
	PORTA = segment_data[j];
	PORTB = (j << 4);
	j++;
}//ISR

/***********************************************************************/


/***********************************************************************/
ISR(ADC_vect) {
	OCR2 = (ADC >> 2);
	ADCSRA |= (1 << ADSC);
}

/***********************************************************************/
//                                main                                 
/***********************************************************************/
int main(){     
	DDRB |= 0xF0; //bits 4-7 outputs
	DDRE = 0x40;  //set bit 6 to output

	tcnt2_init(); //initialize timer/counter two
	tcnt0_init(); //initialize timer/counter zero
	spi_init();   //initialize SPI port
	adc_init();   //initialize ADC

	sei();        //enable interrupts before entering loop

	while(1){
	}     //empty main while loop

} //main
