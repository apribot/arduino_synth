/* TODO:
    filter/reso
 */

#include <MozziGuts.h>
#include <Oscil.h>
#include <EventDelay.h>
#include <ADSR.h> 
#include <mozzi_rand.h>
#include <mozzi_midi.h>
#include <MIDI.h>
#include <IntMap.h>
#include <LowPassFilter.h>

#include <tables/sin512_int8.h>
#include <tables/saw512_int8.h>
#include <tables/triangle512_int8.h>
#include <tables/square_no_alias512_int8.h>
#define NUM_TABLES 4
const int8_t *WAVE_TABLES[NUM_TABLES] = {SQUARE_NO_ALIAS512_DATA, SIN512_DATA, SAW512_DATA, TRIANGLE512_DATA};
#define TABLE_SIZE 512

MIDI_CREATE_DEFAULT_INSTANCE();
#define CONTROL_RATE 64

/*
 *  current pot to input mapping
 *  4 5 6 7
 *  0 2 3
 */
#define ATTACK_POT 4
#define RELEASE_POT 5

#define WAVEFORM2_POT 6
#define WAVEFORM2_DETUNE_POT 7
#define WAVEFORM_POT 0
#define LP_CUTOFF_POT 2
#define LP_RESO_POT 3

Oscil <TABLE_SIZE, AUDIO_RATE> aOscil(SQUARE_NO_ALIAS512_DATA);
Oscil <TABLE_SIZE, AUDIO_RATE> bOscil(SQUARE_NO_ALIAS512_DATA);

LowPassFilter lpf;       

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

boolean note_is_on = false;
byte j = 1;           //a counter.

byte note;

//Intensity of phase
byte attack_level = 255;
byte decay_level = 255;

//Duration of each phase
unsigned int sustain=30000; // arbitrarily long, idk

//enum for nice names
enum Potentiometers {
  WaveFormPot,
  AttackPot,
  ReleasePot,
  LPFResonancePot,
  LPFCutoffPot,
  WaveForm2Pot,
  WaveForm2DetunePot,
  POTENTIOMETER_COUNT
};

int16_t pots[POTENTIOMETER_COUNT];

//Used to mute output if no notes are being played.
boolean no_note=true;
int subosc = 0;
EventDelay noteDelay;

const IntMap attackIntMap(0,1024,0,2500);    // Min value must be large enough to prevent click at note start.
const IntMap decayIntMap(0,1024,28,3000);    // Min value must be large enough to prevent click at note start.
const IntMap sustainIntMap(0,1024,0,255);    // Min value must be large enough to prevent click at note start.
const IntMap releaseIntMap(0,1024,25,3000);   // Min value must be large enough to prevent click at note end.
const IntMap cutoffIntMap(0, 1024, 20, 240);  // Valid range 0-255 corresponds to freq 0-8192 (audio rate/2).
const IntMap resonanceIntMap(0, 1024, 20, 230);  // Valid range 0-255 corresponds to freq 0-8192 (audio rate/2).
const IntMap detuneIntMap(0, 1024, -12, 13); // pot is bad, cant always reach max

const int8_t *potValueToWaveTable (unsigned int value) {
  for (uint8_t i = 0; i < NUM_TABLES-1; ++i) {
    if (value <= (1024 / NUM_TABLES)) return (WAVE_TABLES[i]);
    value -= (1024 / NUM_TABLES);
  }
  return WAVE_TABLES[NUM_TABLES-1];
}



void handleNoteOn(byte channel, byte pitch, byte velocity) {
    note_is_on = true;
    aOscil.setFreq((int)mtof(pitch));

    subosc = (int) pitch + pots[WaveForm2DetunePot];

    bOscil.setFreq((int)mtof((byte) subosc));

    envelope.noteOn();
    envelope.update();
    digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)   
    note = pitch;
}

void handleNoteOff(byte channel, byte pitch, byte velocity) {
    digitalWrite(13, LOW);   // turn the LED off

    if(note == pitch) {
      envelope.noteOff();  
      noteDelay.start(pots[ReleasePot]);
      note_is_on = false;
    }
}

void setup() {
    pinMode(13, OUTPUT);     

    envelope.setADLevels(250,250);               // Sets Attack and Decay Levels; assumes Sustain, Decay, and Idle times
    envelope.setDecayTime(100);                  // Sets Decay time in milliseconds
    envelope.setSustainTime(32500);              // Sustain Time setting for envelope1 
  
    byte midi_note = 1;
    aOscil.setFreq((int)mtof(midi_note));
    bOscil.setFreq((int)mtof(midi_note));

    lpf.setResonance(220);

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
  
    switch (j){
      case 1:
        pots[WaveFormPot] = mozziAnalogRead(WAVEFORM_POT);
        aOscil.setTable(potValueToWaveTable(pots[WaveFormPot]));
        break;
      case 2:
        pots[AttackPot] = attackIntMap(mozziAnalogRead(ATTACK_POT));    // pot 3
        envelope.setAttackTime(pots[AttackPot]);
      break;
      case 3:
        pots[WaveForm2Pot] = mozziAnalogRead(WAVEFORM2_POT);
        bOscil.setTable(potValueToWaveTable(pots[WaveForm2Pot]));
      break;
      case 4:
        pots[WaveForm2DetunePot] = detuneIntMap(mozziAnalogRead(WAVEFORM2_DETUNE_POT));    // Pot 4
      break;
      case 5:
        pots[ReleasePot] = releaseIntMap(mozziAnalogRead(RELEASE_POT));    // Pot 4
        envelope.setReleaseTime(pots[ReleasePot]); 
      break;
      case 7:    
        pots[LPFCutoffPot] = cutoffIntMap(mozziAnalogRead(LP_CUTOFF_POT));             // Pot 7
        lpf.setCutoffFreq(pots[LPFCutoffPot]);
      break;
      case 8:
        pots[LPFResonancePot] = resonanceIntMap(mozziAnalogRead(LP_RESO_POT));             // Pot 7
        lpf.setResonance(pots[LPFResonancePot]);
      break;
    }
    j++;
    if(j>8){
      j=0;
    }         // This index steps us through the above seven pot reads, one per control update.
  
    envelope.update(); //idk if this is needed

    MIDI.read();
} 

int updateAudio() {
    if(no_note)
      return 0;
    else {
      return ((long) envelope.next() * lpf.next( (aOscil.next() + bOscil.next())>>1 ) )>>9;
    }
}

void loop() {
    audioHook();     
}
