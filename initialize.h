#ifndef INITIALIZE_H
#define INITIALIZE_H

//******************************************************************************/
//                            spi_init                               
//Initalizes the SPI port on the mega128. Does not do any further   external
//device specific initalizations.  Sets up SPI to be:
//master mode, clock=clk/2, cycle half phase, low polarity, MSB first
//interrupts disabled, poll SPIF bit in SPSR to check xmit completion
//******************************************************************************/
void spi_init(void){
	DDRB  |= 0x07;   //Turn on SS, MOSI, SCLK
	SPCR  |= (1 << SPE) | (1 << MSTR);  //enable SPI, master mode 
	SPSR  |= (1 << SPI2X); // double speed operation
}//spi_init

//******************************************************************************/
//                              tcnt0_init                             
//Initalizes timer/counter0 (TCNT0). TCNT0 is running in async mode with
//external 32khz crystal.  Runs in normal mode with a 128 prescale.  Interrupt
//occurs at overflow 0xFF. This interrupt is responsible for handling the clock
//time. 
//******************************************************************************/
void tcnt0_init(void){
	TIMSK |= (1 << TOIE0); //enable TCNT0 overflow interrupt

	//Normal mode, 128 prescale, clock from 32kHz crystal
	TCCR0 |= (1 << CS02) | (1 << CS00); 
	ASSR  |= (1 << AS0); 
}

//******************************************************************************/
//															tcnt1_init
//Initializes timer/counter1 (TCNT1). TCNT1 is running in compare mode on the 
//internal 16MHz clock and willtrigger an ISR when the counter reaches the value 
//of the OCR1A register. The OCRIA value is chosen in order to generate a 
//desired frequency that will be the output sound of the alarm.
//******************************************************************************/
void tcnt1_init(void) {
	OCR1A = 0x009C; //initialize to 156
	TIMSK |= (1 << OCIE1A); //enable TCNT1 output compare interrupt enable bit

	//CTC mode with a prescale of 64
	TCCR1B |= (1 << CS11)|(1 << CS10)|(1 << WGM12);
}

//******************************************************************************/
//                              tcnt2_init                             
//Initalizes timer/counter2 (TCNT2). TCNT2 is running in normal mode and enables
//the ISR on overflow. It has a prescale of 128 and is enable in fast PWM mode. 
//It sets the OC2 bit on compare. This timer/counter is responsible 
//******************************************************************************/
void tcnt2_init(void){
	TIMSK  |=  (1 << TOIE2); //enable TCNT2 overflow interrupt

	//Normal mode, 128 pre-scale, Set OC2 on compare, clear OC2 at BOTTOM
	TCCR2 |= (1 << CS21)|(1 << CS20)|(1 << COM21)|(1 << COM20);
	TCCR2 |= (1 << WGM21)|(1 << WGM20);
}


//******************************************************************************/
//															tcnt3_init
//Initializes timer/counter3 (TCNT3). TCNT3 is set in fast PWM mode with a 
//prescale of 64. It is responsible for sending a PWM signal to the volume 
//on the audio amplifier board. This timer/counter does not have and ISR and 
//the value in OC3RA is adjusted with the left encoder. 
//******************************************************************************/
void tcnt3_init(void) {
	TCCR3A |= (1 << COM3A1)|(1 << WGM30); //clear on compare match
	TCCR3B |= (1 << WGM32)|(1 << CS31);
	DDRE |= (1 << PE3); //enable output pin
	OCR3A = 0x7F;
}

//******************************************************************************/
//                                 adc_init
//Initializes the analog to digital converter to pin 6 on PORTF. Enables the
//ADC, sets a prescale, and then sets the interrupt enable. Only after these
//conditions can the ADC begin its conversion.
//******************************************************************************/
void adc_init() {
	//Vcc internal to Atmega128, ADC PIN 6 (PORTF)
	ADMUX |= (1 << REFS0)|(1 << MUX2)|(1 << MUX0);

	//Enable ADC, prescale, ADC interrupt enable
	ADCSRA |= (1 << ADEN)|(1 << ADPS2)|(1 << ADPS1)|(1 << ADPS0)|(1 << ADIE);
	ADCSRA |= (1 << ADSC); //Start conversion
}

//******************************************************************************/
//															radio_init
//Ititializes the radio interrupt and deals with resets to generate a state
//that can operate the radio. 
//******************************************************************************/
void radio_init() {
		PORTE |= 0x08;
		DDRE |= 0x08;

		//PE2 is active high reset
		DDRE |= 0x04;
		PORTE |= 0x04;

		//Enable interrupt 7
		EICRB |= (1 << ISC71) | (1 << ISC70);
		EIMSK |= (1 << INT7);

		//Hardware reset of si4734
		PORTE &= ~(1 << PE7); //set low to sense TWI mode
		DDRE |= 0x80; 
		PORTE |= (1 << PE2); //hardware reset
		_delay_us(200);
		PORTE &= ~(1 << PE2); //release reset
		_delay_us(30);
		DDRE &= ~(0x80); //PE7 to act as input from radio interrupt
}
#endif
