/**
   MozziScout wubwubwub synth using LowPassFilter16
   based a little on Mozzi example "LowPassFilter"

   This sketch has startup modes!
   Hold a key on power up to change behavior:
   - Hold low C for slow filter mod
   - Hold low C# for even slower filter mod
   - Hold low D for even SLOWER filter mod
   - Hold low F to change wave from Saw to Triangle
   - Hold low F# to change wave from Saw to Square

   MozziScout is just like normal Scout,
    but pins 9 & 11 are swapped, so we can use Mozzi

    @todbot 14 Dec 2021
*/

#include <MozziGuts.h>
#include <Oscil.h>
//#include <tables/triangle_warm8192_int8.h>
#include <tables/triangle_analogue512_int8.h>
#include <tables/square_analogue512_int8.h>
#include <tables/saw_analogue512_int8.h>
#include <tables/cos2048_int8.h> // for filter modulation
#include <LowPassFilter.h>
#include <ADSR.h>
#include <Portamento.h>
#include <mozzi_midi.h> // for mtof()

#include <Keypad.h>

// SETTINGS
int octave = 2;
float mod_amount = 0.5;
uint8_t resonance = 187; // range 0-255, 255 is most resonant
uint8_t cutoff = 59;     // range 0-255, corresponds to 0-8192 Hz
int portamento_time = 50;  // milliseconds
int env_release_time = 1000; // milliseconds

byte note_offset = 48 + (octave * 12) - 36; // FIXME

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

//
Oscil<SAW_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aOsc1(SAW_ANALOGUE512_DATA);
Oscil<SAW_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aOsc2(SAW_ANALOGUE512_DATA);

Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA); // filter mod

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
Portamento <CONTROL_RATE> portamento;
LowPassFilter lpf;

byte startup_mode = 0;
bool retrig_lfo = true;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  startMozzi();

  lpf.setCutoffFreqAndResonance(cutoff, resonance);
  kFilterMod.setFreq(4.0f);  // fast
  envelope.setADLevels(255, 255);
  envelope.setTimes(100, 200, 20000, env_release_time);
  portamento.setTime( portamento_time );

}

void loop() {
  audioHook();
}

void updateControl() {
  scanKeys();
  // map the lpf modulation into the filter range (0-255), corresponds with 0-8191Hz
  uint8_t cutoff_freq = cutoff + (mod_amount * (kFilterMod.next()/2));
  lpf.setCutoffFreqAndResonance(cutoff_freq, resonance);
  envelope.update();
  Q16n16 pf = portamento.next();
  aOsc1.setFreq_Q16n16(pf);
  aOsc2.setFreq_Q16n16(pf*1.02); // hmm, feels like this shouldn't work

}

AudioOutput_t updateAudio() {
  long asig = lpf.next( aOsc1.next() + aOsc2.next() );
  return MonoOutput::fromNBit(16, envelope.next() * asig);
}

void scanKeys() {
  if ( !keys.getKeys() ) { return; }

  for (int i = 0; i < LIST_MAX; i++) { // Scan the whole key list.
    byte key = keys.key[i].kchar;
    byte kstate = keys.key[i].kstate;
    if ( kstate == PRESSED || kstate == HOLD ) {
      if( millis() < 1000 ) { handleModeChange(key); }
      byte note = note_offset + key;
      Serial.print((byte)key); Serial.println(" presssed/hold");
      digitalWrite(LED_BUILTIN, HIGH);
      portamento.start(note);
//      aOsc1.setFreq( mtof(note) ); // old way of direct set, now use portamento
//      aOsc2.setFreq( (float)(mtof(note) * 1.01) );
      envelope.noteOn();
    }
    else if( kstate == RELEASED ) { 
      Serial.print((byte)key); Serial.println(" released");
      digitalWrite(LED_BUILTIN, LOW);
      envelope.noteOff();
    }
    else {
//      Serial.print((byte)key); Serial.println(kstate);
    }
  }
}

// allow some simple parameter changing based on keys
// pressed on startup
void handleModeChange(byte m) {
  startup_mode = m;
  Serial.print("mode change:"); Serial.println((byte)m);
  if ( startup_mode == 1 ) {      // lowest C held
    kFilterMod.setFreq(0.5f);     // slow
  }
  else if ( startup_mode == 2 ) { // lowest C# held
    kFilterMod.setFreq(0.25f);    // slower
    retrig_lfo = false;
  }
  else if ( startup_mode == 3 ) {  // lowest D held
    kFilterMod.setFreq(0.05f);      // offish, almost no mod
    cutoff = 78;
  }
  else if ( startup_mode == 6 ) {
    aOsc1.setTable(TRIANGLE_ANALOGUE512_DATA);
    aOsc2.setTable(TRIANGLE_ANALOGUE512_DATA);
    cutoff = 65;
  }
  else if ( startup_mode == 7 ) {
    aOsc1.setTable(SQUARE_ANALOGUE512_DATA);
    aOsc2.setTable(SQUARE_ANALOGUE512_DATA);
   //envelope.setADLevels(230, 230); // square's a bit too loud so it clips
   resonance = 130;
  }
  else {                          // no keys held
    kFilterMod.setFreq(4.0f);     // fast
  }
}
