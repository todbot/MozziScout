/**
 * MozziScout wubwubwub synth using LowPassFilter16
 * based on Mozzi example "LowPassFilter"
 *
 * MozziScout is just like normal Scout,
 *  but pins 9 & 11 are swapped, so we can use Mozzi
 *
 *  @todbot 14 Dec 2021
*/

#include <MozziGuts.h>
#include <Oscil.h>
//#include <tables/triangle_warm8192_int8.h>
#include <tables/triangle_analogue512_int8.h>
#include <tables/square_analogue512_int8.h>
#include <tables/saw_analogue512_int8.h>
#include <tables/cos2048_int8.h> // for filter modulation
#include <LowPassFilter.h>
#include <mozzi_rand.h>
#include <mozzi_midi.h> // for mtof()
#include <Keypad.h>

// SETTINGS
#define NUM_VOICES 8
int octave = 2;
int chord_notes[] = {0,5,12,19, -12,7,9,12 };
//int chord_notes[] = {0,3,7,10, 14,-12,0,12};
byte note_offset = 48 + (octave*12) - 36; // FIXME

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

// pick one
Oscil<SAW_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aCrunchySound(SAW_ANALOGUE512_DATA);
//Oscil<TRIANGLE_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aCrunchySound(TRIANGLE_ANALOGUE512_DATA);
//Oscil<SQUARE_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aCrunchySound(SQUARE_ANALOGUE512_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA);

LowPassFilter16 lpf;

uint16_t resonance = 48000; // range 0-65535, 65535 is most resonant
byte startup_mode = 0;
bool retrig_lfo = true;

void setup(){
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  keys.addEventListener(keypadEvent); // Add an event listener for this keypad

  startMozzi();
  aCrunchySound.setFreq( mtof(note_offset+1) );
  kFilterMod.setFreq(4.0f);  // fast
}

void loop(){
  audioHook();
}

void updateControl(){
  keys.getKeys();
//  if (rand(CONTROL_RATE/2) == 0){ // about once every half second
//    kFilterMod.setFreq((float)rand(255)/64);  // choose a new modulation frequency
//  }
  // map the lpf modulation into the filter range (0-255), corresponds with 0-8191Hz
  // map the lpf16 modulation into the filter range (0-65535), corresponds with 0-8191Hz
  uint16_t cutoff_freq = 10000 + (kFilterMod.next() * 64);
  lpf.setCutoffFreqAndResonance(cutoff_freq, resonance);
}

AudioOutput_t updateAudio(){
  int asig = lpf.next(aCrunchySound.next());
  return MonoOutput::fromAlmostNBit(10,asig); // need extra bits because filter reasonance
}

// allow some simple parameter changing based on keys
// pressed on startup
void handleModeChange(byte m) {
  startup_mode = m;
  if( startup_mode == 1 ) {       // lowest C held
    kFilterMod.setFreq(0.5f);     // slow
  }
  else if( startup_mode == 2 ) {  // lowest C# held
    kFilterMod.setFreq(0.25f);    // slower
    retrig_lfo = false;
  }
  else {                          // no keys held
    kFilterMod.setFreq(4.0f);     // fast    
  }
}

// Keymap key event callback
void keypadEvent(KeypadEvent key) {
  byte note = note_offset + key;
  switch (keys.getState()) {
    case PRESSED:
      if( millis() < 1500 ) { handleModeChange(key); } // FIXME: how to do this in setup()
      if( retrig_lfo ) { 
        kFilterMod.setPhase(0); // only retrigger LFO phase on new press
      }
    case HOLD:
      Serial.print(" mode:"); Serial.print((byte)startup_mode); Serial.print(" ");
      Serial.print((byte)key); Serial.println(" presssed/hold");
      digitalWrite(LED_BUILTIN, HIGH);
      aCrunchySound.setFreq( mtof(note) );
      break;
      
    case RELEASED:
    case IDLE:
      digitalWrite(LED_BUILTIN, LOW);
      Serial.print((byte)key); Serial.println(" released or idle");
      break;
  }
}
