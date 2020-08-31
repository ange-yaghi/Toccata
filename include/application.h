#ifndef TOCCATA_UI_APPLICATION_H
#define TOCCATA_UI_APPLICATION_H

#include "midi_display.h"

#include "music_segment.h"
#include "midi_device_system.h"
#include "decision_thread.h"

#include "delta.h"

namespace toccata {

    class Application {
    public:
        Application();
        ~Application();

        void Initialize(void *instance, ysContextObject::DeviceAPI api);
        void Run();
        void Destroy();

    protected:
        void Process();
        void Render();

        void InitializeLibrary();
        void InitializeDecisionThread();
        void InitializeMidiInput();
        void ProcessMidiInput();
        void CheckMidiStatus();
        void ConstructReferenceNotes();

        dbasic::DeltaEngine m_engine;
        dbasic::AssetManager m_assetManager;

        MidiDisplay m_midiDisplay;
        MusicSegment m_testSegment;
        MusicSegment m_referenceSegment;

        MidiDeviceSystem m_midiSystem;

        Library m_library;
        DecisionThread m_decisionThread;
    };

} /* namespace toccata */

#endif /* TOCCATA_UI_APPLICATION_H */
