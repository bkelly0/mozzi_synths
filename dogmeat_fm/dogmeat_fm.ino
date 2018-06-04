#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> 
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <AutoMap.h>
#include <MIDI.h>

#define CONTROL_RATE 128 // powers of 2 please
#define MIDI_CHANNEL 15

Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> stereoModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kModIndex(COS2048_DATA);
AutoMap modRatioMap(0,1023,300,1280);
AutoMap intensityMap(0,1023,0,700);
AutoMap panMap(0, 1023, 0, 256);


Q8n8 mod_index;
int currentNote = 30;
Q16n16 deviation, modulation, carrier_freq, mod_freq;
long intensity;

int step = 0;
int mod_to_carrier_ratio = 768;
int audio_out_1, audio_out_2, panDir, val;
int pLeft, pRight, spread;

bool noteOn = false;

MIDI_CREATE_DEFAULT_INSTANCE();

void setup(){
  pinMode(13, OUTPUT);
  //Serial.begin(9600);

  pLeft = 1;
  pRight = 1;
  panDir = 1;
  
  MIDI.setHandleNoteOn(HandleNoteOn); 
  MIDI.setHandleNoteOff(HandleNoteOff); 
  MIDI.begin(MIDI_CHANNEL); 
  
  kModIndex.setFreq(.768f); 
  startMozzi(CONTROL_RATE);

  stereoModulator.setFreq_Q16n16(50000);

  mod_to_carrier_ratio = modRatioMap.next(mozziAnalogRead(0));
  intensity = intensityMap.next(mozziAnalogRead(1));
  spread = panMap.next(mozziAnalogRead(2));

  //HandleNoteOn(1, 30, 127);
}


void HandleNoteOn(byte channel, byte note, byte velocity) { 
  digitalWrite(13, HIGH);
  currentNote = note;
  if (spread != 0) {
    updateNotePan();
  }
  noteOn = true;
}

void HandleNoteOff(byte channel, byte note, byte velocity) { 
  if (note == currentNote) {
    noteOn = false;
    digitalWrite(13, LOW);
  }
}

void setFreqs(Q8n8 midi_note){
  carrier_freq = Q16n16_mtof(Q8n8_to_Q16n16(midi_note)); // convert midi note to fractional frequency
  mod_freq = ((carrier_freq>>8) * mod_to_carrier_ratio)  ; // (Q16n16>>8)   Q8n8 = Q16n16, beware of overflow
  deviation = ((mod_freq>>16) * mod_index); // (Q16n16>>16)   Q8n8 = Q24n8, beware of overflow
  aCarrier.setFreq_Q16n16(carrier_freq);
  aModulator.setFreq_Q16n16(mod_freq);
}

void updateNotePan() {
  if(panDir == 0) {
    pRight = spread;
    pLeft =  256 - spread;
    panDir = 1;
  } else {
    pLeft = spread;
    pRight =  256 - spread;
    panDir = 0;
  }
}

void updateControl(){
  MIDI.read();
  if (step == 0) {
    step = 1;
    mod_to_carrier_ratio = modRatioMap.next(mozziAnalogRead(0));
    intensity = intensityMap.next(mozziAnalogRead(1));
  } else if (step == 1) {
    spread = panMap.next(mozziAnalogRead(2));
    step = 0;
    if (spread == 0) {
      //osc based pan
      pRight = stereoModulator.next() + 128;
      pLeft =  256 - pRight;
    } 
  } 
  
  // vary the modulation index
  mod_index = (Q8n8)350+kModIndex.next();
  setFreqs(Q7n0_to_Q7n8(currentNote));
}

void updateAudio(){
  if (noteOn) {
    if (intensity > 0) {
      long modulation = intensity * aModulator.next();
      val = aCarrier.phMod(modulation);
    } else {
      modulation = deviation * aModulator.next() >> 8;
      val = (int)aCarrier.phMod(modulation);
     }
    audio_out_1 = (val * pRight) >> 8;
    audio_out_2 = (val * pLeft) >> 8;
  } else {
    audio_out_1 = 0;
    audio_out_2 = 0;
  }
}

void loop(){
  audioHook();
}




