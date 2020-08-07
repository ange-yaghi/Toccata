#include "../include/application.h"

toccata::Application::Application() {
    m_demoTexture = nullptr;
    m_currentRotation = 0.0f;
    m_temperature = 0.0f;
}

toccata::Application::~Application() {
    /* void */
}

void toccata::Application::Initialize(void *instance, ysContextObject::DeviceAPI api) {
    dbasic::Path modulePath = dbasic::GetModulePath();
    dbasic::Path confPath = modulePath.Append("delta.conf");

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

    std::string shaderDirectory = enginePath + "/shaders/";

    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = true;
    settings.FrameLogging = false;
    settings.Instance = instance;
    settings.LoggingDirectory = "";
    settings.ShaderDirectory = shaderDirectory.c_str();
    settings.WindowTitle = "Toccata";

    m_engine.CreateGameWindow(settings);

    m_engine.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));

    m_assetManager.SetEngine(&m_engine);

    m_assetManager.LoadTexture((assetPath + "/chicken.png").c_str(), "Chicken");
    m_demoTexture = m_assetManager.GetTexture("Chicken")->GetTexture();

    m_assetManager.CompileInterchangeFile((assetPath + "/icosphere").c_str(), 1.0f, true);
    m_assetManager.LoadSceneFile((assetPath + "/icosphere").c_str(), true);

    m_engine.SetCameraMode(dbasic::DeltaEngine::CameraMode::Target);
}

void toccata::Application::Process() {
    if (m_engine.IsKeyDown(ysKeyboard::KEY_SPACE)) {
        m_currentRotation += m_engine.GetFrameLength();
    }

    if (m_engine.IsKeyDown(ysKeyboard::KEY_UP)) {
        m_temperature += m_engine.GetFrameLength() * 0.5f;
    }
    else if (m_engine.IsKeyDown(ysKeyboard::KEY_DOWN)) {
        m_temperature -= m_engine.GetFrameLength() * 0.5f;
    }

    if (m_temperature < 0.0f) m_temperature = 0.0f;
    if (m_temperature > 1.0f) m_temperature = 1.0f;
}

void toccata::Application::Render() {
    m_engine.SetCameraPosition(ysMath::LoadVector(4.0f, 4.0f, 2.0f));
    m_engine.SetCameraUp(ysMath::Constants::ZAxis);

    m_engine.ResetLights();
    m_engine.SetAmbientLight(ysMath::GetVector4(ysColor::srgbiToLinear(0x34, 0x98, 0xdb)));

    dbasic::Light light;
    light.Active = 1;
    light.Attenuation0 = 0.0f;
    light.Attenuation1 = 0.0f;
    light.Color = ysVector4(0.85f, 0.85f, 0.8f, 1.0f);
    light.Direction = ysVector4(0.0f, 0.0f, 0.0f, 0.0f);
    light.FalloffEnabled = 0;
    light.Position = ysVector4(10.0f, 10.0f, 10.0f);
    m_engine.AddLight(light);

    dbasic::Light light2;
    light2.Active = 1;
    light2.Attenuation0 = 0.0f;
    light2.Attenuation1 = 0.0f;
    light2.Color = ysVector4(0.3f, 0.3f, 0.5f, 1.0f);
    light2.Direction = ysVector4(0.0f, 0.0f, 0.0f, 0.0f);
    light2.FalloffEnabled = 0;
    light2.Position = ysVector4(-10.0f, 10.0f, 10.0f);
    m_engine.AddLight(light2);

    dbasic::Light glow;
    glow.Active = 1;
    glow.Attenuation0 = 0.0f;
    glow.Attenuation1 = 0.0f;
    glow.Color = ysVector4(5.0f * m_temperature, 0.0f, 0.0f, 1.0f);
    glow.Direction = ysVector4(0.0f, 0.0f, 0.0f, 0.0f);
    glow.FalloffEnabled = 1;
    glow.Position = ysVector4(0.0f, 0.0f, 0.0f);
    m_engine.AddLight(glow);

    ysMatrix rotationTurntable = ysMath::RotationTransform(ysMath::Constants::ZAxis, m_currentRotation); 

    m_engine.ResetBrdfParameters();
    m_engine.SetMetallic(0.8f); 
    m_engine.SetIncidentSpecular(0.8f);
    m_engine.SetSpecularRoughness(0.7f);
    m_engine.SetSpecularMix(1.0f);
    m_engine.SetDiffuseMix(1.0f); 
    m_engine.SetEmission(ysMath::Mul(ysColor::srgbiToLinear(0xff, 0x0, 0x0), ysMath::LoadScalar(m_temperature))); 
    m_engine.SetBaseColor(ysColor::srgbiToLinear(0x34, 0x49, 0x5e));
    m_engine.SetObjectTransform(ysMath::MatMult(ysMath::TranslationTransform(ysMath::LoadVector(0.0f, 0.0f, 0.0f)), rotationTurntable));
    m_engine.DrawModel(m_assetManager.GetModelAsset("Icosphere"), 1.0f, nullptr);

    m_engine.ResetBrdfParameters();
    m_engine.SetMetallic(0.0f);
    m_engine.SetIncidentSpecular(0.0f);
    m_engine.SetSpecularRoughness(0.8f);
    m_engine.SetSpecularMix(0.1f);
    m_engine.SetDiffuseMix(1.0f);
    m_engine.SetBaseColor(ysColor::srgbiToLinear(0xbd, 0xc3, 0xc7));
    m_engine.SetObjectTransform(ysMath::MatMult(ysMath::TranslationTransform(ysMath::LoadVector(0.0f, 0.0f, 0.0f)), rotationTurntable));
    m_engine.SetObjectTransform(ysMath::MatMult(ysMath::TranslationTransform(ysMath::LoadVector(0.0f, 0.0f, -1.0f)), rotationTurntable));
    m_engine.DrawModel(m_assetManager.GetModelAsset("Floor"), 1.0f, nullptr);
}

void toccata::Application::Run() {
    while (m_engine.IsOpen()) {
        m_engine.StartFrame();

        Process();
        Render();

        m_engine.EndFrame();
    }
}

void toccata::Application::Destroy() {
    m_assetManager.Destroy();
    m_engine.Destroy();
}
