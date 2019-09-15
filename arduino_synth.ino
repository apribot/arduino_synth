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

#define WAVEFORM_POT 0
#define ATTACK_POT 2
#define DECAY_POT 3
#define LP_CUTOFF_POT 4
#define LP_RESO_POT 5
#define LFO_SPEED_POT 6
#define LFO_WAVEFORM_POT 7

Oscil <TABLE_SIZE, AUDIO_RATE> aOscil(SQUARE_ANALOGUE512_DATA);

ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

boolean note_is_on = false;

byte note;

/*
* Paramethers of note
*/
//Intensity of phase
byte attack_level = 255;
byte decay_level = 0;

//Duration of each phase
unsigned int attack=50;
unsigned int decay=10;
unsigned int sustain=20;
unsigned int release_ms=20;

enum Potentiometers {
  WaveFormPot,
  AttackPot,
  DecayPot,
  LPFCutoffPot,
  LPFResonancePot,
  LFOSpeedPot,
  LFOWaveFormPot,
  POTENTIOMETER_COUNT
};

uint16_t pots[POTENTIOMETER_COUNT];

//Used to mute output if no notes are being played.
boolean no_nota=true;
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
  pots[LPFCutoffPot] = mozziAnalogRead(LP_CUTOFF_POT);
  pots[LPFResonancePot] = mozziAnalogRead(LP_RESO_POT);
  pots[LFOSpeedPot] = mozziAnalogRead(LFO_SPEED_POT);
  pots[LFOWaveFormPot] = mozziAnalogRead(LFO_WAVEFORM_POT);  
}

void handleNoteOn(byte channel, byte pitch, byte velocity)
{

    aOscil.setTable(potValueToWaveTable(pots[WaveFormPot]));

    envelope.setTimes(
      pots[AttackPot]+50,
      (pots[DecayPot] * 5) + 20,
      sustain,
      release_ms);   


    aOscil.setFreq((int)mtof(pitch));
    note_is_on = true;
    
    envelope.noteOn();
    envelope.update();
    digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)   
    note = pitch;
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
    digitalWrite(13, LOW);   // turn the LED off

    
    if(note == pitch) {
      envelope.noteOff();  
      noteDelay.start(release_ms+50);
      note_is_on = false;
    }
}

void setup(){
    pinMode(13, OUTPUT);     

    readPots();

    envelope.setLevels(attack_level,decay_level, 0, 0);
 
    // generate a random new adsr time parameter value in milliseconds
    envelope.setTimes(attack,decay,sustain,release_ms);   

    byte midi_note = 1;
    aOscil.setFreq((int)mtof(midi_note));
  
    startMozzi(CONTROL_RATE);
    MIDI.setHandleNoteOn(handleNoteOn);

    // Do the same for NoteOffs
    MIDI.setHandleNoteOff(handleNoteOff);

    // Initiate MIDI communications, listen to all channels
    MIDI.begin(MIDI_CHANNEL_OMNI);
}


void updateControl(){      
    if(noteDelay.ready() && note_is_on == false)
      no_nota=true;
    else
      no_nota=false;
  
    readPots();
  
    envelope.update(); //idk if this is needed
} 

int updateAudio(){
    if(no_nota)
      return 0;
    else {
      return ((long) envelope.next() * aOscil.next())>>8;
    }
}

void loop(){
    MIDI.read();
    audioHook();     
}
