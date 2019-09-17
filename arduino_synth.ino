/* TODO:
    idk
 */

#include <MozziGuts.h>
#include <Oscil.h>
#include <EventDelay.h>
#include <ADSR.h> 
#include <mozzi_rand.h>
#include <mozzi_midi.h>
#include <MIDI.h>

#include <tables/sin512_int8.h>
#include <tables/saw_analogue512_int8.h>
#include <tables/triangle512_int8.h>
#include <tables/square_analogue512_int8.h>
#define NUM_TABLES 4
const int8_t *WAVE_TABLES[NUM_TABLES] = {SQUARE_ANALOGUE512_DATA, SIN512_DATA, SAW_ANALOGUE512_DATA, TRIANGLE512_DATA};
#define TABLE_SIZE 512

MIDI_CREATE_DEFAULT_INSTANCE();
#define CONTROL_RATE 64

/*
 *  current pot to input mapping
 *  4 5 6 7
 *  0 2 3
 */
#define ATTACK_POT 4
#define DECAY_POT 5
#define SUSTAIN_POT 6
#define RELEASE_POT 7
#define WAVEFORM_POT 0
#define LP_RESO_POT 2
#define LP_CUTOFF_POT 3

Oscil <TABLE_SIZE, AUDIO_RATE> aOscil(SQUARE_ANALOGUE512_DATA);

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

boolean note_is_on = false;

byte note;

//Intensity of phase
byte attack_level = 255;
byte decay_level = 0;

//Duration of each phase
unsigned int sustain=30000; // arbitrarily long, idk

//enum for nice names
enum Potentiometers {
  WaveFormPot,
  AttackPot,
  DecayPot,
  SustainPot,
  ReleasePot,
  LPFCutoffPot,
  LPFResonancePot,
  POTENTIOMETER_COUNT
};

uint16_t pots[POTENTIOMETER_COUNT];

//Used to mute output if no notes are being played.
boolean no_note=true;
EventDelay noteDelay;

const int8_t *potValueToWaveTable (unsigned int value) {
  for (uint8_t i = 0; i < NUM_TABLES-1; ++i) {
    if (value <= (1024 / NUM_TABLES)) return (WAVE_TABLES[i]);
    value -= (1024 / NUM_TABLES);
  }
  return WAVE_TABLES[NUM_TABLES-1];
}

void readPots() {
  pots[WaveFormPot] = mozziAnalogRead(WAVEFORM_POT);
  pots[AttackPot] = mozziAnalogRead(ATTACK_POT);
  pots[DecayPot] = mozziAnalogRead(DECAY_POT);
  pots[SustainPot] = mozziAnalogRead(SUSTAIN_POT);
  pots[ReleasePot] = mozziAnalogRead(RELEASE_POT);  
  pots[LPFCutoffPot] = mozziAnalogRead(LP_CUTOFF_POT);
  pots[LPFResonancePot] = mozziAnalogRead(LP_RESO_POT);
}

void grabSettings() {
    aOscil.setTable(potValueToWaveTable(pots[WaveFormPot]));

    envelope.setLevels(
      attack_level,
      decay_level, 
      pots[SustainPot] >> 2, 
      0
    );

    envelope.setTimes(
      ((pots[AttackPot] * pots[AttackPot]) >> 7) + 50, // exponential from 50ms to 8.242s
      ((pots[DecayPot]  * pots[DecayPot])  >> 7) + 20, // exponential from 20ms to 8.212s
      sustain,
      pots[ReleasePot] * 5
    );   
}

void handleNoteOn(byte channel, byte pitch, byte velocity) {
    grabSettings();
    note_is_on = true;
    aOscil.setFreq((int)mtof(pitch));
    envelope.noteOn();
    envelope.update();
    digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)   
    note = pitch;
}

void handleNoteOff(byte channel, byte pitch, byte velocity) {
    digitalWrite(13, LOW);   // turn the LED off

    if(note == pitch) {
      envelope.noteOff();  
      noteDelay.start((pots[ReleasePot] * 5)+50);
      note_is_on = false;
    }
}

void setup() {
    pinMode(13, OUTPUT);     

    readPots();

    grabSettings();
    byte midi_note = 1;
    aOscil.setFreq((int)mtof(midi_note));
  
    startMozzi(CONTROL_RATE);
    MIDI.setHandleNoteOn(handleNoteOn);

    // Do the same for NoteOffs
    MIDI.setHandleNoteOff(handleNoteOff);

    // Initiate MIDI communications, listen to all channels
    MIDI.begin(MIDI_CHANNEL_OMNI);
}


void updateControl() {      
    if(noteDelay.ready() && note_is_on == false)
      no_note=true;
    else
      no_note=false;
  
    readPots();
  
    envelope.update(); //idk if this is needed
} 

int updateAudio() {
    if(no_note)
      return 0;
    else {
      return ((long) envelope.next() * aOscil.next())>>8;
    }
}

void loop() {
    MIDI.read();
    audioHook();     
}
