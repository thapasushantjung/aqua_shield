#include <reg51.h>

#define NUMBER "+9779815007805"        //Here insert your number where you want to send message
#define LCD_data P0

// Rain detection controlling bits
sbit R1 = P3^2;
sbit R2 = P3^3;
sbit R3 = P3^4;

//DC motor control bits

sbit clock = P2^3;
sbit anticlock = P2^4;

// LCD control bits
sbit RS = P2^0;
sbit RW = P2^1;
sbit EN = P2^2;

// Rain condition bits
sbit rain_condition = P3^7;

// Debug pin
sbit DebugPin = P3^5;

// GSM Initialization
void sim_init();
void tx(unsigned char send);
void tx_string(unsigned char *s);
void sms(unsigned char *num1, unsigned char *msg);
void delay(unsigned int ms);

// LCD Initialization
void LCD_cmd(unsigned char command);
void LCD_data_write(unsigned char dataa);
void LCD_init(void);
void LCD_off(void);
void LCD_string_write(unsigned char *string);

//Motor Initialization
void motor_clockwise(void);
void motor_anticlockwise(void);
	
// Idle Mode Initialization
void initTimer0(void);
void enterIdleMode(void);

// Timer variable
unsigned long timer_count_60sec = 0;

// Add a flag for timer completion
bit timer_complete = 0;
bit timer_running = 0;  // New flag to track timer status

// Timer Module
void timer0_isr(void) interrupt 1
{
    TF0 = 0;        // Clear timer flag
    
    // Reload timer values for 50ms interval at 11.0592MHz
    TH0 = 0x4B;
    TL0 = 0xFE;
		DebugPin = !DebugPin;

    
    timer_count_60sec++;
    if (timer_count_60sec >= 40) { // 50ms * 1200 = 60 seconds
        timer_complete = 1;  // Set flag when timer completes
        timer_running = 0;   // Update timer status
        TR0 = 0;             // Stop timer
        timer_count_60sec = 0;
    }
}


//Motor control functions
void motor_clockwise(void)
{
    clock = 1;
    anticlock = 0;
}

void motor_anticlockwise(void)
{
    clock = 0;
    anticlock = 1;
}

void motor_stop(void)
{
    clock = 0;
    anticlock = 0;
}

void initTimer0(void)
{
    // Initialize Timer0 for 50ms intervals
    TMOD = (TMOD & 0xF0) | 0x01;  // Preserve Timer1 settings, set Timer0 to mode 1
    TH0 = 0x4B;     // Initial value for 50ms at 11.0592MHz
    TL0 = 0xFE;
    ET0 = 1;        // Enable Timer0interrupt
    timer_count_60sec = 0;
    timer_complete = 0;     // Reset the timer completion flag
    timer_running = 1;      // Set timer running flag
    TR0 = 1;        // Start timer
}

void enterIdleMode(void)
{
    // Ensure interrupts are enabled before entering idle mode
    EA = 1;
    if (timer_running) {
        PCON |= 0x01;   // Enter idle mode only if timer is running
        // After this instruction, CPU operation is suspended until aninterrupt
        // When a timerinterrupt occurs, CPU wakes up and execution continues
        // from the next instruction after this function call
    }
}

void main()
{
    // Initialize global interrupts
    EA = 1;
    
    // Initialize motor pins
    clock = 0;
    anticlock = 0;
    
    // Initial LCD display
    LCD_init();
    LCD_cmd(0x80);  // First line
    LCD_string_write("System Ready");
    delay(50);
		LCD_cmd(0x01);
    LCD_off();
    
    while(1) {
        if (rain_condition == 1) {  // It's not raining
            
            // Check rain sensors - more lenient detection (any sensor triggers)
            if (R1 == 0 || R2 == 0 || R3 == 0) {
                // Rain is detected
                rain_condition = 0;
                
							  LCD_init();
                
                LCD_cmd(0x80);  // First line
                LCD_string_write("Rain Detected!");
                delay(50);
								LCD_cmd(0x01);  // Clear display
							  LCD_off();

                // Retrieve clothes (rotate clockwise)
                motor_clockwise();
                delay(100);  // Run motor for 5 seconds - increased for visibility
                motor_stop();

							  sim_init();
                sms(NUMBER, "Clothes Retrieved");
                
                // LCD Module
							  LCD_init();
                LCD_cmd(0xC0);  // Second line
                LCD_string_write("Clothes Retrieved");
                delay(50);
								LCD_cmd(0x01);  // Clear display
                LCD_off();
            } else {
                LCD_off();
            }
            
            // Start 60-second timer and enter idle mode
            initTimer0();
            
            while (!timer_complete) {
                enterIdleMode();  // System enters low-power mode here
                
                // ? EXECUTION PAUSES HERE UNTIL AN INTERRUPT OCCURS ?
                
                // When Timer0 generates aninterrupt (every 50ms):
                // 1. The CPU wakes up and executes timer0_isr()
                // 2. After the ISR completes, execution CONTINUES FROM HERE
                
                // Loop returns to enterIdleMode() and CPU sleeps again
            }
            
            // Display timer complete
						if (timer_complete) {
							LCD_init();
							LCD_cmd(0x01);  // Clear display
							LCD_cmd(0x80);
							LCD_string_write("Timer Complete!");
							delay(10);
							LCD_cmd(0x01);  // Clear display
							LCD_off();
						}
						
						
        } else {  // It's raining
            
            
            // Check rain sensors after timer completes
            if (R1 == 1 && R2 == 1 && R3 == 1) {
                // No rain detected
                rain_condition = 1; // It's not raining now

                // Release clothes (rotate anti-clockwise)
                motor_anticlockwise();
                delay(100);  // Run motor for some time
                motor_stop();

								sim_init();
                sms(NUMBER, "Clothes Released");
               

                // LCD Module
                LCD_init();
							  LCD_cmd(0x01);  // Clear display
                LCD_cmd(0xC0);
                LCD_string_write("Clothes Released");
							  delay(50);
							  LCD_cmd(0x01);  // Clear display
                LCD_off();
            }
            // Start 60-second timer and enter idle mode
            initTimer0();
            
            while (!timer_complete) {
                enterIdleMode();  // System will wake on timerinterrupt
              
            }
            
            // Display timer complete
						if (timer_complete) {
							LCD_init();
							LCD_cmd(0x01);  // Clear display
							LCD_cmd(0x80);
							LCD_string_write("Timer Complete!");
							delay(10);
							LCD_cmd(0x01);  // Clear display
							LCD_off();
						}
            
        }
    }
}

// GSM Module Function Declaration
void delay(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 1275; j++);
	
}void sim_init()
{
    SCON=0X50;    //0101 0000 
    TMOD=0X20;      //FOR AUTO RELOAD MODE OF TIMER 1   0010 0000
    TH1=0XFD; //FOR 9600 BAUD RATE OF THE MICROCONTROLLER
    TL1=0XFD; //load the value of high time
    TR1=1;  //START TIMER
}

void tx(unsigned char send)
{
    SBUF = send;
    while(TI==0);
    TI=0;   //reset the timer interrupt

}

void tx_string(unsigned char *s)
{
    while(*s)
        tx(*s++);
}

void sms(unsigned char *num1, unsigned char *msg)
{
    tx_string("AT");
    tx(0x0d); 
		//tx(0x0a);  // Try both CR and LF
    delay(100);

    tx_string("AT+CMGF=1");
    tx(0x0d); //tx(0x0a);
    delay(100);
	
    tx_string("AT+CMGS=");
		tx('"');
    tx_string(num1);
    tx('"');
    tx(0x0d); 
    delay(100); // Increase if needed

    // Ideally, wait here for '>' prompt from SIM800L

  tx_string(msg);
    tx(0x1A); // CTRL+Z to send
    delay(100); // Wait for SMS send to complete
}

// LCD Module Function Declaration

void LCD_off(void){
LCD_cmd(0x08);
}
void LCD_cmd(unsigned char command) {
    LCD_data = command;
    RS = 0;
    RW = 0;
    EN = 1;
    delay(10);
    EN = 0;
}

void LCD_data_write(unsigned char dataa) {
    LCD_data = dataa;
    RS = 1;
    RW = 0;
    EN = 1;
    EN = 0;
}

void LCD_init(void) {
    LCD_cmd(0x38); // 2-line, 5x7 font
	delay(10);
    LCD_cmd(0x0E); // Display ON, Cursor ON
	delay(10);
    LCD_cmd(0x01); // Clear display
}

void LCD_string_write(unsigned char *string) {
    while (*string != '\0') {
        LCD_data_write(*string++);
    }
}
