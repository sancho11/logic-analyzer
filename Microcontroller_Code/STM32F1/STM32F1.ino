/*
 * STM32F1 Logic Analyzer — Mega-style port
 * Author : Vincenzo / Sancho11 (ported to STM32F1 keeping Mega syntax)
 * Core   : Arduino_STM32 (Roger Clark / libmaple)
 */

 #define baudrate        115200   // Serial baudrate (must match other receiver)
 #define samples         500      // Number of signal samples to capture
 #define timezerooffset  1        // Microseconds offset for time correction
 #define PULLUP          true     // Enable internal pull-ups if true
 #define F_CPU           72000000UL // CPU frequency (STM32F1 runs at 72 MHz)
 
 #include <libmaple/timer.h>
 #include <libmaple/libmaple_types.h>
 #include <libmaple/nvic.h>
 
 // Timer2 @ 2 MHz => 0.5 µs per tick (72 MHz / (PSC+1) = 2 MHz  -> PSC = 35)
 #define TIMER_DEV           TIMER2
 #define TIMER_PRESCALER     35
 #define prescaler           TIMER_PRESCALER   // keep Mega's name for convention
 
 // Fallback for old Arduino "B0" macro used in the Mega code
 #ifndef B0
 #define B0 0
 #endif
 
 volatile uint32_t timer2_overflow_count = 0;
 
 uint8_t  initial1, initial2, state1, state2, old_state1, old_state2;
 uint8_t  pinChanged1[samples];
 uint8_t  pinChanged2[samples];
 uint32_t timerStamp[samples];
 uint32_t timefix;
 uint16_t event = 0;
 uint8_t  changeflag = 0;
 
 /*
 initialX → captures initial states (low/high byte of GPIOB).
 stateX / old_stateX → track changes for low and high bytes (like PINA/PINL).
 pinChangedX[] → buffer storing which pin(s) changed at each event (byte-wise).
 timerStamp[] → stores the timestamp (µs) of each event.
 event → index of current captured change.
 changeflag → flag indicating if a change occurred.
 */
 
 // ---------------- Low-level helpers ----------------
 static inline uint16_t readPortB() {
   // Read the 16-bit input data register of GPIOB (libmaple style)
   return GPIOB->regs->IDR;
 }
 
 // ---------------- Board Initialization ----------------
 void init_board() {
   // Configure PB0..PB15 as inputs (with or without pull-up)
   for (uint8_t p = PB0; p <= PB15; p++) {
     if (PULLUP) pinMode(p, INPUT_PULLUP);
     else        pinMode(p, INPUT);
   }
 
   pinMode(LED_BUILTIN, OUTPUT);
   digitalWrite(LED_BUILTIN, LOW);
 }
 
 // ---------------- Timer Initialization ----------------
 void timer2_isr(void) {
   // Update/overflow event
   if (timer_get_interrupt_status(TIMER_DEV, TIMER_UPDATE_INTERRUPT)) {
     timer2_overflow_count++;
     timer_clear_interrupt(TIMER_DEV, TIMER_UPDATE_INTERRUPT);
   }
 }
 
 void init_timer() {
   timer_pause(TIMER_DEV);
 
   // Upcounting, prescaler to 2 MHz ticks, 16-bit auto-reload like AVR T1
   timer_set_prescaler(TIMER_DEV, prescaler);
   timer_set_reload(TIMER_DEV, 0xFFFF);
 
   // Clear any pending flag, reset counter
   TIMER_DEV->regs.gen->CNT = 0;
   TIMER_DEV->regs.gen->SR  = 0;
 
   // Attach overflow ISR
   timer_attach_interrupt(TIMER_DEV, TIMER_UPDATE_INTERRUPT, timer2_isr);
 
   // Enable interrupt in NVIC and the peripheral
   timer_generate_update(TIMER_DEV);
   timer_resume(TIMER_DEV);
 }
 
 // Reset timer counter and overflow counter (Mega-style name)
 void reset_timer1 () {
   noInterrupts();
   TIMER_DEV->regs.gen->CNT = 0;
   TIMER_DEV->regs.gen->SR  = 0;        // clear UIF
   timer2_overflow_count    = 0;
   interrupts();
 }
 
 // Custom micros() implementation (0.5 µs tick → convert to µs)
 uint32_t myMicros () {
   noInterrupts();
 
   // If overflow flag set but ISR not yet run, account for it
   if (TIMER_DEV->regs.gen->SR & TIMER_SR_UIF) {
     TIMER_DEV->regs.gen->SR = ~TIMER_SR_UIF; // clear UIF
     timer2_overflow_count++;
   }
 
   // Combine overflow count and current counter
   uint32_t cnt   = TIMER_DEV->regs.gen->CNT;         // 0..65535
   uint32_t total = (65536UL * timer2_overflow_count + cnt) / 2; // 0.5µs → µs
 
   interrupts();
   return total;
 }
 
 // ---------------- Measurement Start ----------------
 void start() {
   delay(1000);   // startup delay (as in Mega)
   reset_timer1();
   event = 0;
 
   digitalWrite(LED_BUILTIN, HIGH); // signal start
 
   // Capture initial states of GPIOB as two bytes (low/high)
   uint16_t initial = readPortB();
   initial1 = (uint8_t)(initial & 0xFF);
   initial2 = (uint8_t)(initial >> 8);
   state1   = initial1;
   state2   = initial2;
 
   // Reset change buffers
   for (int i = 0; i < samples; i++) {
     pinChanged1[i] = 0;
     pinChanged2[i] = 0;
     timerStamp[i]  = 0;
   }
 }
 
 // ---------------- Data Transmission ----------------
 void sendData() {
   digitalWrite(LED_BUILTIN, LOW);
 
   // Start marker
   Serial.println("S");
 
   // Send initial states and number of samples (match Mega format: two ports)
   Serial.print(initial1); Serial.print(','); Serial.print(initial2); Serial.print(":");
   Serial.println(samples + 2);
 
   // Adjust timing so first event aligns with offset
   timefix = (uint32_t)(0 - timerStamp[0]);
   for (int i = 0; i < samples; i++) {
     timerStamp[i] = timerStamp[i] + timefix;
   }
 
   // Assert all signals as previous states at t = -timezerooffset
   Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println(0 - timezerooffset);
 
   // Send captured events
   for (int i = 0; i < samples; i++) {
     Serial.print(pinChanged1[i]); Serial.print(','); Serial.print(pinChanged2[i]); Serial.print(":");
     Serial.println(timerStamp[i]);
   }
 
   // Final dummy event for visualization matters (+1 µs after last)
   Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
   Serial.println((timerStamp[samples - 1] + 1));
 }
 
 // ---------------- Arduino entry points (Mega-style flow) ----------------
 void setup() {
   Serial.begin(baudrate);
   init_board();
   init_timer();
 }
 
 void loop() {
   // Wait for "Go" from host, then run a capture session
   while (Serial.read() != 'G') { /* idle */ }
   start();
 
   while (1) {
     changeflag = 0;
 
     // Save old states
     old_state1 = state1;
     old_state2 = state2;
 
     // Read current states (two bytes from GPIOB)
     uint16_t now    = readPortB();
     state1          = (uint8_t)(now & 0xFF);
     state2          = (uint8_t)(now >> 8);
 
     // Detect changes on low byte (pins PB0..PB7)
     if (old_state1 != state1) {
       pinChanged1[event] = state1 ^ old_state1; // which bits flipped
       changeflag = 1;
     }
     // Detect changes on high byte (pins PB8..PB15)
     if (old_state2 != state2) {
       pinChanged2[event] = state2 ^ old_state2;
       changeflag = 1;
     }
 
     // If any change occurred, timestamp it
     if (changeflag != 0) {
       timerStamp[event] = myMicros();
       event++;
     }
 
     // Once buffer is full, transmit data and exit to wait for next 'G'
     if (event == samples) {
       sendData();
       break;
     }
   }
 }
 