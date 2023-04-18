#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int j = 0;

volatile int i, n, sampleCount;
volatile int tcnt = 0;
volatile int lock_state = 1;
volatile unsigned char col_holding, row_holding;
volatile unsigned char pressed_key;


volatile unsigned char transmit_key;
char charMode;

void initI2C_master(){
     UCB1CTLW0 |= UCSWRST;          // SW RESET ON

     UCB1CTLW0 |= UCSSEL_3;         // SMCLK
     UCB1BRW = 10;                  // Prescalar to 10

     UCB1CTLW0 |= UCMODE_3;         // Put into I2C mode
     UCB1CTLW0 |= UCMST;            // Set as MASTER
     UCB1CTLW0 |= UCTR;             // Put into Tx mode

     UCB1CTLW1 |= UCASTP_2;         // Enable automatic stop bit
     UCB1TBCNT = 1;
     //UCB1TBCNT = sizeof(packet);    // Transfer byte count

     // Setup ports
     P6DIR |= (BIT6 | BIT5 | BIT4);     // Set P6.6-4 as OUTPUT
     P6OUT &= ~(BIT6 | BIT5 | BIT4);    // Clear P6.6-4

     P4SEL1 &= ~BIT7;            // P4.7 SCL
     P4SEL0 |= BIT7;


     P4SEL1 &= ~BIT6;            // P4.6 SDA
     P4SEL0 |= BIT6;

     PM5CTL0 &= ~LOCKLPM5;       // Turn on I/O
     UCB1CTLW0 &= ~UCSWRST;      // SW RESET OFF

}

void initTimerB0compare(){      // Setup TB0
    TB0CTL |= TBCLR;            // Clear TB0
    TB0CTL |= TBSSEL__ACLK;     // Select SMCLK
    TB0CTL |= MC__UP;           // UP mode
}

void columnInput(){
    P3DIR &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // Initialize pins as input
    P3REN |= (BIT0 | BIT1 | BIT2 | BIT3);   // Enable pull up/down resistor
    P3OUT &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // Configure resistors as pull down

    P3DIR |= (BIT4 | BIT5 | BIT6 | BIT7);   // Init pins as outputs
    P3OUT |= (BIT4 | BIT5 | BIT6 | BIT7);   // Set as outputs

    P3IES &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // L-H edge sensitivity
    P3IE |= (BIT0 | BIT1 | BIT2 | BIT3);    // Enable IRQs

    P3IFG &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // Clear the P3 interrupt flags
}

void rowInput(){
    P3DIR &= ~(BIT4 | BIT5 | BIT6 | BIT7);  // Initialize pins as input
    P3REN |= (BIT4 | BIT5 | BIT6 | BIT7);   // Enable pull up/down resistor
    P3OUT &= ~(BIT4 | BIT5 | BIT6 | BIT7);  // Configure resistors as pull down

    P3DIR |= (BIT0 | BIT1 | BIT2 | BIT3);   // Init pins as outputs
    P3OUT |= (BIT0 | BIT1 | BIT2 | BIT3);   // Set as outputs
}


void delay1000() {
    int i;
    for(i = 0; i <= 1000; i++){}
}


int main(void) {

    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog timer

    initI2C_master();                   // Intialize master for I2C transmission

    initTimerB0compare();               // Initialize Timer B0 for getting keypad input

    columnInput();                      // Configure keypad columns to start as inputs

    PM5CTL0 &= ~LOCKLPM5;               // Turn on Digital I/O

    UCB1I2CSA = 0x0058;                 // Set slave address
    UCB1IE |= UCTXIE0;                  // Enable I2C TX interrupt

    __enable_interrupt();

    int i,k;
    while(1){

        UCB1CTLW0 |= UCTXSTT;           // Generate START condition
        for(k=0; k<=20;k++){
            for(i=0;i<=10000;i++){}
        }

    }

    return 0;
}

void keyPressedAction(char pressed_key) {
    if(lock_state == 1){
        charMode = pressed_key;
    }

    columnInput();                              // Reset keypad columns to be inputs
    P3IFG &= ~(BIT0 | BIT1 | BIT2 | BIT3);      // Clear the P3 interrupt flags

}

#pragma vector=PORT3_VECTOR
__interrupt void ISR_PORT3(void){           // Enable timer for debouncing
    TB0CCTL0 |= CCIE;                       // Local IRQ enable for CCR0
    TB0CCR0 = 500;                          // Set CCR0 value (period) // old value = 2384
    TB0CCTL0 &= ~CCIFG;                     // Clear CCR0 flag to begin count
}

#pragma vector=TIMER0_B0_VECTOR
__interrupt void ISR_TB0_CCR0(void){

    TB0CCTL0 &= ~CCIE;      // Disable TimerB0
    UCB1IE &= ~UCTXIE0;     // Disable I2C B0 TX interrupt

    col_holding = P3IN;

    rowInput();

    row_holding = P3IN;
    pressed_key = col_holding + row_holding;

    keyPressedAction(pressed_key);

    UCB1IE |= UCTXIE0;      // Enable I2C B0 TX interrupt

}

#pragma vector=EUSCI_B1_VECTOR
__interrupt void EUSCI_B1_TX_ISR(void){                     // Fill TX buffer with packet values

    UCB1TXBUF = charMode;
}
