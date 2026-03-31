#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "LiquidCrystal.h"

#define _XTAL_FREQ 1000000

void __interrupt(high_priority) stop(void);
void __interrupt(low_priority) ADC_and_Timer0(void);

volatile uint16_t target_delay = 0;
volatile uint16_t num_cycles = 0;


volatile uint16_t rate_of_change = 0;

volatile uint16_t target_motor_speed = 0;
volatile uint16_t current_motor_speed = 0;

_Bool pressed = false;


int main() {
    //          ***input and output***
    //INPUTS
    TRISBbits.RB0 = 1; //button input
    TRISAbits.RA0 = 1; //RA0 is an input -- potentiometer 1
    TRISAbits.RA1 = 1; //RA1 is an input -- potentiometer 2
    
    //OUTPUTS
    TRISBbits.RB1 = 0; //red LED
    TRISD = 0x00; //data pins for LCD
    TRISE = 0x00; // LCD signal pin RS = RE0, RW = RE1, E = RE2
    
    //                  ***ADC***
    ADCON1 = 0x0D; //setting AN1:AN12 as digital and AN0:AN1 as analog
    
    ADCON2bits.ADCS = 0; //A/D conversion clock
    ADCON2bits.ACQT = 1; //A/D acquisition time
    ADCON2bits.ADFM = 0; //left justified
    
    ADCON0bits.CHS = 0; //select A/D channel (CHS) 
    
    ADCON0bits.ADON = 1; //turn on A/D module
    
    //                   ***LCD***
    pin_setup(&PORTD, &PORTE);
    begin(16, 2, LCD_5x8DOTS);
    
    //                  ***timer0***
    T0CONbits.PSA = 0; // Prescaler is assigned
    T0CONbits.T0PS = 0x05; // 1:64 prescale value
    T0CONbits.T0CS = 0; // clock source is internal instruction cycle
    T0CONbits.T08BIT = 0; // operate in 16 bit mode now
    T0CONbits.TMR0ON = 1; // Turn on timer
    
    TMR0 = 65497; // For 10ms second delay (with 1:64 prescaler)
    
    //                    ***CCP2***
    T2CONbits.T2CKPS = 0b00; // Prescaler 1:1
    T2CONbits.TMR2ON = 1;

    TRISCbits.RC1 = 0;
    CCP2CONbits.CCP2M = 0b1100;
    
    PR2 =  249;
    
    //                 ***interrupt***
    //STOP BUTTON INTERRUPT (INT0)
    INTCONbits.INT0E = 1; // Enable INT0
    INTCONbits.INT0IF = 0; // reset INT0 flag
    INTCON3bits.INT1IP = 1; // Set INT1 to HIGH priority 
    INTCON2bits.INTEDG0 = 0; // falling edge
    
    //TIMER0 INTERRUPT
    INTCONbits.TMR0IE = 1; //enable timer0
    INTCONbits.TMR0IF = 0; //reset timer flag
    INTCON2bits.TMR0IP = 0; //low priority
   
    //ADC INTERRUPT
    PIR1bits.ADIF = 0; //clear ADIF bit
    PIE1bits.ADIE = 1; //set ADIE bit
    IPR1bits.ADIP = 0; //select interrupt priority ADIP bit--LOW PRIORITY ADC
    
    //ENABLE INTERRUPTS
    RCONbits.IPEN = 1; // turn on priorities
    INTCONbits.PEIE = 1; // enable all low-priority peripheral interrupts
    INTCONbits.GIE = 1;// enable all interrupts; IPEN = 1 Enables all high-priority interrupts 
    INTCONbits.GIEH = 1; // enable all high priority
    INTCONbits.GIEL = 1; // enable all low priority
    
    PORTBbits.RB1 = 0; // start with LED off
    clear();
    
    while(1)
    {
        //start conversion
        ADCON0bits.GO = 1;
        
        home();
        
        print("Max Speed:");
        print_int(target_motor_speed);

        print("   ");
        
        setCursor(0,1);
        print("Delay: ");
        print_int(10*target_delay);
        print(" ms");

        print("                        ");
        
    }
    
    return 0;
}

void __interrupt(high_priority) stop(void) //INT0
{
    if(INTCONbits.INT0IE && INTCONbits.INT0IF)//good to do if/else
    {
       INTCONbits.INT0IF = 0;
       pressed = !pressed;
        
        if(pressed)
        {
                clear();
                print("   STOP   ");
            
                PORTBbits.RB1 = 1; //turn on red LED
            
                //motor speed = 0
                CCPR2L = 0b0000000;
                CCP2CONbits.DC2B = 0b00;
        }
        else
            PORTBbits.RB1 = 0; //turn off red LED  
    }
}


void __interrupt(low_priority) ADC_and_Timer0(void)
{
    if (PIR1bits.ADIF && PIE1bits.ADIE && !pressed) //ADC potentiometers
    {
            PIR1bits.ADIF = 0;
                
            if(ADCON0bits.CHS == 0) // AN0 --- motor speed
            {   
                target_motor_speed = ((ADRESH << 2) | (ADRESL >> 6));
                ADCON0bits.CHS = 1; //change the channel
            }
            else if (ADCON0bits.CHS == 1) //AN1 --- delay
            {  
                target_delay = ((ADRESH << 2) | (ADRESL >> 6));
                ADCON0bits.CHS = 0;
            }
            

          if(target_delay > 0) //determine rate of change
            {
              //target delay is the # of cycles
              
                rate_of_change = target_motor_speed  / target_delay ;
                
                if(rate_of_change == 0)
                    rate_of_change = 1;
            }
          else //delay = 0
            {
                current_motor_speed = target_motor_speed;
                CCPR2L = current_motor_speed >> 2;
                CCP2CONbits.DC2B = current_motor_speed & 0x03;
            }
          
       
     }
    
    else if(INTCONbits.TMR0IE && INTCONbits.TMR0IF && !pressed) //timer0
    {
        INTCONbits.TMR0IF = 0; //reset timer flag

        TMR0 = 65497; //reset timer
        

       if (target_motor_speed > 0 && target_delay > 0)
        {
            if (num_cycles < target_delay) //continue gradual increase
            {
                num_cycles++;
                current_motor_speed = rate_of_change * num_cycles;
                CCPR2L = current_motor_speed >> 2;
                CCP2CONbits.DC2B = current_motor_speed & 0x03;
                if (current_motor_speed > target_motor_speed) //don't go over, if max speed reached b4 delay
                    current_motor_speed = target_motor_speed;

            }
            else //reset and restart
                num_cycles = 0;
        }
        else if (target_motor_speed == 0) // target speed = 0 
        {
            num_cycles = 0;
            current_motor_speed = 0;
            CCPR2L = current_motor_speed >> 2;
            CCP2CONbits.DC2B = current_motor_speed & 0x03;
        }

    }   
}        