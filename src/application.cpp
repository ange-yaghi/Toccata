#include "../include/application.h"

#include "../include/midi_stream.h"
#include "../include/midi_handler.h"
#include "../include/midi_file.h"
#include "../include/segment_generator.h"
#include "../include/transform.h"
#include "../include/midi_callback.h"
#include "../include/theme_script_compiler.h"
#include "../include/settings_manager.h"
#include "../include/grid.h"

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

    m_engine.InitializeShaderSet(&m_shaderSet);
    m_engine.InitializeDefaultShaders(&m_shaders, &m_shaderSet);
    m_engine.InitializeConsoleShaders(&m_shaderSet);
    m_engine.SetShaderSet(&m_shaderSet);

    m_shaders.SetCameraMode(dbasic::DefaultShaders::CameraMode::Flat);
    m_shaderSet.GetStage(0)->GetRenderTarget()->SetDepthTestEnabled(false);
    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x00, 0x00, 0x00));

    m_assetManager.SetEngine(&m_engine);

    m_analyzer.SetTimeline(&m_timeline);

    m_barDisplay.Initialize(&m_engine, &m_shaders, &m_textRenderer, &m_settings);
    m_barDisplay.SetTextRenderer(&m_textRenderer);
    m_barDisplay.SetAnalyzer(&m_analyzer);

    m_midiDisplay.SetAnalyzer(&m_analyzer);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_engine.GetConsole()->GetFont());

    m_testSegment.PulseUnit = 1000.0;
    m_testSegment.PulseRate = 1.0;

    m_unresolvedNotes.PulseUnit = 1000.0;
    m_unresolvedNotes.PulseRate = 1.0;

    m_practiceModePanel.Initialize(&m_engine, &m_shaders, &m_textRenderer, &m_settings);
    m_currentTimeDisplay.Initialize(&m_engine, &m_shaders, &m_textRenderer, &m_settings);
    m_metricsPanel.Initialize(&m_engine, &m_shaders, &m_textRenderer, &m_settings);

    m_metricsPanel.SetDecisionThread(&m_decisionThread);

    ReloadThemes();

    m_isKeydown = new bool[(int)ysKey::Code::Count];
    for (int i = 0; i < (int)ysKey::Code::Count; ++i) m_isKeydown[i] = false;
}

void toccata::Application::Process() {
    const int windowWidth = m_engine.GetScreenWidth();
    const int windowHeight = m_engine.GetScreenHeight();

    m_shaders.SetScreenDimensions(windowWidth, windowHeight);
    m_shaders.CalculateUiCamera();
    m_shaders.SetFogNear(10000.0f);
    m_shaders.SetFogFar(10001.0f);

    const float dt = m_engine.GetFrameLength();
    m_currentOffset += dt;

    const int n = m_testSegment.NoteContainer.GetCount();
    timestamp windowStart = m_timeline.GetTimeOffset();
    if (n > 0) {
        const MusicPoint &lastPoint = m_testSegment.NoteContainer.GetPoints()[n - 1];
        timestamp lastTimestamp = lastPoint.Timestamp;

        if (lastTimestamp + 2000 > m_timeline.GetTimeRange() + m_timeline.GetTimeOffset()) {
            windowStart = lastTimestamp + 2000 - 5000;
        }
    }

    m_timeline.SetPositionX(0.0f);
    m_timeline.SetInputSegment(&m_testSegment);
    m_timeline.SetUnfinishedSegment(&m_unresolvedNotes);
    m_timeline.SetTimeOffset(windowStart);
    m_timeline.SetTimeRange(5000);
    m_timeline.SetWidth((float)windowWidth);

    ConstructReferenceNotes();
    m_analyzer.Analyze();

    m_midiDisplay.SetEngine(&m_engine);
    m_midiDisplay.SetShaders(&m_shaders);
    m_midiDisplay.SetTextRenderer(&m_textRenderer);
    m_midiDisplay.SetHeight(windowHeight * 0.7f);
    m_midiDisplay.SetKeyRangeStart(0);
    m_midiDisplay.SetKeyRangeEnd(88);
    m_midiDisplay.SetPositionY(windowHeight - windowHeight * 0.2f);
    m_midiDisplay.SetTimeline(&m_timeline);
    m_midiDisplay.SetSettings(&m_settings);

    m_barDisplay.SetEngine(&m_engine);
    m_barDisplay.SetShaders(&m_shaders);
    m_barDisplay.SetHeight(windowHeight * 0.1f);
    m_barDisplay.SetPositionY(windowHeight - windowHeight * 0.1f);
    m_barDisplay.SetTextRenderer(&m_textRenderer);
    m_barDisplay.SetTimeline(&m_timeline);
    m_barDisplay.SetSettings(&m_settings);

    m_pieceDisplay.SetEngine(&m_engine);
    m_pieceDisplay.SetShaders(&m_shaders);
    m_pieceDisplay.SetHeight(windowHeight * 0.1f);
    m_pieceDisplay.SetPositionY((float)windowHeight);
    m_pieceDisplay.SetTextRenderer(&m_textRenderer);
    m_pieceDisplay.SetTimeline(&m_timeline);
    m_pieceDisplay.SetSettings(&m_settings);

    Grid lowerGrid(
        BoundingBox(windowWidth, windowHeight * 0.1f).AlignLeft(0.0f).AlignBottom(0.0f),
        3, 1, 0.0f);

    m_currentTimeDisplay.SetBoundingBox(lowerGrid.GetCell(2, 0));
    m_practiceModePanel.SetBoundingBox(lowerGrid.GetCell(0, 0));
    m_metricsPanel.SetBoundingBox(lowerGrid.GetCell(1, 0));

    MockMidiInput();

    if (m_engine.ProcessKeyDown(ysKey::Code::F5)) {
        ReloadThemes();
    }

    m_metricsPanel.ProcessAll();
    m_practiceModePanel.ProcessAll();
    m_currentTimeDisplay.ProcessAll();

    m_midiDisplay.SetPracticeMode(m_practiceModePanel.GetPracticeMode());
    m_midiDisplay.SetTimingErrorMin(
        m_practiceModePanel.GetTimingPracticeControls().GetMinError());
    m_midiDisplay.SetTimingErrorMax(
        m_practiceModePanel.GetTimingPracticeControls().GetMaxError());
    m_midiDisplay.SetVelocityErrorThreshold(
        m_practiceModePanel.GetVelocityPracticeControls().GetThreshold());
    m_midiDisplay.SetTargetVelocity(
        m_practiceModePanel.GetVelocityPracticeControls().GetTarget());
}

void toccata::Application::Render() {
    m_midiDisplay.Render();
    m_barDisplay.Render();
    m_pieceDisplay.Render();
    m_practiceModePanel.RenderAll();
    m_currentTimeDisplay.RenderAll();
    m_metricsPanel.RenderAll();

    std::stringstream ss; 
    ss << "TOCCATA" << "\n";
    ss << m_engine.GetAverageFramerate() << "\n";

    m_engine.GetConsole()->MoveToOrigin();
    m_engine.GetConsole()->DrawGeneralText(ss.str().c_str());
}

void toccata::Application::MockMidiInput() {
    MockMidiKey(ysKey::Code::A, 48);
    MockMidiKey(ysKey::Code::S, 50);
    MockMidiKey(ysKey::Code::D, 52);
    MockMidiKey(ysKey::Code::F, 53);
    MockMidiKey(ysKey::Code::G, 55);
    MockMidiKey(ysKey::Code::H, 57);
}

void toccata::Application::MockMidiKey(ysKey::Code key, int midiKey) {
    // Add 100 million years just for fun
    const timestamp t = 
        (timestamp)std::round(m_currentOffset * 1000) + 3153600000000000000;

    if (m_engine.IsKeyDown(key) && !m_isKeydown[(int)key]) {
        m_isKeydown[(int)key] = true;
        MidiHandler::Get()->ProcessEvent(0x9, midiKey, 100, t);
    }
    else if (!m_engine.IsKeyDown(key) && m_isKeydown[(int)key]) {
        m_isKeydown[(int)key] = false;
        MidiHandler::Get()->ProcessEvent(0x8, midiKey, 0, t);
    }
}

void toccata::Application::ProcessMidiInput() {
    MidiStream targetStream;
    MidiStream unresolvedStream;
    MidiHandler::Get()->Extract(&targetStream, &unresolvedStream);

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

    const int unresolvedNodeCount = unresolvedStream.GetNoteCount();
    m_unresolvedNotes.NoteContainer.Clear();
    for (int i = 0; i < unresolvedNodeCount; ++i) {
        const MidiNote &note = unresolvedStream.GetNote(i);
        MusicPoint point;
        point.Pitch = note.MidiKey;
        point.Timestamp = note.Timestamp;
        point.Velocity = note.Velocity;
        point.Length = note.NoteLength;
        m_unresolvedNotes.NoteContainer.AddPoint(point);
    }
}

void toccata::Application::CheckMidiStatus() {
    const bool connected = m_midiSystem.IsConnected();
    const bool status = m_midiSystem.Refresh();
    if (!status) {
        if (m_midiSystem.GetLastErrorCode() == MidiDeviceSystem::ErrorCode::DeviceListChangedDuringUpdate) {
            // TODO
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
}

void toccata::Application::ReloadThemes() {
    ThemeScriptCompiler compiler;
    compiler.Initialize();
    compiler.Compile(piranha::IrPath("../../themes/default.mr"));
    compiler.Execute();
    compiler.Destroy();

    m_settings.Fill(nullptr, SettingsManager::Get()->GetProfile("default"));
}

void toccata::Application::Run() {
    InitializeMidiInput();
    InitializeLibrary();
    InitializeDecisionThread();

    while (m_engine.IsOpen()) {
        m_engine.StartFrame();

        CheckMidiStatus();

        Process();
        Render();

        m_engine.EndFrame();
    }

    m_decisionThread.KillThreads();
    m_decisionThread.Destroy();
}

void toccata::Application::Destroy() {
    m_shaderSet.Destroy();
    m_assetManager.Destroy();
    m_engine.Destroy();
}

void toccata::Application::InitializeLibrary() {
    static const std::string paths[] = 
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
    m_decisionThread.Initialize(&m_library, 1, 1000.0, 1.0);
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
