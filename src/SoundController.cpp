#include <Arduino.h>
#include "SoundController.hpp"

SoundController::SoundController() {
  beat_time = 0;
}

void SoundController::set_bpm(uint8_t BPM) {
  bpm = BPM;
}

uint8_t SoundController::get_bpm() {
  return bpm;
}

unsigned long SoundController::get_beat_time(){
  return beat_time;
}

bool SoundController::beat_from_bpm(){
    if (bpm == 0) bpm = 1; // guard against division by zero
    unsigned int BREAK_TIME = 60 * 1000 / bpm;
  if (millis() > beat_time + BREAK_TIME) {
    beat_time = millis();
    return true;
  } else {
    return false;
  }
}
