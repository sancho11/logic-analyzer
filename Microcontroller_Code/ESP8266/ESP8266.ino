/*
 * Ported from Arduino Mega to ESP8266
 * Keep Mega-style syntax, structure, and serial protocol.
 *
 * Original Author (Mega): Vincenzo / Sancho11
 */

// ---------------- User Configuration ----------------
#define baudrate        115200     // Serial baudrate (must match other receiver)
#define samples         500        // Number of signal samples to capture (Mega default)
#define timezerooffset  1          // Microseconds offset for time correction (same meaning)
#define PULLUP          true       // Enable internal pull-ups if true

// Map up to 8 inputs to the virtual "Port A" byte and 8 to "Port L" byte.
// Use -1 for unused entries. (GPIO numbers, not Dx labels.)
int8_t PORTA_MAP[8] = {
  4,   // bit0  -> GPIO4  (D2)
  5,   // bit1  -> GPIO5  (D1)
  -1,  // bit2
  -1,  // bit3
  -1,  // bit4
  -1,  // bit5
  -1,  // bit6
  -1   // bit7
};

int8_t PORTL_MAP[8] = {
  12,  // bit0  -> GPIO12 (D6)
  14,  // bit1  -> GPIO14 (D5)
  -1,  // bit2
  -1,  // bit3
  -1,  // bit4
  -1,  // bit5
  -1,  // bit6
  -1   // bit7
};

// ---------------- Includes (ESP8266) ----------------
#include <Arduino.h>
extern "C" {
  #include <user_interface.h> // for wdt control
}

// ---------------- Mega-style Globals ----------------
volatile uint16_t timer1_overflow_count = 0; // kept for naming parity; unused on ESP8266

uint8_t initial1, initial2, initial3, state1, state2, state3, old_state1, old_state2, old_state3;
uint8_t pinChanged1[samples];
uint8_t pinChanged2[samples];
uint8_t pinChanged3[samples]; // kept for symmetry; unused in protocol
uint32_t timer[samples];
uint32_t timefix;
uint16_t event = 0;
uint8_t changeflag = 0;

/*
initialX → captures initial states of the pins.
stateX / old_stateX → track changes on the two virtual ports (A and L).
pinChangedX[] → buffer storing which bit(s) changed at each event.
timer[] → stores the timestamp (µs) of each event.
event → index of current captured change.
changeflag → flag indicating if a change occurred.
*/

// ---------------- Helpers to read virtual ports ----------------
uint8_t readVirtualPort(int8_t map8[8]) {
  uint8_t v = 0;
  for (uint8_t b = 0; b < 8; b++) {
    int8_t pin = map8[b];
    if (pin >= 0) {
      // digitalRead returns HIGH(1)/LOW(0). Pack as active-high bits like the Mega PINA/PINL.
      if (digitalRead(pin)) v |= (1 << b);
    }
  }
  return v;
}

void configVirtualPortPins(int8_t map8[8], bool usePullup) {
  for (uint8_t b = 0; b < 8; b++) {
    int8_t pin = map8[b];
    if (pin >= 0) {
      pinMode(pin, usePullup ? INPUT_PULLUP : INPUT);
    }
  }
}

// ---------------- Board Initialization ----------------
void init_board() {
  // Configure mapped pins as inputs (optionally with pull-ups)
  configVirtualPortPins(PORTA_MAP, PULLUP);
  configVirtualPortPins(PORTL_MAP, PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

// ---------------- Timer Stub (ESP keeps naming) ----------------
void init_timer() {
  // On ESP8266 we’ll use micros() directly; preserve function for structure parity.
  // No hardware timer configuration is required here.
}

void ICACHE_RAM_ATTR reset_timer1 () {
  // Naming kept for parity; we don’t use a hardware counter here.
  // If you wanted absolute parity, you could track an offset and subtract from micros().
  // For now, we just rely on micros() and normalize with timefix in sendData().
}

uint32_t ICACHE_RAM_ATTR myMicros () {
  // Directly use the ESP8266 core micros() (1µs resolution).
  return micros();
}

// ---------------- Measurement Start ----------------
void start() {
  delay(1000);    // startup delay (Mega used _delay_ms)

  reset_timer1();
  event = 0;

  digitalWrite(LED_BUILTIN, HIGH); // signal start

  // Capture initial states from the virtual ports
  initial1 = readVirtualPort(PORTA_MAP);
  initial2 = readVirtualPort(PORTL_MAP);
  state1   = initial1;
  state2   = initial2;

  // Reset change buffers
  for (int i = 0; i < samples; i++) {
    pinChanged1[i] = 0;
    pinChanged2[i] = 0;
  }
}

// ---------------- Data Transmission ----------------
void sendData() {
  digitalWrite(LED_BUILTIN, LOW);

  // Start marker
  Serial.println("S");

  // Send initial pin states and number of samples (EXACT Mega format: initial1,initial2:samples+2)
  Serial.print(initial1); Serial.print(','); Serial.print(initial2); Serial.print(":");
  Serial.println(samples + 2);

  // Adjust timing so first event aligns with offset (EXACT Mega math/order)
  timefix = -timer[0];
  for (int i = 0; i < samples; i++) {
    timer[i] = timer[i] + timefix;
  }

  // Assert “all signals as previous states” line (two fields, Mega sends B0,B0 then time 0-timezerooffset)
  Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
  Serial.println(0 - timezerooffset);

  // Send captured events (two bytes: changes on virtual Port A and Port L)
  for (int i = 0; i < samples; i++) {
    Serial.print(pinChanged1[i]); Serial.print(','); Serial.print(pinChanged2[i]); Serial.print(":");
    Serial.println(timer[i]);
  }

  // Final dummy event (visualization convenience), mirrors Mega
  Serial.print(B0); Serial.print(','); Serial.print(B0); Serial.print(":");
  Serial.println(timer[samples - 1] + 1);
}

// ---------------- Core sampling loop (mirrors Mega main while) ----------------
void capture_loop() {
  while (true) {
    changeflag = 0;

    // Save old states
    old_state1 = state1;
    old_state2 = state2;

    // Read current states
    state1 = readVirtualPort(PORTA_MAP);
    state2 = readVirtualPort(PORTL_MAP);

    // Detect changes on virtual Port A
    if (old_state1 != state1) {
      pinChanged1[event] = state1 ^ old_state1; // store which bit flipped
      changeflag = 1;
    }

    // Detect changes on virtual Port L
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

    // Small yield to keep WiFi/RTOS happy without impacting edge detection too much
    // (Remove if you need absolute tightest loop)
    // yield();
  }
}

// ---------------- Arduino Entry Points ----------------
void setup() {
  Serial.begin(baudrate);
  init_board();
  init_timer();
}

void loop() {
  // Mirror Mega: wait for 'G' from host before (re)starting
  while (Serial.read() != 'G') {
    delay(1);
  }

  // Watchdog handling similar to your ESP example
  digitalWrite(LED_BUILTIN, LOW);
  ESP.wdtDisable();

  start();           // restart measurement (captures initial states and clears buffers)
  capture_loop();    // run sampling until buffer fills

  ESP.wdtEnable(WDTO_8S);
  digitalWrite(LED_BUILTIN, HIGH);
}
