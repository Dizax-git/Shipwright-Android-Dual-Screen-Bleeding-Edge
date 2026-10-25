#include "DualScreenHUD.h"

#include "fast/Fast3dWindow.h"
#include "fast/backends/gfx_rendering_api.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "ship/Context.h"
#include "soh/cvar_prefixes.h"
#include "soh/frame_interpolation.h"

#include "global.h"

#include <memory>

namespace {

constexpr uint32_t kHudWidth = 320;
constexpr uint32_t kHudHeight = 240;

int32_t sHudFramebufferId = -1;
bool sHudPassActive = false;
bool sHudPassPaused = false;
float sSavedAspectRatio = 4.0f / 3.0f;

std::shared_ptr<Fast::Interpreter> GetInterpreter() {
    auto context = Ship::Context::GetRawInstance();
    if (context == nullptr) {
        return nullptr;
    }

    auto window = std::dynamic_pointer_cast<Fast::Fast3dWindow>(context->GetWindow());
    return window == nullptr ? nullptr : window->GetInterpreterWeak().lock();
}

} // namespace

void DualScreenHUD_InitFramebuffer(void) {
#if defined(__ANDROID__)
    if (sHudFramebufferId >= 0) {
        return;
    }

    auto interpreter = GetInterpreter();
    if (interpreter != nullptr) {
        sHudFramebufferId = interpreter->CreateFrameBuffer(kHudWidth, kHudHeight, kHudWidth, kHudHeight, true);
    }
#endif
}

bool DualScreenHUD_BeginHudPass(void) {
#if defined(__ANDROID__)
    if (sHudPassActive) {
        return true;
    }
    if (!DualScreenHUD_IsActive() || gPlayState == nullptr) {
        return false;
    }

    DualScreenHUD_InitFramebuffer();
    auto interpreter = GetInterpreter();
    if (interpreter == nullptr || sHudFramebufferId < 0) {
        return false;
    }

    sSavedAspectRatio = interpreter->mCurDimensions.aspect_ratio;
    interpreter->mCurDimensions.aspect_ratio = static_cast<float>(kHudWidth) / static_cast<float>(kHudHeight);

    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    gsSPSetFB(OVERLAY_DISP++, sHudFramebufferId);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_FILL);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(OVERLAY_DISP++, 0);
    gDPSetScissor(OVERLAY_DISP++, G_SC_NON_INTERLACE, 0, 0, kHudWidth, kHudHeight);
    gDPFillRectangle(OVERLAY_DISP++, 0, 0, kHudWidth - 1, kHudHeight - 1);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    CLOSE_DISPS(gfxCtx);

    sHudPassActive = true;
    sHudPassPaused = false;
    return true;
#else
    return false;
#endif
}

bool DualScreenHUD_PauseHudPass(void) {
#if defined(__ANDROID__)
    if (!sHudPassActive || sHudPassPaused || gPlayState == nullptr) {
        return false;
    }

    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    gsSPResetFB(OVERLAY_DISP++);
    CLOSE_DISPS(gfxCtx);

    auto interpreter = GetInterpreter();
    if (interpreter != nullptr) {
        interpreter->mCurDimensions.aspect_ratio = sSavedAspectRatio;
    }
    sHudPassPaused = true;
    return true;
#else
    return false;
#endif
}

bool DualScreenHUD_ResumeHudPass(void) {
#if defined(__ANDROID__)
    if (!sHudPassActive || !sHudPassPaused || gPlayState == nullptr || sHudFramebufferId < 0) {
        return false;
    }

    auto interpreter = GetInterpreter();
    if (interpreter == nullptr) {
        return false;
    }
    interpreter->mCurDimensions.aspect_ratio = static_cast<float>(kHudWidth) / static_cast<float>(kHudHeight);

    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    gsSPSetFB(OVERLAY_DISP++, sHudFramebufferId);
    CLOSE_DISPS(gfxCtx);

    sHudPassPaused = false;
    return true;
#else
    return false;
#endif
}

void DualScreenHUD_EndHudPass(void) {
#if defined(__ANDROID__)
    if (!sHudPassActive) {
        return;
    }

    if (!sHudPassPaused && gPlayState != nullptr) {
        GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
        OPEN_DISPS(gfxCtx);
        gsSPResetFB(OVERLAY_DISP++);
        CLOSE_DISPS(gfxCtx);
    }

    auto interpreter = GetInterpreter();
    if (interpreter != nullptr) {
        interpreter->mCurDimensions.aspect_ratio = sSavedAspectRatio;
    }
    sHudPassActive = false;
    sHudPassPaused = false;
#endif
}

#if defined(__ANDROID__)

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <mutex>

namespace {

constexpr const char* kLogTag = "SoH-DualScreen";

#define DUAL_LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)
#define DUAL_LOG_WARN(...) __android_log_print(ANDROID_LOG_WARN, kLogTag, __VA_ARGS__)

struct PrimaryBindings {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface drawSurface = EGL_NO_SURFACE;
    EGLSurface readSurface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
};

struct SecondaryState {
    std::mutex mutex;
    ANativeWindow* pendingWindow = nullptr;
    bool pendingDestroy = false;

    ANativeWindow* window = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    bool surfaceRejected = false;

    GLuint shaderProgram = 0;
    GLuint vertexBuffer = 0;
    bool glResourcesReady = false;
};

SecondaryState sSecondary;

constexpr const char* kVertexShader =
    "attribute vec2 aPosition;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "  vTexCoord = aTexCoord;\n"
    "  gl_Position = vec4(aPosition, 0.0, 1.0);\n"
    "}\n";

constexpr const char* kFragmentShader =
    "precision mediump float;\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(uTexture, vTexCoord);\n"
    "}\n";

PrimaryBindings CapturePrimaryContext() {
    return {
        eglGetCurrentDisplay(),
        eglGetCurrentSurface(EGL_DRAW),
        eglGetCurrentSurface(EGL_READ),
        eglGetCurrentContext(),
    };
}

bool RestorePrimaryContext(const PrimaryBindings& primary) {
    if (primary.display == EGL_NO_DISPLAY || primary.context == EGL_NO_CONTEXT) {
        DUAL_LOG_ERROR("Cannot restore missing primary EGL bindings");
        return false;
    }
    if (eglMakeCurrent(primary.display, primary.drawSurface, primary.readSurface, primary.context) != EGL_TRUE) {
        DUAL_LOG_ERROR("eglMakeCurrent failed while restoring primary context: 0x%x", eglGetError());
        return false;
    }
    return true;
}

void DestroyEglResourcesLocked() {
    if (sSecondary.display != EGL_NO_DISPLAY) {
        if (sSecondary.surface != EGL_NO_SURFACE) {
            eglDestroySurface(sSecondary.display, sSecondary.surface);
        }
        if (sSecondary.context != EGL_NO_CONTEXT) {
            eglDestroyContext(sSecondary.display, sSecondary.context);
        }
    }

    sSecondary.display = EGL_NO_DISPLAY;
    sSecondary.context = EGL_NO_CONTEXT;
    sSecondary.surface = EGL_NO_SURFACE;
    sSecondary.shaderProgram = 0;
    sSecondary.vertexBuffer = 0;
    sSecondary.glResourcesReady = false;
}

void DestroySecondarySurfaceLocked() {
    DestroyEglResourcesLocked();
    if (sSecondary.window != nullptr) {
        ANativeWindow_release(sSecondary.window);
        sSecondary.window = nullptr;
    }
    sSecondary.surfaceRejected = false;
}

bool CreateSecondarySurfaceLocked(const PrimaryBindings& primary) {
    if (sSecondary.window == nullptr || primary.display == EGL_NO_DISPLAY || primary.context == EGL_NO_CONTEXT) {
        return false;
    }

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (eglChooseConfig(primary.display, configAttributes, &config, 1, &configCount) != EGL_TRUE ||
        configCount == 0) {
        DUAL_LOG_ERROR("eglChooseConfig failed: 0x%x", eglGetError());
        return false;
    }

    EGLint nativeVisualId = 0;
    if (eglGetConfigAttrib(primary.display, config, EGL_NATIVE_VISUAL_ID, &nativeVisualId) != EGL_TRUE) {
        DUAL_LOG_ERROR("eglGetConfigAttrib(EGL_NATIVE_VISUAL_ID) failed: 0x%x", eglGetError());
        return false;
    }
    if (ANativeWindow_setBuffersGeometry(sSecondary.window, 0, 0, nativeVisualId) != 0) {
        DUAL_LOG_ERROR("ANativeWindow_setBuffersGeometry failed");
        return false;
    }

    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(primary.display, config, primary.context, contextAttributes);
    if (context == EGL_NO_CONTEXT) {
        DUAL_LOG_ERROR("eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }

    EGLSurface surface = eglCreateWindowSurface(primary.display, config, sSecondary.window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        DUAL_LOG_ERROR("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglDestroyContext(primary.display, context);
        return false;
    }

    sSecondary.display = primary.display;
    sSecondary.context = context;
    sSecondary.surface = surface;
    eglSwapInterval(primary.display, 1);
    return true;
}

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        DUAL_LOG_ERROR("glCreateShader failed: 0x%x", glGetError());
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        DUAL_LOG_ERROR("Secondary shader compilation failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool EnsureGlResourcesLocked() {
    if (sSecondary.glResourcesReady) {
        return true;
    }

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    if (vertexShader == 0) {
        return false;
    }
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexCoord");
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        DUAL_LOG_ERROR("Secondary shader link failed: %s", log);
        glDeleteProgram(program);
        return false;
    }

    constexpr GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };
    glGenBuffers(1, &sSecondary.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, sSecondary.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    if (sSecondary.vertexBuffer == 0 || glGetError() != GL_NO_ERROR) {
        DUAL_LOG_ERROR("Secondary vertex-buffer creation failed");
        glDeleteProgram(program);
        sSecondary.vertexBuffer = 0;
        return false;
    }

    sSecondary.shaderProgram = program;
    sSecondary.glResourcesReady = true;
    return true;
}

void RejectCurrentSurfaceLocked(const PrimaryBindings& primary, const char* reason) {
    DUAL_LOG_WARN("Disabling secondary surface after %s", reason);
    RestorePrimaryContext(primary);
    DestroyEglResourcesLocked();
    if (sSecondary.window != nullptr) {
        ANativeWindow_release(sSecondary.window);
        sSecondary.window = nullptr;
    }
    sSecondary.surfaceRejected = true;
}

} // namespace

bool DualScreenHUD_IsActive(void) {
    std::lock_guard<std::mutex> lock(sSecondary.mutex);
    return sSecondary.surface != EGL_NO_SURFACE &&
           CVarGetInteger(CVAR_ENHANCEMENT("DualScreenHUD"), 1) != 0;
}

void DualScreenHUD_PresentFrame(void) {
    std::lock_guard<std::mutex> lock(sSecondary.mutex);
    const PrimaryBindings primary = CapturePrimaryContext();

    if (sSecondary.pendingDestroy) {
        DestroySecondarySurfaceLocked();
        sSecondary.pendingDestroy = false;
    }

    if (sSecondary.pendingWindow != nullptr) {
        ANativeWindow* nextWindow = sSecondary.pendingWindow;
        sSecondary.pendingWindow = nullptr;
        DestroySecondarySurfaceLocked();
        sSecondary.window = nextWindow;
        sSecondary.surfaceRejected = false;
    }

    const bool enabled = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenHUD"), 1) != 0;
    if (!enabled) {
        DestroyEglResourcesLocked();
        return;
    }

    if (sSecondary.surface == EGL_NO_SURFACE && sSecondary.window != nullptr && !sSecondary.surfaceRejected) {
        if (!CreateSecondarySurfaceLocked(primary)) {
            RejectCurrentSurfaceLocked(primary, "EGL initialization failure");
            return;
        }
    }

    if (sSecondary.surface == EGL_NO_SURFACE || sHudFramebufferId < 0) {
        return;
    }

    auto interpreter = GetInterpreter();
    if (interpreter == nullptr) {
        return;
    }
    Fast::GfxRenderingAPI* renderingApi = interpreter->GetCurrentRenderingAPI();
    if (renderingApi == nullptr) {
        return;
    }
    const GLuint hudTexture = static_cast<GLuint>(
        reinterpret_cast<uintptr_t>(renderingApi->GetFramebufferTextureId(sHudFramebufferId)));

    if (eglMakeCurrent(sSecondary.display, sSecondary.surface, sSecondary.surface, sSecondary.context) != EGL_TRUE) {
        RejectCurrentSurfaceLocked(primary, "eglMakeCurrent failure");
        return;
    }

    if (!EnsureGlResourcesLocked()) {
        RejectCurrentSurfaceLocked(primary, "OpenGL resource failure");
        return;
    }

    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    if (eglQuerySurface(sSecondary.display, sSecondary.surface, EGL_WIDTH, &surfaceWidth) != EGL_TRUE ||
        eglQuerySurface(sSecondary.display, sSecondary.surface, EGL_HEIGHT, &surfaceHeight) != EGL_TRUE ||
        surfaceWidth <= 0 || surfaceHeight <= 0) {
        RejectCurrentSurfaceLocked(primary, "invalid surface dimensions");
        return;
    }

    glViewport(0, 0, surfaceWidth, surfaceHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float hudAspect = static_cast<float>(kHudWidth) / static_cast<float>(kHudHeight);
    const float surfaceAspect = static_cast<float>(surfaceWidth) / static_cast<float>(surfaceHeight);
    GLint viewportX = 0;
    GLint viewportY = 0;
    GLsizei viewportWidth = surfaceWidth;
    GLsizei viewportHeight = surfaceHeight;
    if (surfaceAspect > hudAspect) {
        viewportWidth = static_cast<GLsizei>(surfaceHeight * hudAspect);
        viewportX = (surfaceWidth - viewportWidth) / 2;
    } else {
        viewportHeight = static_cast<GLsizei>(surfaceWidth / hudAspect);
        viewportY = (surfaceHeight - viewportHeight) / 2;
    }
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    glUseProgram(sSecondary.shaderProgram);
    glBindBuffer(GL_ARRAY_BUFFER, sSecondary.vertexBuffer);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hudTexture);
    glUniform1i(glGetUniformLocation(sSecondary.shaderProgram, "uTexture"), 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    const EGLBoolean swapSucceeded = eglSwapBuffers(sSecondary.display, sSecondary.surface);
    const bool primaryRestored = RestorePrimaryContext(primary);
    if (swapSucceeded != EGL_TRUE || !primaryRestored) {
        DUAL_LOG_ERROR("Secondary presentation failed (swap=%d restore=%d egl=0x%x)", swapSucceeded,
                       primaryRestored, eglGetError());
        DestroyEglResourcesLocked();
        if (sSecondary.window != nullptr) {
            ANativeWindow_release(sSecondary.window);
            sSecondary.window = nullptr;
        }
        sSecondary.surfaceRejected = true;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_nativeSecondaryDisplaySurfaceChanged(
    JNIEnv* env, jobject, jobject surface, jint, jint) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        DUAL_LOG_WARN("Ignoring a secondary Java surface without an ANativeWindow");
        return;
    }

    std::lock_guard<std::mutex> lock(sSecondary.mutex);
    if (sSecondary.pendingWindow != nullptr) {
        ANativeWindow_release(sSecondary.pendingWindow);
    }
    sSecondary.pendingWindow = window;
    sSecondary.pendingDestroy = false;
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_nativeSecondaryDisplaySurfaceDestroyed(
    JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(sSecondary.mutex);
    if (sSecondary.pendingWindow != nullptr) {
        ANativeWindow_release(sSecondary.pendingWindow);
        sSecondary.pendingWindow = nullptr;
    }
    sSecondary.pendingDestroy = true;
}

#else

bool DualScreenHUD_IsActive(void) {
    return false;
}

void DualScreenHUD_PresentFrame(void) {
}

#endif
