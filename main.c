//using ATtiny to generate 1pps signal
//
//
//Using a clock (external or crystal) to generate 1PPS pulses
//
//connection:
//
//
//
//                    |---------------------|
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//   F_OSC----------->| OSCI        PPS_PIN |---------> 1PPS
//                    |                     |
//                    |                     |
//                    |   ATtiny25/45/85    |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |                     |
//                    |---------------------|
//
//
//
//FOUR parameters to pick:
//1. PS_FUSE: 	fuse setting for divide-by-8 (CKDIV8 bit). 1 (not programmed) or 8 (programmed). ****This setting will impact fuse programming****
//2. PS_TMR: 	TMR0 clock divider setting. 1/8/64/256/1024.
//3. TMR_TOP:	timer ticks between each ISR invocation
//4. ISR_CNT:	the number of ISR invocations needed for each 1PPS pulse
//
//other parameters the user must specify:
//5. F_OSC:		frequency of external oscillator
//6. PPS_DC:	controls the on duration of the 1PPS signal
//7. PPS_PIN:	1pps output pin/pins. Signal on the rising edge. Falling edge may have jitter when other programs are running.
//
//the following conditions ***MUST*** be true:
//
//   F_OSC = PS_FUSE * PS_TMR * TMR_TOP * ISR_CNT
//
//if not, the programm will generate an error message
//
//general suggestions:
//1. pick PS_FUSE to 8 (default)
//2. select an as large PS_TMR and TMR_TOP as you can
//3. if ISR_CNT is less than 256, change its type to uint8_t
//
//examples for popular frequencies
//  24,00Mhz = 8 * 8 * 250 * 1500 		(slightly out of spec but runs reliably)
//  20,00Mhz = 8 * 8 * 250 * 1250
//  19,80Mhz = 8 * 8 * 125 * 2475
//  19,68Mhz = 8 * 8 * 250 * 1250
//  19,44Mhz = 8 * 8 * 243 * 1230
//  19,20Mhz = 8 * 8 * 250 * 1200
//  18,432Mhz= 8 *64 * 250 *  144
//  17,328Mhz= 8 * 8 * 250 * 1083
//  16,80Mhz = 8 * 8 * 250 * 1050
//  16,384Mhz= 8 *64 * 250 *  128
//  16,384Mhz= 8 * 8 * 250 * 1024
//  16,20Mhz = 8 *64 * 125 * 2025
//  16,00Mhz = 8 *64 * 250 *  125
//  15,60Mhz = 8 * 8 * 250 *  975
//  15,36Mhz = 8 *64 * 250 *  250
//  14,40Mhz = 8 * 8 * 250 *  900
//  13,824Mhz= 8 * 8 * 250 *  864
//  12,96Mhz = 8 * 8 * 250 *  810
//  12,80Mhz = 8 *64 * 250 *  100
//  12,288Mhz= 8 *64 * 250 *   96
//  12,00Mhz = 8 * 8 * 250 *  750
//  11,52Mhz = 8 *64 * 250 *   90
//  10,368Mhz= 8 *64 * 250 *   81
//  10,00Mhz = 8 * 8 * 250 *  625
//   9,60Mhz = 8 *64 * 250 *   75
//   8,00Mhz = 8 * 8 * 250 *  500
//	...
//

#include "gpio.h"
#include "delay.h"							//we use software delays
#include "tmr0oc.h"							//we use timer0
//#include "tmr1oc.h"						//we use timer1


//hardware configuration
#define F_OSC		19440000ul				//external oscillator speed
#define PS_FUSE		8						//8 (default) or 1: fuse setting for 8x divider.
#define PS_TMR		8						//1/8/64/256/1024: clock divider setting for TMR0
#define TMR_TOP		243						//steps in which TMR0 output compare advances
#define ISR_CNT		1250					//number of ISR invocation for each 1PPS pulse
#define PPS_DC		10						//1PPS on / high duration - between 1 and ISR_CNT

#define PPS_PORT	PORTB
#define PPS_DDR		DDRB
#define PPS_PIN		(1<<2)					//1PPS output on PB2. Multiple pins are allowed
//end hardware configuration

//global defines
//set fuse clock divider
#if PS_FUSE == 8
#define F_CLK		(F_OSC/8)				//oscillator timer clock, in HZ
#else
#define F_CLK		(F_OSC/1)
#endif

//checking for error conditions
//set TMR0 to valid prescalers
#if PS_TMR == 1
#define PPS_PS		TMR0_PS1x				//8x is also a good choice
#elif PS_TMR == 8
#define PPS_PS		TMR0_PS8x				//8x is also a good choice
#elif PS_TMR == 64
#define PPS_PS		TMR0_PS64x				//8x is also a good choice
#elif PS_TMR == 256
#define PPS_PS		TMR0_PS256x				//8x is also a good choice
#elif
#define PPS_PS		TMR0_PS1024x			//8x is also a good choice
#else
#error "Invalid PS_TMR settings!"
#endif

//check to see if F_OSC = PS_FUSE * PS_TMR * TMR*TOP * ISR_CNT
//report error if not equal
#if F_OSC != PS_FUSE * PS_TMR * TMR_TOP * ISR_CNT
#error "F_OSC not divisable by PS_FUSE, PS_TMR, TMR_TOP, and ISR_CNT"
#endif

//check if TMR_TOP is more than 8bit
#if TMR_TOP > 255
#error "TMR_TOP is too large: it must be between 32 - 255"
#endif

//check if ISR_CNT is too large
#if ISR_CNT > 65536-1
#error "ISR_CNT is too large: it must be between 1 - 65536"
#endif

//make sure PPS_DC is less than ISR_CNT
#if PPS_CN > ISR_CNT
#error "PPS_DC is too big: it must be less than ISR_CNT"
#endif
//end error checking

//global variables
volatile uint16_t cnt=ISR_CNT;

//user code for timer1 isr
void pps_out(void) {

	cnt-=1;										//decrement cnt - downcounter
	if (cnt == 0) {								//if enough isr invocations have passed
		cnt = ISR_CNT;							//reset cnt
		//strobe the output pin
		IO_SET(PPS_PORT, PPS_PIN);
	}
}

//initialize the pps calibrator
void pps_init(uint32_t ps) {
	//initialize isr counter
	cnt = ISR_CNT;

	//initialize pps low, as output
	IO_CLR(PPS_PORT, PPS_PIN);
	IO_OUT(PPS_DDR, PPS_PIN);

	//initialize TIMER1
	ps = ps & TMR0_PSMASK;
	tmr0_init(ps);							//initialize TMR0
	//set TMR0 OCRA advance
	//switch (ps) {
	//	case TMR0_PS1x: 	tmr0a_setpr(F_CLK /     1 / ISR_CNT); break;
	//	case TMR0_PS8x: 	tmr0a_setpr(F_CLK /     8 / ISR_CNT); break;
	//	case TMR0_PS64x: 	tmr0a_setpr(F_CLK /    64 / ISR_CNT); break;
	//	case TMR0_PS256x: 	tmr0a_setpr(F_CLK /   256 / ISR_CNT); break;
	//	case TMR0_PS1024x: 	tmr0a_setpr(F_CLK /  1024 / ISR_CNT); break;
	//}
	tmr0a_setpr(TMR_TOP);					//alternatively
	tmr0a_act(pps_out);						//install user handler
	//1PPS generator now running
	//needs to enable global interrupt in main()
}

int main(void) {

	mcu_init();								//reset the mcu
	pps_init(PPS_PS);						//reset the pss
	ei();									//enable global interrupts
	while(1) {
		//turn off 1pps output
		if (cnt == ISR_CNT - PPS_DC) IO_CLR(PPS_PORT, PPS_PIN);
	}

	return 0;
}
