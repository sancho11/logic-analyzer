/*
 * Created: 11/12/2016 19.35.51
 * Author : Vincenzo / Sancho11
 * Ported to UNO keeping Mega syntax/conventions
 */

 #define baudrate 115200   // Serial baudrate (must match other receiver)
 #define samples 200       // Number of signal samples to capture
 #define timezerooffset 1  // Microseconds offset for time correction
 #define PULLUP true       // Enable internal pull-ups if true
 #define F_CPU 16000000UL  // CPU frequency (16 MHz for Arduino Uno)
 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <util/delay.h>
 #define prescaler 0x02    // Timer1 prescaler (divide by 8 → 2 MHz → 0.5 µs per tick)
 volatile uint16_t timer1_overflow_count;
 
 uint8_t initial1, initial2, state1, state2, old_state1, old_state2;
 uint8_t pinChanged1[samples];
 uint8_t pinChanged2[samples];
 uint32_t timer[samples];
 uint32_t timefix;
 uint16_t event = 0;
 uint8_t changeflag = 0;
 
 /*
 On UNO:
 - We sample two 8-bit ports:
   * state1 → PIND  (D0..D7)  [NOTE: D0/D1 are UART; we avoid enabling pullups there]
   * state2 → PINB  (D8..D13)
 - Serial protocol matches Mega's 2-byte variant:
   initial1,initial2 : samples+2
   then (B0,B0) @ (0 - timezerooffset)
   then pinChanged1,pinChanged2 : timestamp (µs)
   then trailing dummy event at last_time + 1
 */
 
 // ---------------- Board Initialization ----------------
 void init_board() {
   // Configure ports as input
   DDRD = 0x00;   // D0..D7 input
   DDRB = 0x00;   // D8..D13 input
 
   if (PULLUP){
     // Enable internal pull-ups so floating pins read HIGH
     // Avoid enabling pull-ups on D0/D1 to not disturb hardware UART
     for (uint8_t p = 2; p <= 13; p++){
       pinMode(p, INPUT_PULLUP);
     }
   } else {
     for (uint8_t p = 2; p <= 13; p++){
       pinMode(p, INPUT);
     }
   }
 
   pinMode(LED_BUILTIN, OUTPUT);
   digitalWrite(LED_BUILTIN, LOW);
 }
 
 // ---------------- Timer Initialization ----------------
 void init_timer() {
   // Reset Timer1 registers
   TCCR1A = 0;
   TCCR1B = 0;
   TIMSK1 = 0;
 
   // Normal mode operation
   TCCR1A |= (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0);
   TCCR1A |= (0 << WGM11) | (0 << WGM10);
   TCCR1B |= (0 << WGM13) | (0 << WGM12);
 
   // Set prescaler (0x02 → divide by 8 → 2 MHz)
   TCCR1B |= prescaler;
 
   sei();                  // Enable global interrupts
   TIMSK1 |= (1 << TOIE1); // Enable Timer1 overflow interrupt
 }
 
 // Overflow interrupt service routine
 ISR(TIMER1_OVF_vect) {
   timer1_overflow_count++;
 }
 
 // Reset timer counter and overflow counter
 void reset_timer1 () {
   TCNT1 = 0;
   timer1_overflow_count = 0;
 }
 
 // Custom micros() implementation (0.5 µs ticks → convert to µs)
 uint32_t myMicros () {
   cli(); // Disable interrupts for atomic read
 
   // If overflow occurred but not yet serviced
   if (TIFR1 & (1 << TOV1)) {
     TIFR1 = (1 << TOV1);  // Correct way to clear overflow flag
     timer1_overflow_count++;
   }
 
   // Combine overflow count and current timer value
   uint32_t total_time = (65536UL * timer1_overflow_count + TCNT1) / 2;
 
   sei(); // Re-enable interrupts
   return total_time; // Return elapsed microseconds
 }
 
 // ---------------- Measurement Start ----------------
 void start() {
   _delay_ms(1000);   // startup delay
   reset_timer1();
   event = 0;
 
   digitalWrite(LED_BUILTIN, HIGH); // signal start
 
   // Capture initial pin states (UNO uses PIND and PINB)
   initial1 = PIND;  // D0..D7
   initial2 = PINB;  // D8..D13
   state1 = initial1;
   state2 = initial2;
 
   // Reset change buffers
   for (int i=0;  i < samples; i++) {
     pinChanged1[i]=0;
     pinChanged2[i]=0;
   }
 }
 
 // ---------------- Data Transmission ----------------
 void sendData() {
   digitalWrite(LED_BUILTIN, LOW);
 
   // Start marker
   Serial.println("S");
 
   // Send initial pin states and number of samples
   Serial.print(initial1); Serial.print(','); Serial.print(initial2); Serial.print(":");
   Serial.println(samples+2);
 
   // Adjust timing so first event aligns with offset
   timefix = -timer[0];
   for (int i = 0; i < samples; i++) {
     timer[i]=timer[i]+timefix;
   }
 
   // Assert all signals as previous states at negative offset
   Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println(0 - timezerooffset);
 
   // Send captured events
   for (int i = 0; i < samples; i++) {
     Serial.print(pinChanged1[i]); Serial.print(','); Serial.print(pinChanged2[i]); Serial.print(":");
     Serial.println(timer[i]);
   }
 
   // Insert final dummy event for visualization matters
   Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println((timer[samples-1] + 1));
 }
 
 // ---------------- Main Program ----------------
 int main(void) {
   Serial.begin(baudrate);
   init_board();
   init_timer();
 
   while (1) {
     while (Serial.read() != 'G'); // wait for "Go" signal from PC
     start(); // restart measurement
 
     while (1) {
       changeflag = 0;
 
       // Save old states
       old_state1 = state1;
       old_state2 = state2;
 
       // Read current states
       state1 = PIND;  // D0..D7
       state2 = PINB;  // D8..D13
 
       // Detect changes on PIND
       if (old_state1 != state1) {
         pinChanged1[event] = state1 ^ old_state1; // store which bit flipped
         changeflag = 1;
       }
       // Detect changes on PINB
       if (old_state2 != state2) {
         pinChanged2[event] = state2 ^ old_state2;
         changeflag = 1;
       }
 
       // If any change occurred, timestamp it
       if (changeflag != 0) {
         timer[event] = myMicros();
         event++;
       }
 
       // Once buffer is full, transmit data
       if (event == samples) {
         sendData();
         break;
       }
     }
   }
 }
 