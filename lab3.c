//****************************************************************************/
//   Name: Jesse Harris
//   Date: 25OCT2017
//Program: lab3.c
//Purpose: The purpose of this program is to utilize the encoder, bar graph
//				 and the 7 segment display as I/O peripherals on the ATMega128.
//				 The encoders will increment and decrement the count depending
//				 on the rotation direction of the encoder. The count will appear
//				 on the 7 segment display and it will increment by one on default.
//				 The user can use push buttons 0 and 1 to select a count by two
//				 and count by four mode, respectively; and either two or four leds
//				 on the bar graph will be illuminated to display the mode. If both
//				 modes are selected simultaneously, then there will be eight leds
//				 illuminated on the bar graph and the encoders will neither 
//				 incrmenet, nor decrement. 
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

//holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5];

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12] = {
	0b11000000, //number 0
	0b11111001, //number 1
	0b10100100, //number 2
	0b10110000, //number 3
	0b10011001, //number 4
	0b10010010, //number 5
	0b10000011, //number 6
	0b11111000, //number 7
	0b10000000, //number 8
	0b10011000, //number 9
	0b11111111, //all segments off
	0b00000000  //all segments on (used for testing)
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

/***********************************************************************/
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
//Initalizes timer/counter0 (TCNT0). TCNT0 is running in async mode
//with external 32khz crystal.  Runs in normal mode with no prescaling.
//Interrupt occurs at overflow 0xFF.
//
void tcnt0_init(void){
	TIMSK  |=  (1 << TOIE0); //enable TCNT0 overflow interrupt
	TCCR0  |=  (1 << CS02) | (1 << CS00); //normal mode, prescale of 128
	ASSR   |= _BV(AS0);
}

void tcnt2_init(void) {
	TIMSK |= (1 << TOIE2);
	TCCR2 |= (1 << CS21) | (1 << CS20);
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

	//blank out leading zero digits
	if (sum < 1000) { 
		digit3 = dec_to_7seg[10]; //blank out thousands place
		if (sum < 100)  {  //blank out hundreds place      
			digit2 = digit3;
			if (sum < 10) {digit1 = digit2;} //blank out tens place
		}
	} 
	//now move data to right place for misplaced colon position
	segment_data[0] = digit0;
	segment_data[1] = digit1;
	segment_data[2] = dec_to_7seg[10];
	segment_data[3] = digit2;

	segment_data[4] = digit3;
}//segment_sum

/*************************************************************************/
//                             encoder_exchange
//Takes the past encoder value and the present encoder value and shifts                                                             
//them into an eight bit integer. This value is represented as a case in
//a switch statement. Each case is either skipped or increments the 
//display value in accord with the selected state. 
/*************************************************************************/
void encoder_exchange(uint8_t past_encoder, uint8_t  encoder,uint8_t display_mode, uint16_t *disp_value) {
	uint8_t direc = (past_encoder << 2 | encoder);
	if (display_mode == 0x11) {direc = 0xFF;}
	switch (direc) { //Determine which transistions must be skipped in order to comply with disply mode
		case 0x01: (*disp_value)++; //this transistion will always increment; no matter the mode
							 break;
		case 0x0E: if (display_mode == 0x01 || display_mode == 0x10) {(*disp_value)++;} //increment if in quad or double mode
								  break;
		case 0x08: 
		case 0x07: if (display_mode == 0x10) {(*disp_value)++;} //increment if in quad mode
								  break;
							 //Decrement
		case 0x04: (*disp_value)--;  //this transistion will always deccrement; no matter the mode
							 break;
		case 0x0B: if (display_mode == 0x01 || display_mode == 0x10) {(*disp_value)--;} //increment if in quad or double mode
								  break;
		case 0x02: 
		case 0x0D: if (display_mode == 0x10) {(*disp_value)--;} //increment in quad
								  break;
		default: break;
	}//switch
}//encoder_exchange

/*************************************************************************/
//                           timer/counter0 ISR                          
//When the TCNT0 overflow interrupt occurs, the count_7ms variable is    
//incremented. Every 7680 interrupts the minutes counter is incremented.
//TCNT0 interrupts come at 7.8125ms internals. write to bg, read from encoders
// 1/32768         = 30.517578uS
//(1/32768)*256    = 7.8125ms
//(1/32768)*256*64 = 500mS
/*************************************************************************/
ISR(TIMER2_OVF_vect) {
	static uint8_t display_mode = 0; //variable to store selected mode (single, double, quad, disable)
	static uint8_t encoder = 0; //stores current encoder state value (11, 10, 00, 01)
	static uint8_t past_encoder = 0; //stores previous encoder state value
	static uint8_t j = 0; //segment display variable
	static int16_t disp_value = 0; //value to be displayed on the 7seg

	//Initialize ports
	DDRA = 0x00;  //intitialize PORTA to inputs
	PORTA = 0xFF; //enable pull-ups
	DDRB |= 0xF0; //bits 4-7 outputs
	PORTB = 0x70; //enable pull-ups on bits 4-6
	DDRE = 0x40;  //set bit 6 to output

	//Load data from the encoders
	clr_bit(PORTE, PE6); //load data in the 165
	set_bit(PORTE, PE6); //shift data out the 165

	//Check the buttons (0 and 1)
	for(uint8_t i=0; i < 2; i++) {
		if(chk_buttons(i)) { //if button is pressed
			switch(i) { //cases for buttons pressed
				case 0: display_mode ^= 0x01; //button 0 pressed
								break;	
				case 1: display_mode ^= 0x10; //button 1 pressed
								break;
			}//switch
		}//if			
	}//for

	switch(display_mode) { //cases for bar graph led indication
		case 0x00: SPDR = 0x01; //display one leds
							 break;
		case 0x01: SPDR = 0x03; //display two leds
							 break;
		case 0x10: SPDR = 0x0F; //display four leds
							 break;
		case 0x11: SPDR = 0xFF; //display all leds
							 break;
		default: SPDR = 0x55; break; //default 
	}
	while (bit_is_clear(SPSR, SPIF)){} //wait until the end of the load
	PORTB |= 0x01;  //rising edge for ss_n pin on 595
	PORTB &= ~0x01; //falling edge for ss_n pin on 595

	//Shift data from MISO into SPDR
	encoder = SPDR; //set encoder equal to the SPI data register
	encoder &= (0x0F); //set all encoder bits (3:0) high

  //Call encoder exchange on both encoders.
	//The static integer disp_value is passed by reference for change in the function
	encoder_exchange(past_encoder & 0x03, encoder & 0x03, display_mode, &disp_value); //encoder 1 first pair of bits
	encoder_exchange((past_encoder & 0x0C) >> 2, (encoder & 0x0C) >> 2, display_mode, &disp_value); //encoder 2 second pair of bits

	//Roll over counters
	if (disp_value > 1023) {disp_value = 1;} 
	if (disp_value < 0) {disp_value = 1023;}

	segsum(disp_value); //call segsum
	past_encoder = encoder; //remember current state for next interrupt

	//Loop through segments
	if (j > 4) {j = 0;}
	DDRA = 0xFF;
	PORTA = segment_data[j];
	PORTB = (j << 4);
	j++;
}//ISR

/***********************************************************************/
//                                main                                 
/***********************************************************************/
int main(){     
//	tcnt0_init();  //initalize counter timer zero
	tcnt2_init();
	spi_init();    //initalize SPI port
	sei();         //enable interrupts before entering loop
	while(1){
	}     //empty main while loop

} //main
