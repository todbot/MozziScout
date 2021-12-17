/**
 * MozziScout drumkit + bass
 * Uses slightly modified "d_kit.h" from
 * https://github.com/diyelectromusic/sdemp/tree/master/src/SDEMP/ArduinoMozziSampleDrumMachine 
 * 
 * Drums are on these keys, mostly following general MIDI practice:
 * - low C  : Bass Drum (BD)
 * - low D  : Snare Drum (SD)
 * - low F# : Closed Hihat (CH)
 * - low G# : Open Hihat (OH)
 * - low C# : Set sample speed normal
 * - low D# : Set sample speed 0.5x
 * - low E  : Set sample speed 1.5x
 * - All other keys play bass tone
 * 
 *  MozziScout is just like normal Oskitone Scout,
 *  but pins 9 & 11 are swapped, so we can use Mozzi
 *
 *  @todbot 16 Dec 2021
 **/
 
#include <MozziGuts.h>
#include <Sample.h> // Sample template
#include <Oscil.h>
#include <ADSR.h>
#include <LowPassFilter.h>
#include <mozzi_midi.h>
#include <tables/saw2048_int8.h> // for bass sound

#include <Keypad.h>

// config!
float d_pitch = 1.0;
int octave = 1;
byte note_offset = 12 + (octave * 12) - 1; // FIXME
uint8_t resonance = 140; // range 0-255, 255 is most resonant
uint8_t cutoff = 20;    // range 0-255, corresponds to 0-8192 Hz

#include "samples/d_kit.h"
// use: Sample <table_size, update_rate> SampleName (wavetable)
Sample <BD_NUM_CELLS, AUDIO_RATE> aBD(BD_DATA);
Sample <SD_NUM_CELLS, AUDIO_RATE> aSD(SD_DATA);
Sample <CH_NUM_CELLS, AUDIO_RATE> aCH(CH_DATA);
Sample <OH_NUM_CELLS, AUDIO_RATE> aOH(OH_DATA);

Oscil <SAW2048_NUM_CELLS, AUDIO_RATE> aOsc1(SAW2048_DATA);
//Oscil <SIN2048_NUM_CELLS, CONTROL_RATE> kFilterMod(SIN2048_DATA); // filter mod
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
LowPassFilter lpf;

// Set up keyboard
const byte ROWS = 4;
const byte COLS = 5;
byte key_indexes[ROWS][COLS] = { // note this goes from 1-17, 0 is undefined
  {1, 5, 9, 12, 15},  {2, 6, 10, 13, 16},  {3, 7, 11, 14, 17},  {4, 8}
};
byte rowPins[ROWS] = {7, 8, 11, 10}; // MozziScout: note pin 11 instead on 9 here
byte colPins[COLS] = {2, 3, 4, 5, 6};
Keypad keys = Keypad(makeKeymap(key_indexes), rowPins, colPins, ROWS, COLS);

uint8_t cutoff_mod = 0;

void set_sample_pitches() {
  // set samples to play at the speed it was recorded, pitch-shifted potentially
  aBD.setFreq(((float) D_SAMPLERATE / (float) BD_NUM_CELLS) * d_pitch);
  aSD.setFreq(((float) D_SAMPLERATE / (float) SD_NUM_CELLS) * d_pitch);
  aCH.setFreq(((float) D_SAMPLERATE / (float) CH_NUM_CELLS) * d_pitch);
  aOH.setFreq(((float) D_SAMPLERATE / (float) OH_NUM_CELLS) * d_pitch);
}

void setup () {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  set_sample_pitches();
  
  envelope.setADLevels(200,127);
  envelope.setTimes(50,200,10000,700);
  lpf.setCutoffFreqAndResonance(cutoff, resonance);
  
  startMozzi();
  
  delay(1000);
  Serial.println("mozziscout_drums_bass");
}

void loop(){
  audioHook();
}

void scanKeys() {
  if ( !keys.getKeys() ) { return; }

  for (int i = 0; i < LIST_MAX; i++) { // Scan the whole key list.
    if ( !keys.key[i].stateChanged ) {  continue; } // skip keys that haven't changed
    byte key = keys.key[i].kchar;
    byte kstate = keys.key[i].kstate;
    
    if ( kstate == PRESSED ) {
      Serial.print((byte)key); Serial.println(" presssed");
      digitalWrite(LED_BUILTIN, HIGH);

      switch(key) {
        case 1:  // low C
          aBD.start(); 
          break;
       case 3: // low D
          aSD.start();
          break;
        case 7:  // low F#
          aCH.start();
          break;
        case 9:  // low G#
          aOH.start();
          break;
        case 2:  // low C#
          d_pitch = 1.0; 
          set_sample_pitches();
          break;
        case 4:  // low D#
          d_pitch = 0.5;
          set_sample_pitches();
          break;
        case 5:  // low E
          d_pitch = 1.5;
          set_sample_pitches();
          break;
        default: // all other notes play bass sound
          uint8_t note = key + note_offset;
          aOsc1.setFreq_Q16n16(Q16n16_mtof(Q8n0_to_Q16n16(note))); // more accurate than mtof()
          envelope.noteOn();
          cutoff_mod = 127; // really simple filter mod
      }
    }
    else if( kstate == RELEASED ) { 
      Serial.print((byte)key); Serial.println(" released");
      digitalWrite(LED_BUILTIN, LOW);
      envelope.noteOff();
    }
  } // for LIST_MAX
}

void updateControl() {
  scanKeys();
  envelope.update();
  uint8_t cutoff_freq = cutoff + cutoff_mod; 
  if( cutoff_mod >=2 ) { cutoff_mod -= 2; }
  lpf.setCutoffFreqAndResonance(cutoff_freq, resonance);
}

AudioOutput_t updateAudio() {
  int asig = (envelope.next() * lpf.next(aOsc1.next()))>>8; // bass  
  asig += aBD.next() + aSD.next() + aCH.next() + aOH.next(); // drums
  return MonoOutput::fromAlmostNBit(9, asig).clip();
}
