/**
 * MozziScout "Chord Organ"-like synth 
 * based on Music Thing Modular's Chord Organ
 * https://musicthing.co.uk/pages/chord.html
 *
 * To change parameters:
 *  - Press high D# to increment chord 
 *  - Press high C# to decrement chord
 *  
 *  MozziScout is just like normal Oskitone Scout,
 *  but pins 9 & 11 are swapped, so we can use Mozzi
 *
 *  @todbot 21 Dec 2021
 **/

#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/triangle_analogue512_int8.h>
#include <tables/square_analogue512_int8.h>
#include <tables/saw_analogue512_int8.h>
#include <tables/cos2048_int8.h> // for filter modulation
#include <ADSR.h>
#include <Portamento.h>
#include <mozzi_midi.h> // for mtof()
#include <mozzi_rand.h> // for rand()

#include <Keypad.h>

// SETTINGS
#define NUM_VOICES 5
int octave = 2;
int detune = 50;  // 50 sounds pretty good for octave 2
int portamento_time = 50;  // milliseconds
int env_release_time = 750; // milliseconds

byte note_offset = 12 + (octave * 12) - 1; // FIXME

byte chord_id = 0;
// stolen from Chord Organ config page http://polyfather.com/chord_organ/
byte chord_notes[][5] = { 
  {0,4,7,12,0},    // 0 Major
  {0,3,7,12,0},    // 1 Minor
  {0,4,7,11,0},    // 2 Major 7th
  {0,3,7,10,0},    // 3 Minor 7th
  {0,4,7,11,14},   // 4 Major 9th
  {0,3,7,10,14},   // 5 Minor 9th
  {0,5,7,12,0},    // 6 Suspended 4th
  {0,7,12,0,7},    // 7 Power 5th
  {0,5,12,0,5},    // 8 Power 4th
  {0,4,7,8,0},     // 9 Major 6gh
  {0,3,7,8,0},     // 10 Minor 6th
  {0,3,6,0,3},     // 11 Diminished
  {0,4,8,0,4},     // 12
  {0,0,0,0,0},     // 13 Root 
  {-12,-12,0,0,0}, // 14 2 down octave
  {-12,0,0,12,24}, // 15 1 down oct, 1 up, 2 up oct
};
#define chord_count (sizeof(chord_notes) / sizeof(chord_notes[0]))

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
// Set up keyboard done

Oscil<SAW_ANALOGUE512_NUM_CELLS, AUDIO_RATE> aOscs[ NUM_VOICES];
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA); // filter mod
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
Portamento <CONTROL_RATE> portamentos[NUM_VOICES];

byte note, last_note;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  startMozzi();

  for( int i=0; i<NUM_VOICES; i++) { 
    aOscs[i].setTable( SAW_ANALOGUE512_DATA );
    portamentos[i].setTime(portamento_time);
  }

  envelope.setADLevels(255, 255);
  envelope.setTimes(100, 200, 20000, env_release_time);

  Serial.println("mozziscout_chordorgan");
}

// loop can only contain audioHook
void loop() {
  audioHook();
}

// mozzi function, used to update modulation-rate things
void updateControl() {
  scanKeys();
  
  envelope.update();

  for( int i=0; i<NUM_VOICES; i++) {
    Q16n16 pf = portamentos[i].next();  // Q16n16 is a fixed-point fraction in 32-bits (16bits . 16bits)
    aOscs[i].setFreq_Q16n16(pf);
    //aOscs[i].setFreq_Q16n16(pf + Q7n8_to_Q15n16(rand(detune))); // this does not sound as good
  }
}

// mozzi function, used to update audio
AudioOutput_t updateAudio() {
  long asig = 0;
  for ( int i = 0; i < NUM_VOICES; i++) {
    asig += aOscs[i].next();
  }
  return MonoOutput::fromAlmostNBit(18, asig * envelope.next());
}

bool chord_switched = false;

// read the keys, trigger notes
void scanKeys() {
  if ( !keys.getKeys() ) { return; } // no keys, get out

  for (int k = 0; k < LIST_MAX; k++) { // Scan the whole key list.
    //if ( !keys.key[k].stateChanged ) {  continue; } // skip keys that haven't changed
    byte key = keys.key[k].kchar;
    byte kstate = keys.key[k].kstate;
    
    if ( kstate == PRESSED || kstate == HOLD) {
      note = note_offset + key;
      Serial.print("chord:"); Serial.print((byte)chord_id); Serial.print(" key:"); Serial.print((byte)key); Serial.println(" presssed/hold");
      digitalWrite(LED_BUILTIN, HIGH);
      switch(key) { 
        case 14:  // high C#
          chord_id = (chord_id - 1) % chord_count;
          note = last_note;
          chord_switched = true;
          break;
        case 16:  // high D#
          chord_id = (chord_id + 1) % chord_count;
          note = last_note;
          chord_switched = true;
          break;
        default:
          chord_switched = false;
      } // switch
      
      for( int i=0; i<NUM_VOICES; i++) {
         portamentos[i].start(Q8n0_to_Q16n16(note + chord_notes[chord_id][i]) + Q7n8_to_Q15n16(rand(detune)));
        //portamentos[i].start(Q8n0_to_Q16n16(note + chord_notes[chord_id][i]));
      }
      if( !chord_switched ) {
        envelope.noteOn();
      }
      last_note = note;
    }
    else if( kstate == RELEASED ) { 
      Serial.print("chord:"); Serial.print((byte)chord_id); Serial.print(" key:"); Serial.print((byte)key); Serial.println(" released");
      digitalWrite(LED_BUILTIN, LOW);
      if( !chord_switched ) {
        envelope.noteOff();
      }
    }
  }
}
