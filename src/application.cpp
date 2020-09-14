#include "../include/application.h"

#include "../include/midi_stream.h"
#include "../include/midi_handler.h"
#include "../include/midi_file.h"
#include "../include/segment_generator.h"
#include "../include/transform.h"
#include "../include/midi_callback.h"
#include "../include/theme_script_compiler.h"
#include "../include/settings_manager.h"

#include <sstream>

toccata::Application::Application() {
    m_currentOffset = 0.0;
}

toccata::Application::~Application() {
    /* void */
}

void toccata::Application::Initialize(void *instance, ysContextObject::DeviceAPI api) {
    const dbasic::Path modulePath = dbasic::GetModulePath();
    const dbasic::Path confPath = modulePath.Append("delta.conf");

    std::string enginePath = "../../dependencies/delta/engines/basic";
    std::string assetPath = "../../assets";
    if (confPath.Exists()) {
        std::fstream confFile(confPath.ToString(), std::ios::in);
        
        std::getline(confFile, enginePath);
        std::getline(confFile, assetPath);
        enginePath = modulePath.Append(enginePath).ToString();
        assetPath = modulePath.Append(assetPath).ToString();

        confFile.close();
    }

    m_engine.GetConsole()->SetDefaultFontDirectory(enginePath + "/fonts/");

    const std::string shaderDirectory = enginePath + "/shaders/";

    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = true;
    settings.FrameLogging = false;
    settings.Instance = instance;
    settings.LoggingDirectory = "";
    settings.ShaderDirectory = shaderDirectory.c_str();
    settings.WindowTitle = "Toccata";

    m_engine.CreateGameWindow(settings);
    m_engine.SetCameraMode(dbasic::DeltaEngine::CameraMode::Target);
    m_engine.SetClearColor(ysColor::srgbiToLinear(0x00, 0x00, 0x00));

    m_assetManager.SetEngine(&m_engine);

    m_analyzer.SetTimeline(&m_timeline);

    m_barDisplay.Initialize(&m_engine);
    m_barDisplay.SetTextRenderer(&m_textRenderer);
    m_barDisplay.SetAnalyzer(&m_analyzer);

    m_midiDisplay.SetAnalyzer(&m_analyzer);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_engine.GetConsole()->GetFont());

    m_testSegment.PulseUnit = 1000.0;
    m_testSegment.PulseRate = 1.0;

    m_timingToggle.Initialize(&m_engine, &m_textRenderer, &m_settings);
    m_timingToggle.SetPosition({ 10, 40 });
    m_timingToggle.SetSize({ 30, 30 });
    m_timingToggle.SetText("T");

    m_velocityToggle.Initialize(&m_engine, &m_textRenderer, &m_settings);
    m_velocityToggle.SetPosition({ 50, 40 });
    m_velocityToggle.SetSize({ 30, 30 });
    m_velocityToggle.SetText("V");

    m_practiceModeGroup.AddToggle(&m_timingToggle);
    m_practiceModeGroup.AddToggle(&m_velocityToggle);

    m_numericInput.Initialize(&m_engine, &m_textRenderer, &m_settings);

    ReloadThemes();
}

void toccata::Application::Process() {
    const int windowWidth = m_engine.GetScreenWidth();
    const int windowHeight = m_engine.GetScreenHeight();

    const float dt = m_engine.GetFrameLength();
    m_currentOffset += dt;

    const int n = m_testSegment.NoteContainer.GetCount();
    timestamp windowStart = m_timeline.GetTimeOffset();
    if (n > 0) {
        const MusicPoint &lastPoint = m_testSegment.NoteContainer.GetPoints()[n - 1];

        if (lastPoint.Timestamp + 2000 > m_timeline.GetTimeRange() + m_timeline.GetTimeOffset()) {
            windowStart = lastPoint.Timestamp + 2000 - 5000;
        }
    }

    m_timeline.SetPositionX(-windowWidth / 2.0f);
    m_timeline.SetInputSegment(&m_testSegment);
    m_timeline.SetTimeOffset(windowStart);
    m_timeline.SetTimeRange(5000);
    m_timeline.SetWidth((float)windowWidth);

    m_midiDisplay.SetEngine(&m_engine);
    m_midiDisplay.SetTextRenderer(&m_textRenderer);
    m_midiDisplay.SetHeight(windowHeight * 0.7f);
    m_midiDisplay.SetKeyRangeStart(0);
    m_midiDisplay.SetKeyRangeEnd(88);
    m_midiDisplay.SetPositionY(windowHeight / 2.0f - windowHeight * 0.2f);
    m_midiDisplay.SetTimeline(&m_timeline);
    m_midiDisplay.SetSettings(&m_settings);

    m_barDisplay.SetEngine(&m_engine);
    m_barDisplay.SetHeight(windowHeight * 0.1f);
    m_barDisplay.SetPositionY(windowHeight / 2.0f - windowHeight * 0.1f);
    m_barDisplay.SetTextRenderer(&m_textRenderer);
    m_barDisplay.SetTimeline(&m_timeline);
    m_barDisplay.SetSettings(&m_settings);

    m_pieceDisplay.SetEngine(&m_engine);
    m_pieceDisplay.SetHeight(windowHeight * 0.1f);
    m_pieceDisplay.SetPositionY(windowHeight / 2.0f);
    m_pieceDisplay.SetTextRenderer(&m_textRenderer);
    m_pieceDisplay.SetTimeline(&m_timeline);
    m_pieceDisplay.SetSettings(&m_settings);

    MockMidiInput();

    if (m_engine.ProcessKeyDown(ysKeyboard::KEY_F5)) {
        ReloadThemes();
    }

    m_velocityToggle.Process();
    m_timingToggle.Process();
    m_numericInput.Process();

    if (m_velocityToggle.GetChecked()) {
        m_midiDisplay.SetPracticeMode(MidiDisplay::PracticeMode::Velocity);
    }
    else if (m_timingToggle.GetChecked()) {
        m_midiDisplay.SetPracticeMode(MidiDisplay::PracticeMode::Timing);
    }
    else {
        m_midiDisplay.SetPracticeMode(MidiDisplay::PracticeMode::Default);
    }
}

void toccata::Application::Render() {
    m_midiDisplay.Render();
    m_barDisplay.Render();
    m_pieceDisplay.Render();
    m_velocityToggle.Render();
    m_timingToggle.Render();
    m_numericInput.Render();

    std::stringstream ss; 
    ss << "TOCCATA" << "\n";
    ss << m_engine.GetAverageFramerate() << "\n";

    m_engine.GetConsole()->MoveToOrigin();
    m_engine.GetConsole()->DrawGeneralText(ss.str().c_str());
}

void toccata::Application::MockMidiInput() {
    MockMidiKey(ysKeyboard::KEY_A, 48);
    MockMidiKey(ysKeyboard::KEY_S, 50);
    MockMidiKey(ysKeyboard::KEY_D, 52);
    MockMidiKey(ysKeyboard::KEY_F, 53);
    MockMidiKey(ysKeyboard::KEY_G, 55);
    MockMidiKey(ysKeyboard::KEY_H, 57);
}

void toccata::Application::MockMidiKey(ysKeyboard::KEY_CODE key, int midiKey) {
    // Add 100 million years just for fun
    const timestamp t = 
        (timestamp)std::round(m_currentOffset * 1000) + 3153600000000000000;

    if (m_engine.ProcessKeyDown(key)) {
        MidiHandler::Get()->ProcessEvent(0x9, midiKey, 100, t);
    }
    else if (m_engine.ProcessKeyUp(key)) {
        MidiHandler::Get()->ProcessEvent(0x8, midiKey, 0, t);
    }
}

void toccata::Application::ProcessMidiInput() {
    MidiStream targetStream;
    MidiHandler::Get()->Extract(&targetStream);

    const int noteCount = targetStream.GetNoteCount();
    for (int i = 0; i < noteCount; ++i) {
        const MidiNote &note = targetStream.GetNote(i);
        MusicPoint point;
        point.Pitch = note.MidiKey;
        point.Timestamp = note.Timestamp;
        point.Velocity = note.Velocity;
        point.Length = note.NoteLength;
        m_decisionThread.AddNote(point);
        m_testSegment.NoteContainer.AddPoint(point);
    }
}

void toccata::Application::CheckMidiStatus() {
    const bool connected = m_midiSystem.IsConnected();
    const bool status = m_midiSystem.Refresh();
    if (!status) {
        if (m_midiSystem.GetLastErrorCode() == MidiDeviceSystem::ErrorCode::DeviceListChangedDuringUpdate) {

        }
        else {
            // TODO
        }
    }

    if (!connected) {
        const bool reconnected = m_midiSystem.Reconnect();
        if (reconnected) {
            toccata::MidiHandler::Get()->AlignTimestampOffset();
        }
    }

    ProcessMidiInput();
}

void toccata::Application::ConstructReferenceNotes() {
    m_referenceSegment.NoteContainer.Clear();

    m_timeline.ClearBars();
    m_timeline.ClearPieces();

    auto pieces = m_decisionThread.GetPieces();
    for (const DecisionTree::MatchedPiece &piece : pieces) {
        m_timeline.AddPiece(*piece.Bars.begin(), *piece.Bars.rbegin());

        for (const DecisionTree::MatchedBar &bar : piece.Bars) {
            m_timeline.AddBar(bar);
        }
    }

    m_analyzer.Analyze();
}

void toccata::Application::ReloadThemes() {
    ThemeScriptCompiler compiler;
    compiler.Initialize();
    compiler.Compile(piranha::IrPath("../../themes/default.mr"));
    compiler.Execute();
    //compiler.Destroy();

    m_settings.Fill(nullptr, SettingsManager::Get()->GetProfile("default"));
}

void toccata::Application::Run() {
    InitializeMidiInput();
    InitializeLibrary();
    InitializeDecisionThread();

    while (m_engine.IsOpen()) {
        m_engine.StartFrame();

        CheckMidiStatus();
        ConstructReferenceNotes();

        Process();
        Render();

        m_engine.EndFrame();
    }

    m_decisionThread.KillThreads();
    m_decisionThread.Destroy();
}

void toccata::Application::Destroy() {
    m_assetManager.Destroy();
    m_engine.Destroy();
}

void toccata::Application::InitializeLibrary() {
    const std::string paths[] = 
    {
        "../../test/midi/simple_passage.midi",
        "../../test/midi/simple_passage_2.mid"
    };

    for (const std::string &path : paths) {
        toccata::MidiStream stream;
        toccata::MidiFile midiFile;
        midiFile.Read(path.c_str(), &stream);

        std::string name = dbasic::Path(path).GetStem();

        toccata::SegmentGenerator::Convert(&stream, &m_library, name, 0);
    }
}

void toccata::Application::InitializeDecisionThread() {
    m_decisionThread.Initialize(&m_library, 12, 1000.0, 1.0);
    m_decisionThread.StartThreads();
}

void toccata::Application::InitializeMidiInput() {
    m_midiSystem.Refresh();

    const bool connectSuccess = m_midiSystem.Connect(1);

    if (connectSuccess) {
        // TODO
    }
    else {
        // TODO
    }
}
