/* Keypad Interrupt + UART + Switch controlled RGB*/

#include <stdint.h>
#include "inc/tm4c123gh6pm.h"


// FUNCTION DECLARATIONS


void GPIO_Init(void);
void UART_Init(void);

void UART_SendChar(char c);
void UART_SendString(char *str);
char UART_ReadChar(void);
void UART_ShowMenu(void);

void delayMs(int n);

void displayDigit(int digit, int value);
void refreshDisplay(void);
void refreshStopwatchDisplay(void);

void setLED(int colour);


// Stopwatch
void Stopwatch_EnableDisable(void);
void Stopwatch_StartReset(void);
void Stopwatch_PauseResume(void);


// Keypad
int Keypad_Scan(void);
void Keypad_Service(void);


// GLOBAL VARIABLES

// LAB 4 RGB LED


int status = 1;       // 1 = RUNNING, 0 = PAUSED
int colour = 1;       // 0 = OFF, 1-7 = colours
int speed = 1;        // 1-8
int ledState = 1;     // 1 = ON, 0 = OFF

// SysTick


volatile uint32_t ms_ticks = 0;


// STOPWATCH VARIABLES


volatile uint32_t stopwatchMs = 0;

volatile int stopwatchEnabled = 0;
volatile int stopwatchRunning = 0;
volatile int stopwatchPaused = 0;


// KEYPAD VARIABLES


volatile int keypadEvent = 0;
volatile int keypadWaitingRelease = 0;
volatile uint32_t keypadEventTime = 0;


// 7-SEGMENT CODES


const unsigned char segCode[10] =
{
    0x3F,   // 0
    0x06,   // 1
    0x5B,   // 2
    0x4F,   // 3
    0x66,   // 4
    0x6D,   // 5
    0x7D,   // 6
    0x07,   // 7
    0x7F,   // 8
    0x6F    // 9
};


#define LETTER_S 0x6D
#define LETTER_r 0x50
#define LETTER_P 0x73

// SYSTICK INTERRUPT


void SysTick_Handler(void)
{
    // 1 ms tick
    ms_ticks++;

    // Stopwatch timing


    if(stopwatchEnabled &&
       stopwatchRunning &&
       !stopwatchPaused)
    {
        stopwatchMs++;

        // Maximum displayable time = 59.99 seconds
        if(stopwatchMs >= 60000)
        {
            stopwatchMs = 0;
        }
    }
}


// GPIO PORT C INTERRUPT

//
// PC4-PC7 = keypad columns
//
// ISR only records the keypad event.
// Actual keypad scanning is done in main loop.
//

void GPIOPortC_Handler(void)
{
    uint32_t interruptStatus;


    interruptStatus = GPIO_PORTC_MIS_R & 0xF0;


    if(interruptStatus)
    {
        // Clear interrupt
        GPIO_PORTC_ICR_R = interruptStatus;


        // Temporarily disable keypad interrupts
        GPIO_PORTC_IM_R &= ~0xF0;


        // Record keypad event
        keypadEvent = 1;

        keypadWaitingRelease = 0;

        keypadEventTime = ms_ticks;
    }
}



// GPIO INITIALIZATION

void GPIO_Init(void)
{
    // Enable clocks for:
    // Port A, B, C, E and F

    SYSCTL_RCGCGPIO_R |= 0x37;


    // Wait for GPIO clocks

    while((SYSCTL_PRGPIO_R & 0x37) != 0x37)
    {
    }


    delayMs(1);


    // PORT A
    // PA4-PA7 = 7-segment digit selection

    GPIO_PORTA_DIR_R |= 0xF0;

    GPIO_PORTA_DEN_R |= 0xF0;


    // PORT B
    // PB0-PB7 = 7-segment segments

    GPIO_PORTB_DIR_R |= 0xFF;

    GPIO_PORTB_DEN_R |= 0xFF;


    // PORT C
    // PC4-PC7 = keypad columns

    GPIO_PORTC_DIR_R &= ~0xF0;

    GPIO_PORTC_DEN_R |= 0xF0;

    // Internal pull-ups

    GPIO_PORTC_PUR_R |= 0xF0;


    // PORT E
    // PE0-PE3 = keypad rows

    GPIO_PORTE_DIR_R |= 0x0F;

    GPIO_PORTE_DEN_R |= 0x0F;

    // Open drain outputs

    GPIO_PORTE_ODR_R |= 0x0F;

    // Drive all rows LOW initially

    GPIO_PORTE_DATA_R &= ~0x0F;


    // PORT F
    //
    // PF1 = RED
    // PF2 = BLUE
    // PF3 = GREEN
    // PF4 = SW1
    // PF0 = SW2

    // Unlock PF0

    GPIO_PORTF_LOCK_R = 0x4C4F434B;


    // Allow changes to PF0

    GPIO_PORTF_CR_R |= 0x01;


    // PF1-PF3 = outputs

    GPIO_PORTF_DIR_R |= 0x0E;


    // PF0 and PF4 = inputs

    GPIO_PORTF_DIR_R &= ~0x11;


    // Digital enable

    GPIO_PORTF_DEN_R |= 0x1F;


    // Pull-up resistors

    GPIO_PORTF_PUR_R |= 0x11;


    // INITIAL VALUES


    GPIO_PORTA_DATA_R &= ~0xF0;

    GPIO_PORTB_DATA_R = 0x00;

    GPIO_PORTF_DATA_R &= ~0x0E;


    // KEYPAD INTERRUPT CONFIGURATION

    // PC4-PC7:
    // Edge sensitive
    // Single edge
    // Falling edge

    GPIO_PORTC_IS_R &= ~0xF0;

    GPIO_PORTC_IBE_R &= ~0xF0;

    GPIO_PORTC_IEV_R &= ~0xF0;


    // Clear old interrupts

    GPIO_PORTC_ICR_R = 0xF0;


    // Enable interrupts on PC4-PC7

    GPIO_PORTC_IM_R |= 0xF0;

    // NVIC


    // GPIO Port C = IRQ 18
    // EN0 bit 2

    NVIC_EN0_R |= (1 << 2);


    // SYSTICK

    // 16 MHz clock
    // 1 ms period
    // Reload = 16000 - 1

    NVIC_ST_CTRL_R = 0;

    NVIC_ST_RELOAD_R = 15999;

    NVIC_ST_CURRENT_R = 0;


    // ENABLE
    // INTEN
    // CLK_SRC = system clock

    NVIC_ST_CTRL_R = 0x07;


    // Enable global interrupts

    __asm("cpsie i");
}


// UART INITIALIZATION

void UART_Init(void)
{
    // Enable UART0 clock

    SYSCTL_RCGCUART_R |= 0x01;


    // Enable Port A clock

    SYSCTL_RCGCGPIO_R |= 0x01;


    delayMs(1);


    // PA0 = RX
    // PA1 = TX

    GPIO_PORTA_AFSEL_R |= 0x03;


    // UART0 function on PA0 and PA1

    GPIO_PORTA_PCTL_R =
        (GPIO_PORTA_PCTL_R & 0xFFFFFF00)
        | 0x00000011;


    // Digital enable

    GPIO_PORTA_DEN_R |= 0x03;


    // Disable UART before configuration

    UART0_CTL_R &= ~0x01;


    // 115200 BAUD
    // 16 MHz clock

    UART0_IBRD_R = 8;

    UART0_FBRD_R = 44;


    // 8-bit data
    // No parity
    // 1 stop bit
    // FIFO enabled

    UART0_LCRH_R = 0x70;


    // Enable UART
    // TX enable
    // RX enable

    UART0_CTL_R = 0x301;
}


// UART SEND CHARACTER


void UART_SendChar(char c)
{
    // Wait while transmit FIFO is full

    while(UART0_FR_R & 0x20)
    {
    }


    UART0_DR_R = c;
}


// UART SEND STRING


void UART_SendString(char *str)
{
    while(*str)
    {
        UART_SendChar(*str);

        str++;
    }
}

// UART RECEIVE CHARACTER


char UART_ReadChar(void)
{
    // Wait until RX FIFO has data

    while(UART0_FR_R & 0x10)
    {
    }


    return (char)(UART0_DR_R & 0xFF);
}


// DELAY

void delayMs(int n)
{
    int i;
    int j;


    for(i = 0; i < n; i++)
    {
        for(j = 0; j < 3180; j++)
        {
        }
    }
}


// ==================================================
// RGB LED
// ==================================================

void setLED(int colour)
{
    // Turn OFF RGB LED first

    GPIO_PORTF_DATA_R &= ~0x0E;


    if(colour == 1)
    {
        // RED

        GPIO_PORTF_DATA_R |= 0x02;
    }

    else if(colour == 2)
    {
        // GREEN

        GPIO_PORTF_DATA_R |= 0x08;
    }

    else if(colour == 3)
    {
        // BLUE

        GPIO_PORTF_DATA_R |= 0x04;
    }

    else if(colour == 4)
    {
        // YELLOW

        GPIO_PORTF_DATA_R |= 0x0A;
    }

    else if(colour == 5)
    {
        // CYAN

        GPIO_PORTF_DATA_R |= 0x0C;
    }

    else if(colour == 6)
    {
        // PURPLE

        GPIO_PORTF_DATA_R |= 0x06;
    }

    else if(colour == 7)
    {
        // WHITE

        GPIO_PORTF_DATA_R |= 0x0E;
    }
}


// ==================================================
// DISPLAY ONE DIGIT
// ==================================================

void displayDigit(int digit, int value)
{
    unsigned char pattern;


    // Turn OFF all digits
    GPIO_PORTA_DATA_R &= ~0xF0;


    // ------------------------------------------------
    // Select segment pattern
    // ------------------------------------------------

    if(value >= 0 && value <= 9)
    {
        pattern = segCode[value];
    }

    else if(value == 10)
    {
        pattern = LETTER_S;
    }

    else if(value == 11)
    {
        pattern = LETTER_r;
    }

    else if(value == 12)
    {
        pattern = LETTER_P;
    }

    else
    {
        pattern = 0x00;
    }


    // Send pattern to segments
    GPIO_PORTB_DATA_R = pattern;

    // Decimal point for stopwatch only
    // Digit 3 is the second physical digit from the left
    if(stopwatchEnabled && digit == 3)
    {
        GPIO_PORTB_DATA_R |= 0x80;
    }


    // ------------------------------------------------
    // Select required digit
    // ------------------------------------------------

    if(digit == 1)
    {
        GPIO_PORTA_DATA_R |= 0x10;
    }

    else if(digit == 2)
    {
        GPIO_PORTA_DATA_R |= 0x20;
    }

    else if(digit == 3)
    {
        GPIO_PORTA_DATA_R |= 0x40;
    }

    else if(digit == 4)
    {
        GPIO_PORTA_DATA_R |= 0x80;
    }


    // Keep digit ON briefly
    delayMs(1);


    // Turn digit OFF
    GPIO_PORTA_DATA_R &= ~0xF0;
}

// ==================================================
// LAB 4 DISPLAY
// ==================================================

void refreshDisplay(void)
{
    // Digit 1 = r when running
    //           P when paused

    if(status == 1)
    {
        displayDigit(1, 11);
    }

    else
    {
        displayDigit(1, 12);
    }


    // Digit 2 = colour

    displayDigit(2, colour);


    // Digit 3 = S

    displayDigit(3, 10);


    // Digit 4 = speed

    displayDigit(4, speed);
}


// ==================================================
// STOPWATCH DISPLAY
// ==================================================
//
// Display:
//
//     SS:CC
//
// SS = seconds
// CC = centiseconds
//
// Example:
//
//     12.34
//
// = 12 seconds and 34 hundredths of a second
//

void refreshStopwatchDisplay(void)
{
    uint32_t time;
    int seconds;
    int centiseconds;

    time = stopwatchMs;

    seconds = (time / 1000) % 60;
    centiseconds = (time % 1000) / 10;

    displayDigit(4, seconds / 10);
    displayDigit(3, seconds % 10);
    displayDigit(2, centiseconds / 10);
    displayDigit(1, centiseconds % 10);
}

// ==================================================
// STOPWATCH ENABLE / DISABLE
// ==================================================

void Stopwatch_EnableDisable(void)
{
    if(stopwatchEnabled == 0)
    {
        // Enable

        stopwatchEnabled = 1;

        stopwatchRunning = 0;

        stopwatchPaused = 0;

        stopwatchMs = 0;


        UART_SendString("Stopwatch Enabled\r\n");
    }

    else
    {
        // Disable

        stopwatchEnabled = 0;

        stopwatchRunning = 0;

        stopwatchPaused = 0;

        stopwatchMs = 0;


        UART_SendString("Stopwatch Disabled\r\n");
    }
}


// ==================================================
// STOPWATCH START / RESET
// ==================================================

void Stopwatch_StartReset(void)
{
    // Stopwatch must be enabled

    if(stopwatchEnabled == 0)
    {
        UART_SendString("Stopwatch is disabled\r\n");

        return;
    }


    // =================================================
    // NOT RUNNING
    //
    // START FROM ZERO
    // =================================================

    if(stopwatchRunning == 0)
    {
        stopwatchMs = 0;

        stopwatchRunning = 1;

        stopwatchPaused = 0;


        UART_SendString("Stopwatch Started\r\n");
    }


    // =================================================
    // RUNNING
    //
    // RESET AND STOP
    // =================================================

    else
    {
        stopwatchMs = 0;

        stopwatchRunning = 0;

        stopwatchPaused = 0;


        UART_SendString("Stopwatch Reset\r\n");
    }
}


// ==================================================
// STOPWATCH PAUSE / RESUME
// ==================================================

void Stopwatch_PauseResume(void)
{
    // Stopwatch disabled

    if(stopwatchEnabled == 0)
    {
        UART_SendString("Stopwatch is disabled\r\n");

        return;
    }


    // Stopwatch hasn't started

    if(stopwatchRunning == 0)
    {
        UART_SendString("Start stopwatch first\r\n");

        return;
    }


    // =================================================
    // RUNNING -> PAUSED
    // =================================================

    if(stopwatchPaused == 0)
    {
        stopwatchPaused = 1;

        UART_SendString("Stopwatch Paused\r\n");
    }


    // =================================================
    // PAUSED -> RUNNING
    // =================================================

    else
    {
        stopwatchPaused = 0;

        UART_SendString("Stopwatch Resumed\r\n");
    }
}


// ==================================================
// KEYPAD SCAN
// ==================================================
//
// PE0 = row 1
// PE1 = row 2
// PE2 = row 3
// PE3 = row 4
//
// PC4 = column 1
// PC5 = column 2
// PC6 = column 3
// PC7 = column 4
//
// Returns:
//
// 1-16 = key number
// 0    = no valid key
//

int Keypad_Scan(void)
{
    int row;

    uint32_t columns;


    for(row = 0; row < 4; row++)
    {
        // Release all rows

        GPIO_PORTE_DATA_R |= 0x0F;


        // Drive current row LOW

        GPIO_PORTE_DATA_R &= ~(1 << row);


        delayMs(1);


        // Read columns

        columns = GPIO_PORTC_DATA_R & 0xF0;


        // =================================================
        // COLUMN 1
        // =================================================

        if(columns == 0xE0)
        {
            GPIO_PORTE_DATA_R &= ~0x0F;

            return (row * 4) + 1;
        }


        // =================================================
        // COLUMN 2
        // =================================================

        if(columns == 0xD0)
        {
            GPIO_PORTE_DATA_R &= ~0x0F;

            return (row * 4) + 2;
        }


        // =================================================
        // COLUMN 3
        // =================================================

        if(columns == 0xB0)
        {
            GPIO_PORTE_DATA_R &= ~0x0F;

            return (row * 4) + 3;
        }


        // =================================================
        // COLUMN 4
        // =================================================

        if(columns == 0x70)
        {
            GPIO_PORTE_DATA_R &= ~0x0F;

            return (row * 4) + 4;
        }
    }


    // Drive all rows LOW

    GPIO_PORTE_DATA_R &= ~0x0F;


    return 0;
}


// ==================================================
// KEYPAD SERVICE
// ==================================================

void Keypad_Service(void)
{
    int key;


    if(keypadEvent == 0)
    {
        return;
    }


    // =================================================
    // DEBOUNCE
    // =================================================

    if(keypadWaitingRelease == 0)
    {
        if((ms_ticks - keypadEventTime) >= 20)
        {
            key = Keypad_Scan();


            // -----------------------------------------
            // KEY 1
            // Enable / Disable
            // -----------------------------------------

            if(key == 1)
            {
                Stopwatch_EnableDisable();
            }


            // -----------------------------------------
            // KEY 2
            // Start / Reset
            // -----------------------------------------

            else if(key == 2)
            {
                Stopwatch_StartReset();
            }


            // -----------------------------------------
            // KEY 3
            // Pause / Resume
            // -----------------------------------------

            else if(key == 3)
            {
                Stopwatch_PauseResume();
            }


            // Wait for release

            keypadWaitingRelease = 1;
        }
    }


    // =================================================
    // WAIT FOR KEY RELEASE
    // =================================================

    else
    {
        // Drive all rows LOW

        GPIO_PORTE_DATA_R &= ~0x0F;


        // All columns HIGH = released

        if((GPIO_PORTC_DATA_R & 0xF0) == 0xF0)
        {
            keypadEvent = 0;

            keypadWaitingRelease = 0;


            // Clear pending interrupt

            GPIO_PORTC_ICR_R = 0xF0;


            // Re-enable keypad interrupts

            GPIO_PORTC_IM_R |= 0xF0;
        }
    }
}

void UART_ShowMenu(void)
{
    UART_SendString("\r\n");
    UART_SendString("========================================\r\n");
    UART_SendString("        EMBEDDED SYSTEM CONTROLLER      \r\n");
    UART_SendString("========================================\r\n");

    UART_SendString("\r\n");
    UART_SendString("--------------- RGB LED ----------------\r\n");

    UART_SendString("SW1 / 'rate'  : Change LED speed\r\n");
    UART_SendString("SW2 / 'color' : Change LED colour\r\n");
    UART_SendString("SW1 + SW2 / 'pause/run'   : Pause / Resume LED\r\n");

    UART_SendString("\r\nCurrent LED Status : ");

    if(status == 1)
        UART_SendString("RUNNING\r\n");
    else
        UART_SendString("PAUSED\r\n");

    UART_SendString("Current Speed      : ");

    if(speed == 1) UART_SendString("1\r\n");
    else if(speed == 2) UART_SendString("2\r\n");
    else if(speed == 3) UART_SendString("3\r\n");
    else if(speed == 4) UART_SendString("4\r\n");
    else if(speed == 5) UART_SendString("5\r\n");
    else if(speed == 6) UART_SendString("6\r\n");
    else if(speed == 7) UART_SendString("7\r\n");
    else if(speed == 8) UART_SendString("8\r\n");

    UART_SendString("Current Colour     : ");

    if(colour == 0)
        UART_SendString("OFF\r\n");
    else if(colour == 1)
        UART_SendString("RED\r\n");
    else if(colour == 2)
        UART_SendString("GREEN\r\n");
    else if(colour == 3)
        UART_SendString("BLUE\r\n");
    else if(colour == 4)
        UART_SendString("YELLOW\r\n");
    else if(colour == 5)
        UART_SendString("CYAN\r\n");
    else if(colour == 6)
        UART_SendString("PURPLE\r\n");
    else if(colour == 7)
        UART_SendString("WHITE\r\n");


    UART_SendString("\r\n------------- STOPWATCH ---------------\r\n");

    UART_SendString("1 / Keypad 1 : Enable / Disable\r\n");
    UART_SendString("2 / Keypad 2 : Start / Reset\r\n");
    UART_SendString("3 / Keypad 3 : Pause / Resume\r\n");

    UART_SendString("\r\nStopwatch Status   : ");

    if(stopwatchEnabled == 0)
    {
        UART_SendString("DISABLED\r\n");
    }
    else if(stopwatchRunning == 0)
    {
        UART_SendString("ENABLED - STOPPED\r\n");
    }
    else if(stopwatchPaused == 1)
    {
        UART_SendString("PAUSED\r\n");
    }
    else
    {
        UART_SendString("RUNNING\r\n");
    }

    UART_SendString("\r\n");
       UART_SendString("========================================\r\n");
       UART_SendString("For viewing this menu, type 'help'\r\n");
       UART_SendString("> ");

    UART_SendString("\r\n");
    UART_SendString("========================================\r\n");
    UART_SendString("Enter command and press ENTER\r\n");
    UART_SendString("> ");
}

// ==================================================
// MAIN
// ==================================================

int main(void)
{
    //int blinkCounter = 0;


    // =================================================
    // LAB 4 SWITCH VARIABLES
    // =================================================

    int sw1;

    int sw2;


    int lastSW1 = 1;

    int lastSW2 = 1;


    /*
     * switchPending:
     *
     * 0 = waiting for a new press
     * 1 = waiting to decide which switch was pressed
     * 2 = press processed, waiting for release
     */

    int switchPending = 0;


    // Approximately 50 ms debounce

    int switchTimer = 0;


    // =================================================
    // UART VARIABLES
    // =================================================

    char command[10];

    int index = 0;

    char receivedChar;


    // =================================================
    // LED TIMING
    // =================================================

    uint32_t lastBlinkTime = 0;


    // =================================================
    // INITIALIZE
    // =================================================

    GPIO_Init();

    UART_Init();

    UART_ShowMenu();


    // =================================================
    // UART STARTUP MESSAGE
    // =================================================

    /*UART_SendString("\r\nUART LED Controller + Stopwatch\r\n");

    UART_SendString("LED Commands:\r\n");

    UART_SendString("rate\r\n");

    UART_SendString("color\r\n");

    UART_SendString("pause\r\n");

    UART_SendString("run\r\n");


    UART_SendString("\r\nStopwatch Commands:\r\n");

    UART_SendString("1 = Enable / Disable\r\n");

    UART_SendString("2 = Start / Reset\r\n");

    UART_SendString("3 = Pause / Resume\r\n");


    UART_SendString("> ");*/


    // =================================================
    // MAIN LOOP
    // =================================================

    while(1)
    {

        // =================================================
        // READ ONBOARD SWITCHES
        //
        // EXACT LAB 4 LOGIC
        // =================================================

        // Active LOW
        //
        // 1 = NOT PRESSED
        // 0 = PRESSED

        sw1 = (GPIO_PORTF_DATA_R & 0x10) ? 1 : 0;

        sw2 = (GPIO_PORTF_DATA_R & 0x01) ? 1 : 0;


        // =================================================
        // START DETECTION OF NEW PRESS
        // =================================================

        if(switchPending == 0)
        {
            /*
             * Detect a new press of either switch.
             *
             * We don't immediately decide whether it is
             * SW1 or SW2.
             *
             * We wait approximately 50 ms so that if the
             * user presses both switches slightly apart,
             * both can be detected.
             */

            if((sw1 == 0 && lastSW1 == 1) ||
               (sw2 == 0 && lastSW2 == 1))
            {
                switchPending = 1;

                switchTimer = 13;
            }
        }


        // =================================================
        // WAIT BEFORE DECIDING WHICH SWITCH
        // =================================================

        if(switchPending == 1)
        {
            if(switchTimer > 0)
            {
                switchTimer--;
            }


            /*
             * After approximately:
             *
             * 13 × approximately 4 ms = approximately 52 ms
             *
             * check the switches again.
             */

            if(switchTimer == 0)
            {
                // Read switches again

                sw1 = (GPIO_PORTF_DATA_R & 0x10) ? 1 : 0;

                sw2 = (GPIO_PORTF_DATA_R & 0x01) ? 1 : 0;


                // =================================================
                // BOTH SWITCHES
                // =================================================

                if(sw1 == 0 && sw2 == 0)
                {
                    // Toggle RUN / PAUSE

                    status = !status;


                    if(status == 1)
                    {
                        UART_SendString("\r\nRunning\r\n");
                    }

                    else
                    {
                        UART_SendString("\r\nPaused\r\n");
                    }
                }


                // =================================================
                // SW1 ONLY
                // =================================================

                else if(sw1 == 0 && sw2 == 1)
                {
                    if(status == 1)
                    speed++;


                    if(speed > 8)
                    {
                        speed = 1;
                    }


                    UART_SendString("\r\nRate increased\r\n");
                }


                // =================================================
                // SW2 ONLY
                // =================================================

                else if(sw1 == 1 && sw2 == 0)
                {
                    if(status == 1)
                    colour++;


                    if(colour > 7)
                    {
                        colour = 0;
                    }


                    UART_SendString("\r\nColor changed\r\n");
                }


                // Press has been processed

                switchPending = 2;
            }
        }


        // =================================================
        // WAIT FOR RELEASE
        // =================================================

        if(switchPending == 2)
        {
            // Read switches

            sw1 = (GPIO_PORTF_DATA_R & 0x10) ? 1 : 0;

            sw2 = (GPIO_PORTF_DATA_R & 0x01) ? 1 : 0;


            // Both released

            if(sw1 == 1 && sw2 == 1)
            {
                switchPending = 0;
            }
        }


        // Save current states

        lastSW1 = sw1;

        lastSW2 = sw2;


        // =================================================
        // KEYPAD SERVICE
        // =================================================

        Keypad_Service();


        // =================================================
        // UART COMMAND HANDLING
        // =================================================

        if(!(UART0_FR_R & 0x10))
        {
            receivedChar = UART_ReadChar();


            // =================================================
            // ENTER
            // =================================================

            if(receivedChar == '\r' ||
               receivedChar == '\n')
            {
                // Finish string

                command[index] = '\0';


                UART_SendString("\r\n");


                // =================================================
                // STOPWATCH COMMAND 1
                // =================================================

                if(command[0] == '1' &&
                   command[1] == '\0')
                {
                    Stopwatch_EnableDisable();
                }


                // =================================================
                // STOPWATCH COMMAND 2
                // START / RESET
                // =================================================

                else if(command[0] == '2' &&
                        command[1] == '\0')
                {
                    Stopwatch_StartReset();
                }


                // =================================================
                // STOPWATCH COMMAND 3
                // PAUSE / RESUME
                // =================================================

                else if(command[0] == '3' &&
                        command[1] == '\0')
                {
                    Stopwatch_PauseResume();
                }

                else if(command[0] == 'h' &&
                        command[1] == 'e' &&
                        command[2] == 'l' &&
                        command[3] == 'p' &&
                        command[4] == '\0')
                {
                    UART_ShowMenu();
                }


                // =================================================
                // RATE
                // =================================================

                else if(command[0] == 'r' &&
                        command[1] == 'a' &&
                        command[2] == 't' &&
                        command[3] == 'e' &&
                        command[4] == '\0')
                {

                    if(status == 1)
                    speed++;


                    if(speed > 8)
                    {
                        speed = 1;
                    }


                    UART_SendString("Rate increased\r\n");
                }


                // =================================================
                // COLOR
                // =================================================

                else if(command[0] == 'c' &&
                        command[1] == 'o' &&
                        command[2] == 'l' &&
                        command[3] == 'o' &&
                        command[4] == 'r' &&
                        command[5] == '\0')
                {
                    if(status == 1)
                    colour++;


                    if(colour > 7)
                    {
                        colour = 0;
                    }


                    UART_SendString("Color changed\r\n");
                }


                // =================================================
                // PAUSE
                // =================================================

                else if(command[0] == 'p' &&
                        command[1] == 'a' &&
                        command[2] == 'u' &&
                        command[3] == 's' &&
                        command[4] == 'e' &&
                        command[5] == '\0')
                {
                    status = 0;

                    ledState = 1;


                    UART_SendString("Paused\r\n");
                }


                // =================================================
                // RUN
                // =================================================

                else if(command[0] == 'r' &&
                        command[1] == 'u' &&
                        command[2] == 'n' &&
                        command[3] == '\0')
                {
                    status = 1;


                    UART_SendString("Running\r\n");
                }


                // =================================================
                // INVALID COMMAND
                // =================================================

                else if(index != 0)
                {
                    UART_SendString("Invalid command\r\n");
                }


                // Reset command buffer

                index = 0;


                // New prompt

                UART_SendString("> ");
            }


            // =================================================
            // NORMAL CHARACTER
            // =================================================

            else if(index < 9)
            {
                command[index] = receivedChar;

                index++;


                // Echo character

                UART_SendChar(receivedChar);
            }
        }


        // =================================================
        // RGB LED CONTROL
        // =================================================

        if(status == 1)
        {
            /*
             * Use SysTick time for LED blink timing.
             *
             * This does NOT affect the stopwatch.
             */

            if((ms_ticks - lastBlinkTime) >=
               (uint32_t)(1000 / speed))
            {
                lastBlinkTime = ms_ticks;

                ledState = !ledState;
            }


            // Colour 0 = OFF

            if(colour == 0)
            {
                setLED(0);
            }

            else if(ledState == 1)
            {
                setLED(colour);
            }

            else
            {
                setLED(0);
            }
        }


        // =================================================
        // LED PAUSED
        // =================================================

        else
        {
            ledState = 1;

            setLED(colour);
        }


        // =================================================
        // 7-SEGMENT DISPLAY
        // =================================================

        if(stopwatchEnabled)
        {
            refreshStopwatchDisplay();
        }

        else
        {
            refreshDisplay();
        }
    }
}
