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
int chord_notes[] = {0,5,12,19,-12, 7,9,12,5,0 };
//int chord_notes[] = {0,3,7,10,14};

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

#define CONTROL_RATE 64

// oscillators
Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> oscs [NUM_VOICES] = 
  Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE>(TRIANGLE2048_DATA);

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
Portamento <CONTROL_RATE> portamentos[NUM_VOICES];

byte note_offset = 60 + (octave*12) - 36; // FIXME

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

  envelope.setADLevels(255, 64);
  envelope.setTimes(350, 200, 20000, 1000); // 20000 is so the note will sustain 20 seconds unless a noteOff comes
  for( int i=0; i<NUM_VOICES; i++) {
    portamentos[i].setTime(1000u + i*1000u);
    portamentos[i].start((byte)(note_offset + random(-12, 12)));
  }
  startMozzi(); // start with default control rate of 64
}

void loop() {
  audioHook(); // required here
}

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
      for(int i=0; i<NUM_VOICES; i++) { 
        portamentos[i].start((byte)(note + chord_notes[i]));      
      }
      envelope.noteOn();
      break;
      
    case RELEASED:
    case IDLE:
      Serial.print((byte)key); Serial.println(" released or idle");
      envelope.noteOff();
      for(int i=0; i<NUM_VOICES; i++) {
        portamentos[i].start((byte)(note_offset + random(-12, 12)));        
      }
      break;
  }
}

void updateControl() {
  keys.getKeys();

  envelope.update();

  for( int i=0; i<NUM_VOICES; i++) {
    oscs[i].setFreq_Q16n16(portamentos[i].next());
  }
}

AudioOutput_t updateAudio() {
  long asig = (long) 0;
  for( int i=0; i<NUM_VOICES; i++) {
    asig += oscs[i].next();    
  }
  // 26 bits = 8 bits envelope + 18 bits signal
  return MonoOutput::fromAlmostNBit(19, envelope.next() * asig);
}
