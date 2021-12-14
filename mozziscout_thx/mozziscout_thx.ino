/**
   MozziScout sort-of like a THX "Deep Note" sound

   MozziScout is just like normal Scout,
   but pins 9 & 11 are swapped, so we can use Mozzi

    @todbot 12 Dec 2021
*/
#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/cos8192_int8.h>
#include <tables/triangle2048_int8.h>
#include <mozzi_midi.h> // for mtof()
#include <ADSR.h>
#include <Portamento.h>
#include <Keypad.h>

// SETTINGS
#define NUM_VOICES 8
int octave = 2;
int chord_notes[] = {0,5,12,19, -12,7,9,12 };
//int chord_notes[] = {0,3,7,10, 14,-12,0,12};

// Set up keyboard
const byte ROWS = 4;
const byte COLS = 5;
byte key_indexes[ROWS][COLS] = { // note this goes from 1-17, 0 is undefined
  {1, 5, 9, 12, 15},
  {2, 6, 10, 13, 16},
  {3, 7, 11, 14, 17},
  {4, 8}
};
byte rowPins[ROWS] = {7, 8, 11, 10}; // MozziScout: note pin 11 instead on 9 here
byte colPins[COLS] = {2, 3, 4, 5, 6};

Keypad keys = Keypad(makeKeymap(key_indexes), rowPins, colPins, ROWS, COLS);

// oscillators
Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> oscs [NUM_VOICES] = 
  Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE>(TRIANGLE2048_DATA);
// envelope for overall volume
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
// portamentos to slide the pitch around
Portamento <CONTROL_RATE> portamentos[NUM_VOICES];

byte note_offset = 48 + (octave*12) - 36; // FIXME

void blink(int count = 2, int wait = 200) {
  while (count >= 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(wait);
    digitalWrite(LED_BUILTIN, LOW);
    delay(wait);
    count = count - 1;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  keys.addEventListener(keypadEvent); // Add an event listener for this keypad

  blink();

  envelope.setADLevels(255, 255);
  envelope.setTimes(350, 200, 20000, 100); // 20000 is so the note will sustain 20 seconds unless a noteOff comes
  for( int i=0; i<NUM_VOICES; i++) {
    portamentos[i].setTime(1000u + i*1000u); // notes later in chord_notes take longer
    portamentos[i].start((byte)(note_offset + random(-12, 12)));
  }
  startMozzi(); // start with default control rate of 64
}

//
void loop() {
  audioHook(); // required here, don't put anything else in loop
}

// Keymap key event callback
void keypadEvent(KeypadEvent key) {
  byte note = note_offset + key; 
  
  // weirdness with keypad, either because of Scout circuitry
  // or Keypad library: new pressed keys how up as HOLD if a
  // key is already PRESSED. Similarly, some RELEASED keys will
  // show up as IDLE events instead. sigh.
  switch (keys.getState()) {
    case PRESSED:
    case HOLD:      
      Serial.print((byte)key); Serial.println(" press");
      digitalWrite(LED_BUILTIN, HIGH);
      for(int i=0; i<NUM_VOICES; i++) { 
        portamentos[i].start((byte)(note + chord_notes[i]));      
      }
      envelope.noteOn();
      break;
      
    case RELEASED:
    case IDLE:
      digitalWrite(LED_BUILTIN, LOW);
      Serial.print((byte)key); Serial.println(" released or idle");
      envelope.noteOff();
      for(int i=0; i<NUM_VOICES; i++) {
        portamentos[i].start((byte)(note_offset + random(-12, 12)));        
      }
      break;
  }
}

// Mozzi function to update control-rate things like LFOs & envelopes
void updateControl() {
  keys.getKeys();

  envelope.update();

  for( int i=0; i<NUM_VOICES; i++) {
    oscs[i].setFreq_Q16n16(portamentos[i].next());
  }
}

// Mozzi function to update audio
AudioOutput_t updateAudio() {
  long asig = (long) 0;
  for( int i=0; i<NUM_VOICES; i++) {
    asig += oscs[i].next();    
  }
  // 19 = 8 bits signal * 8 bits envelope = 16 bits + exp(NUM_VOICES,1/2) 
  return MonoOutput::fromAlmostNBit(19, envelope.next() * asig);
}
