#pragma once
#include <rack.hpp>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <dsp/digital.hpp>

using namespace rack;

struct ProcessingParams {
    float brightness{1.0f};
    float contrast{1.0f};
    float saturation{1.0f};
    float hueShift{0.0f};
    float sharpness{0.0f};
    float pixelation{0.0f};
    float edgeDetect{0.0f};
    float rgbAberration{0.0f};
    float noise{0.0f};
    float glitchSlice{0.0f};
    bool mirrorEffect{false};
    bool flipEffect{false};
    bool ditherEffect{false};
    float ditherIntensity{0.2f};
    bool interlaceEffect{false};
    float interlaceIntensity{0.5f}; // Nuevo parámetro de intensidad
    bool invertColors{false};
    bool halfMirrorEffect{false};
    bool halfMirrorVerticalEffect{false};
    float posterize{0.0f};
    float glitchArtifacts{0.0f};
    float glitchBlockSize{0.0f};
    float glitchDisplacement{0.0f};
    float bitCrush{0.0f};
    float dataShift{0.0f};
    float pixelSort{0.0f};
};

struct GIFGlitcher : Module {
    // Estructura para frames GIF
    struct GifFrame {
        std::vector<unsigned char> data;
        int delay;  // en milisegundos
        int imageHandle{0};  // Añadir handle para cada frame
    };

    enum ParamIds {
        BRIGHTNESS_PARAM,
        CONTRAST_PARAM,
        SATURATION_PARAM,
        HUE_SHIFT_PARAM,
        SHARPNESS_PARAM,
        PIXELATION_PARAM,
        EDGE_DETECT_PARAM,
        RGB_ABERRATION_PARAM,
        NOISE_PARAM,
        GLITCH_SLICE_PARAM,
        POSTERIZE_PARAM,  
        DITHER_INTENSITY_PARAM,
        INTERLACE_INTENSITY_PARAM, 
        GLITCH_ARTIFACTS_INTENSITY_PARAM, 
        GLITCH_BLOCK_SIZE_PARAM,
        GLITCH_DISPLACEMENT_PARAM, // Nuevo parámetro
        BIT_CRUSH_PARAM,
        DATA_SHIFT_PARAM,
        PIXEL_SORT_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        BRIGHTNESS_INPUT,
        CONTRAST_INPUT,
        SATURATION_INPUT,
        HUE_SHIFT_INPUT,
        SHARPNESS_INPUT,
        PIXELATION_INPUT,
        EDGE_DETECT_INPUT,
        RGB_ABERRATION_INPUT,
        MIRROR_INPUT,
        FLIP_INPUT,
        INVERT_INPUT,
        DITHER_INPUT,
        INTERLACE_INPUT,    // Nuevo input
        NOISE_INPUT,
        GLITCH_SLICE_INPUT,
        POSTERIZE_INPUT,
        HALF_MIRROR_INPUT,  // Nuevo input
        HALF_MIRROR_VERTICAL_INPUT,  // Nuevo input
        GLITCH_ARTIFACTS_INPUT,  // Nuevo input
        DATA_MOSH_INPUT,
        RESET_INPUT,
        RANDOM_INPUT,   // Nuevo input
        NUM_INPUTS
    };
    enum OutputIds {
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    dsp::SchmittTrigger randomTrigger;
    dsp::SchmittTrigger resetTrigger;

    // Variables para GIF
    std::vector<GifFrame> gifFrames;
    size_t currentFrame{0};
    float frameAccumulator{0.0f};
    bool isAnimated{false};
    std::mutex bufferMutex;
    bool textureNeedsUpdate{false};

    // Variables existentes
    NVGcontext* vg{nullptr};
    int imageHandle{0};
    int outputImageHandle{0};
    int imageWidth{0};
    int imageHeight{0};
    std::string imagePath;
    std::vector<unsigned char> imageData;
    std::vector<unsigned char> processedData;
    
    // Thread-related members
    std::atomic<bool> threadRunning{false};
    std::atomic<bool> processRequested{false};
    std::thread workerThread;
    std::mutex paramsMutex;
    std::condition_variable processCV;

    // Métodos públicos
    GIFGlitcher();
    ~GIFGlitcher() override;
    void process(const ProcessArgs& args) override;
    bool loadGif(const std::string& path);
    void onReset() override;
    
    // Getters y setters existentes
    int getOutputImageHandle() const { return outputImageHandle; }
    int getImageWidth() const { return imageWidth; }
    int getImageHeight() const { return imageHeight; }
    NVGcontext* getVG() const { return vg; }
    bool isImageLoaded() const { return !imagePath.empty(); }
    const unsigned char* getProcessedDataPtr() const { return processedData.data(); }
    const unsigned char* getImageDataPtr() const { return imageData.data(); }

    void setVG(NVGcontext* _vg);

    float accumulatedTime{0.0f};
    ProcessingParams currentParams;

    void loadImage(std::string path);
    void reloadImage();

    // Agregar control de velocidad
    enum PlaybackMode {
        FORWARD,    // Normal playback
        PING_PONG,  // Back and forth
        RANDOM      // Random frames
    };

    float playbackSpeed{1.0f};
    PlaybackMode playbackMode{FORWARD};
    bool playbackReverse{false};  // For ping-pong mode
    
    void setPlaybackSpeed(float speed) {
        playbackSpeed = speed;
    }

    float getPlaybackSpeed() const {
        return playbackSpeed;
    }

    void setPlaybackMode(PlaybackMode mode) {
        playbackMode = mode;
    }

    PlaybackMode getPlaybackMode() const {
        return playbackMode;
    }

    // Agregar las declaraciones de los métodos de serialización
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

private:
    // Métodos privados
    void processImage();
    void workerFunction();
    void startWorkerThread();
    void stopWorkerThread();
    std::vector<unsigned char>& getCurrentFrameData() {
        return gifFrames[currentFrame].data;
    }
    // Variable para almacenar el path pendiente de cargar
    std::string pendingGifPath;
    bool hasPendingGif = false;

    // Estructura interna para el procesamiento de píxeles
    struct PixelInfo {
        int sourceX, sourceY;
        float r, g, b, a;
        bool processed = false;
    };

    // Funciones de procesamiento de efectos
    void applyGeometricEffects(PixelInfo& pixel, int x, int y);
    void applyPixelation(std::vector<PixelInfo>& pixelBuffer, int y);
    void applyRgbAberration(std::vector<PixelInfo>& pixelBuffer, int y);
    void applyColorAdjustments(std::vector<PixelInfo>& pixelBuffer);
    void applyKernelEffects(std::vector<PixelInfo>& pixelBuffer, int y);
    void applyGlitchEffects(std::vector<PixelInfo>& pixelBuffer, int y);
    void applyPosterizeAndDither(std::vector<PixelInfo>& pixelBuffer, int y);
    void applyPostProcessingEffects(PixelInfo& pixel, int x, int y);
    void applyDataMoshEffects(std::vector<PixelInfo>& pixelBuffer, int y);
};

struct GIFGlitcherWidget : ModuleWidget {
    GIFGlitcherWidget(GIFGlitcher* module);
    ~GIFGlitcherWidget() override;
    void drawLayer(const DrawArgs& args, int layer) override;
    void appendContextMenu(Menu* menu) override;
    
private:
    GIFGlitcher* getModule() {
        return static_cast<GIFGlitcher*>(module);
    }
};
