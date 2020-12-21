/**
 *
 * @file main.c
 * @brief Snake game
 * @author Nikita Zhukov xzhuko01@vutbr.cz
 * @date 20.12.2020
 */

#include "MK60D10.h"
#include <stdbool.h>

/* Macros for bit-level registers manipulation. */
#define GPIO_PIN_MASK 0x1Fu
#define GPIO_PIN(x) (((1)<<(x & GPIO_PIN_MASK)))

/* Mapping of LEDs and buttons to specific port pins: */
#define BTN_SW2 0x400     // Port E, bit 10
#define BTN_SW3 0x1000    // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800     // Port E, bit 11

/* Constants specifying delay loop duration. */
#define	tdelaylong		10000
#define tdelaymedium 	20
#define tdelayshort     1

#define UP	5
#define RIGHT 2
#define DOWN 3
#define LEFT 4
#define HALT 6

#define SNAKE_LENGTH 5

int ROWS[8] = {
	GPIO_PIN(26), // row 0
	GPIO_PIN(24), // row 1
	GPIO_PIN(9),  // row 2
	GPIO_PIN(25), // row 3
	GPIO_PIN(28), // row 4
	GPIO_PIN(7),  // row 5
	GPIO_PIN(27), // row 6
	GPIO_PIN(29)  // row 7
};

int snake_bitmap[SNAKE_LENGTH][2];
int current_btn;
unsigned int compare = 0x200;

/* Conversion of requested column number into the 4-to-16 decoder control.  */
void column_select(unsigned int col_num)
{
	unsigned i, result, col_sel[4];

	for (i = 0; i<4; i++) {
		result = col_num / 2;	  // Whole-number division of the input number
		col_sel[i] = col_num % 2;
		col_num = result;

		switch(i) {

			// Selection signal A0
		    case 0:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(8))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(8)));
				break;

			// Selection signal A1
			case 1:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(10))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(10)));
				break;

			// Selection signal A2
			case 2:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(6))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(6)));
				break;

			// Selection signal A3
			case 3:
				((col_sel[i]) == 0) ? (PTA->PDOR &= ~GPIO_PDOR_PDO( GPIO_PIN(11))) : (PTA->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(11)));
				break;

			// Otherwise nothing to do...
			default:
				break;
		}
	}
}

/* Variable delay loop. */
void delay(int t1, int t2)
{
	int i, j;

	for(i=0; i<t1; i++) {
		for(j=0; j<t2; j++);
	}
}

/* Initialize the MCU - basic clock settings, turning the watchdog off. */
void MCUInit(void)  {
    MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
    WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
}

void PortsInit(void)
{
    /* Turn on all port clocks. */
    SIM->SCGC5 = SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTA_MASK;

    /* Set corresponding buttons. */
    PORTE->PCR[10] = PORT_PCR_MUX(0x01); // SW2
    PORTE->PCR[12] = PORT_PCR_MUX(0x01); // SW3
    PORTE->PCR[27] = PORT_PCR_MUX(0x01); // SW4
    PORTE->PCR[26] = PORT_PCR_MUX(0x01); // SW5
    PORTE->PCR[11] = PORT_PCR_MUX(0x01); // SW6

   /* Set corresponding PTA pins (rows selectors of 74HC154) for GPIO functionality. */
    PORTA->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );  // A0
    PORTA->PCR[10] = ( 0|PORT_PCR_MUX(0x01) ); // A1
    PORTA->PCR[6] = ( 0|PORT_PCR_MUX(0x01) );  // A2
    PORTA->PCR[11] = ( 0|PORT_PCR_MUX(0x01) ); // A3

    PORTA->PCR[26] = ( 0|PORT_PCR_MUX(0x01) );  // R0
    PORTA->PCR[24] = ( 0|PORT_PCR_MUX(0x01) );  // R1
    PORTA->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );   // R2
    PORTA->PCR[25] = ( 0|PORT_PCR_MUX(0x01) );  // R3
    PORTA->PCR[28] = ( 0|PORT_PCR_MUX(0x01) );  // R4
    PORTA->PCR[7] = ( 0|PORT_PCR_MUX(0x01) );   // R5
    PORTA->PCR[27] = ( 0|PORT_PCR_MUX(0x01) );  // R6
    PORTA->PCR[29] = ( 0|PORT_PCR_MUX(0x01) );  // R7

    /* Change corresponding PTB port pins as outputs. */
    PTB->PDDR = GPIO_PDDR_PDD(0x3C);     // LED ports as outputs

    /* Set corresponding PTE pins (output enable of 74HC154) for GPIO functionality. */
    PORTE->PCR[28] = ( 0|PORT_PCR_MUX(0x01) ); // #EN

    /* Change corresponding PTA port pins as outputs. */
    PTA->PDDR = GPIO_PDDR_PDD(0x3F000FC0);

    /* Change corresponding PTE port pins as outputs. */
    PTE->PDDR = GPIO_PDDR_PDD( GPIO_PIN(28) );
}

void PORTE_IRQHandler(void) {
	delay(tdelaylong, 2);

	if(PORTE->ISFR & BTN_SW5 && ((GPIOE_PDIR & BTN_SW5) == 0)) { // Up
		if(((snake_bitmap[0][1] == snake_bitmap[1][1]) &&
			(snake_bitmap[0][0] >  snake_bitmap[1][0])) ||
			(snake_bitmap[0][0] == 0 && snake_bitmap[1][0] == 15)) {
				current_btn = DOWN;
		} else {
				current_btn = UP;
		}
	} else if (PORTE->ISFR & BTN_SW2 && ((GPIOE_PDIR & BTN_SW2) == 0)){ // Right
		if(((snake_bitmap[0][0] == snake_bitmap[1][0]) &&
			(snake_bitmap[0][1] >  snake_bitmap[1][1])) ||
			(snake_bitmap[0][1] == 0 && snake_bitmap[1][1] == 7)) {
				current_btn = LEFT;
		} else {
				current_btn = RIGHT;
		}
	} else if (PORTE->ISFR & BTN_SW3 && ((GPIOE_PDIR & BTN_SW3) == 0)){ // Down
		if(((snake_bitmap[0][1] == snake_bitmap[1][1]) &&
			(snake_bitmap[0][0] <  snake_bitmap[1][0])) ||
			(snake_bitmap[0][0] == 15 && snake_bitmap[1][0] == 0)) {
				current_btn = UP;
		} else {
				current_btn = DOWN;
		}
	} else if (PORTE->ISFR & BTN_SW4 && ((GPIOE_PDIR & BTN_SW4) == 0)){ // Left
		if(((snake_bitmap[0][0] == snake_bitmap[1][0]) &&
			(snake_bitmap[0][1] <  snake_bitmap[1][1])) ||
			(snake_bitmap[0][1] == 7 && snake_bitmap[1][1] == 0)) {
				current_btn = RIGHT;
		} else {
				current_btn = LEFT;
		}
	} else if (PORTE->ISFR & BTN_SW6 && ((GPIOE_PDIR & BTN_SW6) == 0)){
		current_btn = HALT;
	}

	PORTE->ISFR = BTN_SW5 | BTN_SW2 | BTN_SW3 | BTN_SW4 | BTN_SW6 ;
}

void PORTEInit(void) {

	int btns[5] = {10, 12, 27, 26, 11};

	for (int i = 0; i < 5; i++) {
		PORTE->PCR[btns[i]] = ( PORT_PCR_ISF(0x01) |
								PORT_PCR_IRQC(0x0A) |
								PORT_PCR_MUX(0x01)  |
								PORT_PCR_PE(0x01)   |
								PORT_PCR_PS(0x01));
	}
}

void LPTMR0_IRQHandler(void)
{
    // Set new compare value set by up/down buttons
    LPTMR0_CMR = compare;                // !! the CMR reg. may only be changed while TCF == 1
    LPTMR0_CSR |=  LPTMR_CSR_TCF_MASK;   // writing 1 to TCF tclear the flag
}
void LPTMR0Init(int count)
{
    SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK; // Enable clock to LPTMR
    LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;   // Turn OFF LPTMR to perform setup
    LPTMR0_PSR = ( LPTMR_PSR_PRESCALE(0) // 0000 is div 2
                 | LPTMR_PSR_PBYP_MASK   // LPO feeds directly to LPT
                 | LPTMR_PSR_PCS(1)) ;   // use the choice of clock
    LPTMR0_CMR = count;                  // Set compare value
    LPTMR0_CSR =(  LPTMR_CSR_TCF_MASK    // Clear any pending interrupt (now)
                 | LPTMR_CSR_TIE_MASK    // LPT interrupt enabled
                );
    NVIC_EnableIRQ(LPTMR0_IRQn);         // enable interrupts from LPTMR0
    PORTEInit();
    NVIC_ClearPendingIRQ(PORTE_IRQn);
    NVIC_EnableIRQ(PORTE_IRQn);

    LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;    // Turn ON LPTMR0 and start counting
}

void swap_coordinates() {
	/* The function responsible for the moving of the snake.  */
	for (int i = SNAKE_LENGTH - 1; i > 0; i--) {
		snake_bitmap[i][0] = snake_bitmap[i - 1][0];
		snake_bitmap[i][1] = snake_bitmap[i - 1][1];
	}
}


void init_snake(void) {
	current_btn = UP;
	/* The same logic as in the show_snake() function. */
	for (int i = 0; i < SNAKE_LENGTH; i++) {
		snake_bitmap[i][0] = 11+i;
		snake_bitmap[i][1] = 0;
		PTA->PDOR &= ~GPIO_PDOR_PDO(0x3F000280); // nullify
		PTA->PDOR |= GPIO_PDOR_PDO(ROWS[snake_bitmap[0][1]]);
		column_select(snake_bitmap[i][0]);
		delay(tdelaylong, 1);
	}
}

void show_snake(void) {
	/* Visualization of the snake. */
	for(int i = 0; i < 30; i++) {
		for (int j = 0; j < SNAKE_LENGTH; j++) {
			PTA->PDOR &= ~GPIO_PDOR_PDO(0x3F000280); // nullify
			PTA->PDOR |= GPIO_PDOR_PDO( ROWS[snake_bitmap[j][1]] ); // turn on specific row
			column_select(snake_bitmap[j][0]); // turn on column
			delay(tdelaylong, 0); // do delay in order to create an effect of showing all snake in one shot
		}
	}
}

bool hit(void) {
	/* Snake's head touches its tail. Game is over. */
	if(snake_bitmap[0][0] == snake_bitmap[4][0] && snake_bitmap[0][1] == snake_bitmap[4][1]){
	    return true;
	} else {
		/* Otherwise the game keeps going. */
		return false;
	}
}

int main(void)
{
	/* Initialization of all needed parts. */
	MCUInit();
    PortsInit();
    LPTMR0Init(compare);

    init_snake();

    /* Game's logic */
    while (1) {

    	switch(current_btn) {

    		case UP:
    			swap_coordinates();
    			/* Check whether the snake's head is on the board edge. */
    			if (snake_bitmap[0][0] == 0) {
    				/* Place the head to opposite part of the board. */
    				snake_bitmap[0][0] = 15;
    			} else {
    				/* Otherwise the normal step. */
    				snake_bitmap[0][0] = snake_bitmap[0][0] - 1;
    			}
    			/* Visualize the snake. */
    			show_snake();
    			break;

    		case RIGHT:
    			swap_coordinates();
    			if(snake_bitmap[0][1] == 0) {
    				snake_bitmap[0][1] = 7;
    			} else {
    				snake_bitmap[0][1] = snake_bitmap[0][1] - 1;
    			}
    			show_snake();
    			break;

    		case DOWN:
    			swap_coordinates();
    			if (snake_bitmap[0][0] == 15) {
    				snake_bitmap[0][0] = 0;
    			} else {
    				snake_bitmap[0][0] = snake_bitmap[0][0] + 1;
    			}
    			show_snake();
    			break;

    		case LEFT:
    			swap_coordinates();
    			if(snake_bitmap[0][1] == 7) {
    				snake_bitmap[0][1] = 0;
    			} else {
    				snake_bitmap[0][1] = snake_bitmap[0][1] + 1;
    			}
    			show_snake();
    			break;

    		case HALT:
    			init_snake();
    			break;
    	}

    	/* The snake's head touches itself body. Game is over. */
    	if(hit()) {
    		init_snake();
    	}

    	PTE->PDDR &= ~GPIO_PDDR_PDD( GPIO_PIN(28) );
    	delay(tdelaylong, tdelaymedium);
    	PTE->PDOR |= GPIO_PDOR_PDO( GPIO_PIN(28));
    }

    return 0;
}
