#ifndef SOUND_H
#define SOUND_H

class SoundController
{
    private:
        uint8_t bpm = 1;
        unsigned long beat_time;
    public:
        SoundController();
        void set_bpm(uint8_t BPM);
        uint8_t get_bpm();
        bool beat_from_bpm();
        unsigned long get_beat_time();
};

#endif
