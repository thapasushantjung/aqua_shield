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

// PWM variables for motor speed control
unsigned char pwm_duty_cycle = 25; // 25% duty cycle for very gentle operation (reduced from 45%)
unsigned char pwm_counter = 0;

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
void motor_stop(void);
void motor_pwm_clockwise(unsigned int duration_ms);
void motor_pwm_anticlockwise(unsigned int duration_ms);
void pwm_delay(void);
void set_motor_speed(unsigned char speed_percent);
void motor_gradual_start_stop(unsigned char direction, unsigned int duration_ms);
	
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

// PWM delay function for motor speed control
void pwm_delay(void)
{
    unsigned char i;
    for(i = 0; i < 35; i++); // Increased delay for slower PWM timing (~35μs per cycle, was 20μs)
}

// PWM motor control functions
// PWM Implementation:
// - Creates variable speed control by switching motor ON/OFF rapidly
// - Duty cycle determines average power delivered to motor
// - Lower duty cycle = slower speed, gentler operation
// - Prevents sudden jerks that could damage clothes
void motor_pwm_clockwise(unsigned int duration_ms)
{
    unsigned int cycle_count;
    unsigned int total_cycles = duration_ms / 2; // Each PWM cycle takes ~2ms
    unsigned char on_time;
    unsigned char off_time;
    
    for(cycle_count = 0; cycle_count < total_cycles; cycle_count++)
    {
        // PWM ON period (duty cycle)
        clock = 1;
        anticlock = 0;
					
        // ON time based on duty cycle
        for(on_time = 0; on_time < pwm_duty_cycle; on_time++)
        {
            pwm_delay();
        }
        
        // PWM OFF period
        clock = 0;
        anticlock = 0;
        
        // OFF time (100 - duty_cycle)
        for(off_time = 0; off_time < (100 - pwm_duty_cycle); off_time++)
        {
            pwm_delay();
        }
    }
    
    // Ensure motor is stopped after PWM
    motor_stop();
}

void motor_pwm_anticlockwise(unsigned int duration_ms)
{
    unsigned int cycle_count;
    unsigned int total_cycles = duration_ms / 2; // Each PWM cycle takes ~2ms
    unsigned char on_time;
    unsigned char off_time;
    
    for(cycle_count = 0; cycle_count < total_cycles; cycle_count++)
    {
        // PWM ON period (duty cycle)
        clock = 0;
        anticlock = 1;
        
        // ON time based on duty cycle
        for(on_time = 0; on_time < pwm_duty_cycle; on_time++)
        {
            pwm_delay();
        }
        
        // PWM OFF period
        clock = 0;
        anticlock = 0;
        
        // OFF time (100 - duty_cycle)
        for(off_time = 0; off_time < (100 - pwm_duty_cycle); off_time++)
        {
            pwm_delay();
        }
    }
    
    // Ensure motor is stopped after PWM
    motor_stop();
}

// Function to set motor speed (0-100%)
void set_motor_speed(unsigned char speed_percent)
{
    if(speed_percent > 100) speed_percent = 100;
    if(speed_percent < 5) speed_percent = 5;  // Minimum 5% for ultra-gentle operation (was 10%)
    pwm_duty_cycle = speed_percent;
}

// Gradual start/stop function for ultra-smooth operation
// direction: 1 = clockwise, 0 = anticlockwise
void motor_gradual_start_stop(unsigned char direction, unsigned int duration_ms)
{
    unsigned char original_speed = pwm_duty_cycle;
    unsigned char ramp_steps = 10;
    unsigned char i;
    unsigned int ramp_duration = duration_ms / 10; // 10% of total time for ramp up/down
    
    // Gradual start (ramp up)
    for(i = 5; i <= original_speed; i += (original_speed / ramp_steps))
    {
        set_motor_speed(i);
        if(direction == 1)
            motor_pwm_clockwise(ramp_duration);
        else
            motor_pwm_anticlockwise(ramp_duration);
    }
    
    // Run at target speed for main duration
    set_motor_speed(original_speed);
    if(direction == 1)
        motor_pwm_clockwise(duration_ms - (2 * ramp_duration * ramp_steps));
    else
        motor_pwm_anticlockwise(duration_ms - (2 * ramp_duration * ramp_steps));
    
    // Gradual stop (ramp down)
    for(i = original_speed; i >= 5; i -= (original_speed / ramp_steps))
    {
        set_motor_speed(i);
        if(direction == 1)
            motor_pwm_clockwise(ramp_duration);
        else
            motor_pwm_anticlockwise(ramp_duration);
        if(i <= 5) break; // Prevent underflow
    }
    
    motor_stop();
    set_motor_speed(original_speed); // Restore original speed setting
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

                // Retrieve clothes (rotate clockwise with PWM control)
                set_motor_speed(25); // 25% speed for very gentle retrieval (reduced from 50%)
                
                LCD_init();
                LCD_cmd(0x80);
                LCD_string_write("Retrieving...");
                LCD_cmd(0xC0);
                LCD_string_write("Speed: 25%");
                delay(30);
                LCD_cmd(0x01);
                LCD_off();
                
                motor_pwm_clockwise(1500); // Run for 1.5 seconds with PWM (reduced from 3 seconds)
                
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
            if (R1 == 1 || R2 == 1 || R3 == 1) {
                // No rain detected
                rain_condition = 1; // It's not raining now

                // Release clothes (rotate anti-clockwise with PWM control)
                set_motor_speed(20); // 20% speed for very gentle release (reduced from 40%)
                
                LCD_init();
                LCD_cmd(0x80);
                LCD_string_write("Releasing...");
                LCD_cmd(0xC0);
                LCD_string_write("Speed: 20%");
                delay(30);
                LCD_cmd(0x01);
                LCD_off();
                
                motor_pwm_anticlockwise(1200); // Run for 1.2 seconds with PWM (reduced from 2.5 seconds)

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
}

void sim_init()
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
