//using ATtiny to generate 1pps signal
//two critical parameters to configure: a) TIMER0 prescaler: PPS_PS; b) TIMER0 ISR counter: PPS_CNT
//pick PPS_PS and PPS_CNT so that the follow conditions are met:
//1: PPS_PS is one of the valid prescalers, from 1x ... 16384x;
//2: PPS_CNT is a 16-bit number;
//3: F_CLK / (PPS_PS x PPS_CNT) is an integer between 32 and 256.
//
//for example, for F_CLK = 19,440,000Hz, we pick PPS_PS = 64, TMR_TOP = 250, -> PPS_CNT = 19,330,000 / 64 / 250 = 1215
//the above combination generally works for most of the frequencies.

#include "gpio.h"
#include "delay.h"							//we use software delays
#include "tmr0oc.h"							//we use timer0
//#include "tmr1oc.h"							//we use timer1


//hardware configuration
//19,440,000 / 16 / 5000 = 243, [32, 255]
#define F_CLK		(19440000ul/1)			//oscillator timer clock, in HZ
#define PPS_PS		TMR0_PS64x				//8x is also a good choice
#define TMR_TOP		250						//number of ticks for each isr
#define PPS_CNT		(F_CLK / 64 / TMR_TOP)	//number of ISR invocation. needs to be an integer

#define PPS_DLY		(PPS_CNT * 10 / 100)		//cycle counts for 1PPS to be on
#define PPS_PORT	PORTB
#define PPS_DDR		DDRB
#define PPS			(1<<2)					//1PPS output on PB2
//end hardware configuration

//global defines


//global variables
volatile uint16_t cnt=0;

//user code for timer1 isr
void pps_out(void) {

	cnt+=1;									//increment cnt
	if (cnt == PPS_CNT) {
		cnt = 0;							//reset cnt
		//strobe the output pin
		IO_SET(PPS_PORT, PPS);
	}
}

//initialize the pps calibrator
void pps_init(uint32_t ps) {
	//initialize pps low, as output
	IO_CLR(PPS_PORT, PPS);
	IO_OUT(PPS_DDR, PPS);

	//initialize TIMER1
	ps = ps & TMR0_PSMASK;
	tmr0_init(ps);
	switch (ps) {
		case TMR0_PS1x: 	tmr0a_setpr(F_CLK /     1 / PPS_CNT); break;
		//case TMR1_PS2x: 	tmr1a_setpr(F_CLK /     2 / PPS_CNT); break;
		//case TMR1_PS4x: 	tmr1a_setpr(F_CLK /     4 / PPS_CNT); break;
		case TMR0_PS8x: 	tmr0a_setpr(F_CLK /     8 / PPS_CNT); break;
		//case TMR1_PS16x: 	tmr1a_setpr(F_CLK /    16 / PPS_CNT); break;
		//case TMR1_PS32x: 	tmr1a_setpr(F_CLK /    32 / PPS_CNT); break;
		case TMR0_PS64x: 	tmr0a_setpr(F_CLK /    64 / PPS_CNT); break;
		//case TMR1_PS128x: 	tmr1a_setpr(F_CLK /   128 / PPS_CNT); break;
		case TMR0_PS256x: 	tmr0a_setpr(F_CLK /   256 / PPS_CNT); break;
		//case TMR1_PS512x: 	tmr1a_setpr(F_CLK /   512 / PPS_CNT); break;
		case TMR0_PS1024x: 	tmr0a_setpr(F_CLK /  1024 / PPS_CNT); break;
		//case TMR1_PS2048x: 	tmr1a_setpr(F_CLK /  2048 / PPS_CNT); break;
		//case TMR1_PS4096x: 	tmr1a_setpr(F_CLK /  4096 / PPS_CNT); break;
		//case TMR1_PS8192x: 	tmr1a_setpr(F_CLK /  8192 / PPS_CNT); break;
		//case TMR1_PS16384x: tmr1a_setpr(F_CLK / 16384 / PPS_CNT); break;
	}
	tmr0a_act(pps_out);						//install user handler

	//configure the pwm generator
}

int main(void) {

	mcu_init();								//reset the mcu
	pps_init(PPS_PS);						//reset the pss
	ei();									//enable global interrupts
	while(1) {
		//turn off 1pps output
		if (cnt == PPS_DLY) IO_CLR(PPS_PORT, PPS);
	}

	return 0;
}
