/**
   MozziScout polysynth
   Can't play held chords because no diodes on Scout key matrix,
   but if you play a fast arpeggio, a chord will sound

   MozziScout is just like normal Scout,
   but pins 9 & 11 are swapped, so we can use Mozzi

    @todbot 12 Dec 2021
*/

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/cos8192_int8.h>
#include <mozzi_midi.h> // for mtof()
#include <ADSR.h>
#include <Keypad.h>

// SETTINGS
int octave = 3;

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

#define NUM_VOICES 5
#define CONTROL_RATE 64

Oscil<COS8192_NUM_CELLS, AUDIO_RATE> myOscs[ NUM_VOICES ] = {
  Oscil<COS8192_NUM_CELLS, AUDIO_RATE>(COS8192_DATA),
  Oscil<COS8192_NUM_CELLS, AUDIO_RATE>(COS8192_DATA),
  Oscil<COS8192_NUM_CELLS, AUDIO_RATE>(COS8192_DATA),
  Oscil<COS8192_NUM_CELLS, AUDIO_RATE>(COS8192_DATA),
  Oscil<COS8192_NUM_CELLS, AUDIO_RATE>(COS8192_DATA),
};

// volume control envelopes
ADSR <CONTROL_RATE, AUDIO_RATE> myEnvs[NUM_VOICES];

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

  for ( int i = 0; i < NUM_VOICES; i++) {
    myEnvs[i].setADLevels(255, 255);
    myEnvs[i].setTimes(100, 200, 750, 750); 
  }

  startMozzi(); // start with default control rate of 64
}

void loop() {
  audioHook(); // required here
}

byte note_list[NUM_VOICES] = {0};

void keypadEvent(KeypadEvent key) {
  byte note = 60 + (octave * 12) - 36 + key; // FIXME
  // weirdness with keypad, either because of Scout circuitry
  // or Keypad library: new pressed keys how up as HOLD if a
  // key is already PRESSED. Similarly, some RELEASED keys will
  // show up as IDLE events instead. sigh.
  switch (keys.getState()) {
    case PRESSED:
      Serial.print((byte)key); Serial.println(" press");
    case HOLD:
      Serial.print((byte)key); Serial.println(" hold");
      for(int i=0; i<NUM_VOICES; i++) {
        if( note_list[i] == note ) { // already pressed
          Serial.print("ALREADY PRESSED");
          myEnvs[i].noteOn();
          return;
        }
      }
      for(int i=0; i<NUM_VOICES; i++) { 
        if( note_list[i]==0) { // available
          note_list[i] = note;
          myEnvs[i].noteOn();
          myOscs[i].setFreq( mtof(note));
          break;
        }
      }
      break;
      
    case RELEASED:
      Serial.print((byte)key); Serial.println(" release");
    case IDLE:
      Serial.print((byte)key); Serial.println(" idle");
//      for(int i=0; i< NUM_VOICES; i++) { 
//        if( note == note_list[i] ) {
//          note_list[i] = 0; // say its available
//          myEnvs[i].noteOff(); 
//        }
//      }
      break;
      
      break;
  }
  Serial.print("note_list:"); 
  for(int i=0; i<NUM_VOICES;i++) { Serial.print(note_list[i]); Serial.print(',');}
  Serial.println();
}

void updateControl() {
  keys.getKeys();

  for (int i = 0; i < NUM_VOICES; i++) {
    myEnvs[i].update();
    if( myEnvs[i].adsr_playing == false ) { 
      note_list[i] = 0; // just in case
    }
  }
  
}
AudioOutput_t updateAudio() {
  long asig = (long) 0;
  for ( int i = 0; i < NUM_VOICES; i++) {
    //    asig += myOscs[i].next();
    asig += myOscs[i].next() * myEnvs[i].next();
  }
  return MonoOutput::fromAlmostNBit(20, asig); // 11-bits for 5 voices
  //  return MonoOutput::fromAlmostNBit(11, asig); // 11-bits for 5 voices
}

// 26 bits = 8 bits envelope + 18 bits signal
// return MonoOutput::fromAlmostNBit(26, envelope.next() * asig);
