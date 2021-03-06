#include "../include/midi_callback.h"

#include "../include/midi_handler.h"

void toccata::MidiCallback(
    HMIDIIN hMidiIn,
    UINT wMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2) 
{
    if (wMsg == MIM_DATA) {
        DWORD_PTR dwMidiMessage = dwParam1;
        timestamp dwTimestamp = dwParam2;

        WORD hiWord = HIWORD(dwMidiMessage);
        WORD loWord = LOWORD(dwMidiMessage);

        BYTE midiByte1 = HIBYTE(loWord);
        BYTE midiByte2 = LOBYTE(hiWord);
        BYTE midiStatus = (LOBYTE(loWord) & 0xF0) >> 4;

        MidiHandler::Get()->ProcessEvent(midiStatus, midiByte1, midiByte2, dwTimestamp);
    }
}
