/*
 * Created: 11/12/2016 19.35.51
 * Author : Vincenzo / Sancho11
 */

 #define baudrate 115200 // Serial baudrate (must match other receiver)
 #define samples 750     // Number of signal samples to capture
 #define timezerooffset 1 // Microseconds offset for time correction
 #define PULLUP true     // Enable internal pull-ups if true
 #define F_CPU 16000000UL // CPU frequency (16 MHz typical for Arduino Mega/Uno)
 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <util/delay.h>
 #define prescaler 0x02  // Timer1 prescaler (sets clock division factor) (every tick of the clock is 0.5uS thus 1/2)
                         // Timer1 counts every 8 cycles of clock (at 2MHz) if we devide 1 second by 2MHz we get 0.5uS
volatile uint16_t timer1_overflow_count;

uint8_t initial1, initial2, initial3, state1, state2, state3, old_state1, old_state2, old_state3;
uint8_t pinChanged1[samples];
uint8_t pinChanged2[samples];
//uint8_t pinChanged3[samples];
uint32_t timer[samples];
uint32_t timefix;
uint16_t event = 0;
uint8_t changeflag = 0;

/*
initialX → captures initial states of the pins.
stateX / old_stateX → track changes on ports A, L, and C.
pinChangedX[] → buffer storing which pin(s) changed at each event.
timer[] → stores the timestamp (µs) of each event.
event → index of current captured change.
changeflag → flag indicating if a change occurred.
*/

 // ---------------- Board Initialization ----------------
 void init_board() {
   DDRB = 0x00;     
   DDRC = 0x00;     
   DDRL = 0x00;     // configure ports as input
 
   if (PULLUP){
     // Enable internal pull-ups so floating pins read HIGH
     for (uint8_t p = 22; p <= 49; p++){
       pinMode(p, INPUT_PULLUP);
     }
   } else {
     // Leave pins floating (external pull-down required)
     for (uint8_t p = 22; p <= 49; p++){
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
 
   sei();                // Enable global interrupts
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
 
 // Custom micros() implementation (with 0.5 µs resolution → converted to µs)
 uint32_t myMicros () {
   cli(); // Disable interrupts for atomic read
 
   // If overflow occurred but not yet serviced
   if (TIFR1 & (1 << TOV1)) {
     TIFR1 = (1 << TOV1); // Correct way to clear overflow flag
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
 
   // Capture initial pin states
   initial1 = PINA;
   initial2 = PINL;
   //initial3 = PINC;
   state1 = initial1;
   state2 = initial2;
   //state3 = initial3;
 
   // Reset change buffers
   for (int i=0;  i < samples; i++) {
     pinChanged1[i]=0;
     pinChanged2[i]=0;
     //pinChanged3[i]=0;
   }
 }
 
 // ---------------- Data Transmission ----------------
 void sendData() {
   digitalWrite(LED_BUILTIN, LOW);
 
   // Start marker
   Serial.println("S");
 
   // Send initial pin states and number of samples
   //Serial.print(initial1); Serial.print(','); Serial.print(initial2); Serial.print(','); Serial.print(initial3); Serial.print(":");
   Serial.print(initial1); Serial.print(','); Serial.print(initial2); Serial.print(":");
   Serial.println(samples+2);
 
   // Adjust timing so first event aligns with offset
   timefix = -timer[0];
   for (int i = 0; i < samples; i++) {
     timer[i]=timer[i]+timefix;
   }
 
   // Lets start by asserting all signals as previous states.
   //Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println(0-timezerooffset);
 
   // Send captured events
   for (int i = 0; i < samples; i++) {
     //Serial.print(pinChanged1[i]);Serial.print(','); Serial.print(pinChanged2[i]); Serial.print(','); Serial.print(pinChanged3[i]); Serial.print(":");
     Serial.print(pinChanged1[i]);Serial.print(','); Serial.print(pinChanged2[i]); Serial.print(":");
     Serial.println(timer[i]);
   }

   // Insert final dummy event for visualization matters
   //Serial.print(B0);Serial.print(','); Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.print(B0);Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println((timer[samples-1]+1));
 }
 
 // ---------------- Main Program ----------------
 int main(void) {
   Serial.begin(baudrate);
   init_board();
   init_timer();
 
   while (1) {
    while (Serial.read() != 'G'); // wait for "Go" signal from PC
    start(); // restart measurement
    while(1){
     changeflag=0;
 
     // Save old states
     old_state1 = state1;
     old_state2 = state2;
     //old_state3 = state3;
 
     // Read current states
     state1 = PINA;
     state2 = PINL;
     //state3 = PINC;
 
     // Detect changes on port A
     if (old_state1 != state1 ) {
       pinChanged1[event] = state1 ^ old_state1; // store which bit flipped
       changeflag=1;
     }
     // Detect changes on port L
     if (old_state2 != state2 ) {
       pinChanged2[event] = state2 ^ old_state2;
       changeflag=1;
     }
     // Detect changes on port C
     //if (old_state3 != state3 ) {
     //  pinChanged3[event] = state3 ^ old_state3;
     //  changeflag=1;
     //}
 
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
 