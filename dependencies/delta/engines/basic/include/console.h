#ifndef DELTA_BASIC_CONSOLE_H
#define DELTA_BASIC_CONSOLE_H

#include "font.h"
#include "delta_core.h"
#include "shader_controls.h"

#include <string>

namespace dbasic {

    // Class name declarations
    class DeltaEngine;

    struct GuiPoint {
        GuiPoint(int x, int y) : x(x), y(y) {}
        GuiPoint() : x(0), y(0) {}
        ~GuiPoint() {}

        GuiPoint operator+(const GuiPoint &p) {
            return GuiPoint(x + p.x, y + p.y);
        }

        GuiPoint operator-(const GuiPoint &p) {
            return GuiPoint(x - p.x, y - p.y);
        }

        GuiPoint operator+=(const GuiPoint &p) {
            x += p.x;
            y += p.y;

            return *this;
        }

        GuiPoint operator-=(const GuiPoint &p) {
            x -= p.x;
            y -= p.y;

            return *this;
        }

        void Clear() {
            x = 0;
            y = 0;
        }

        int x;
        int y;
    };

    class Console : public ysObject {
    public:
        static const int BufferWidth = 175;
        static const int BufferHeight = 75;
        static const int BufferSize = BufferWidth * BufferHeight; // Maximum of 4096 characters displayed at once

    public:
        Console();
        ~Console();

        ysError Initialize();
        ysError ResetScreenPosition();
        ysError Destroy();

        ysError UpdateDisplay();

        void SetEngine(DeltaEngine *engine) { m_engine = engine; }
        DeltaEngine *GetEngine() { return m_engine; }

        void SetDefaultFontDirectory(const std::string &s) { m_defaultFontDirectory = s; }
        std::string GetDefaultFontDirectory() const { return m_defaultFontDirectory; }

    protected:
        DeltaEngine *m_engine;

        ysGPUBuffer *m_mainVertexBuffer;
        ysGPUBuffer *m_mainIndexBuffer;

        // Textures
        ysTexture *m_fontTexture;

        ysShader *m_vertexShader;
        ysShader *m_pixelShader;
        ysShaderProgram *m_shaderProgram;

        ysRenderGeometryFormat m_standardFormat;
        ysInputLayout *m_standardInputLayout;

        // Vertex buffer
        ConsoleVertex *m_vertexData;

        Font *m_font;

    protected:
        // Settings
        std::string m_defaultFontDirectory;

    protected:
        ysError InitializeGeometry();

    protected:
        // Window metrics
        int m_bufferWidth;
        int m_bufferHeight;

    protected:
        // ----------------------------------------------------
        // GUI Drawing Tools
        //
        // SYSTEM
        //
        // ----------------------------------------------------

        void RealignLocation() { m_actualLocation = m_nominalLocation; }

        GuiPoint m_nominalLocation;
        GuiPoint m_actualLocation;

        char *m_buffer;

    public:
        // ----------------------------------------------------
        // GUI Drawing Tools
        //
        // INTERFACE
        //
        // ----------------------------------------------------

        // Drawing Text
        ysError SetCharacter(char character);

        void Clear();

        void OutputChar(unsigned char c, int n = 1);
        void DrawGeneralText(const char *text, int maxLength = -1);
        void DrawBoundText(const char *text, int width, int height, int xOffset, int yOffset);
        void DrawWrappedText(const char *text, int width);

        // Drawing Shapes
        void DrawLineRectangle(int width, int height);
        void DrawHorizontalLine(int length);
        void DrawVerticalLine(int length);

        // Utilities
        static int FindEndOfNextWord(const char *text, int location);
        static bool IsWhiteSpace(char c);

        // Navigation
        void MoveDownLine(int n = 1);
        void MoveToLocation(const GuiPoint &location);
        void MoveToOrigin();
    };

} /* namespace dbasic */

#endif /* DELTA_BASIC_CONSOLE_H */
