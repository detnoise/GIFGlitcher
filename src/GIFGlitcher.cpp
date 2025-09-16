#include "GIFGlitcher.hpp"
#include "plugin.hpp"
#include <app/Scene.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <osdialog.h>
#include <cstring>
#include "stb_image.h"
#include "gif_lib.h"
#include <math.hpp>
#include <rack.hpp>

// --- Helper functions for color conversion ---
namespace { // Use an anonymous namespace to limit scope

[[maybe_unused]]
void rgbToHsv(float r, float g, float b, float &h, float &s, float &v) {
    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    v = maxVal; // V is the max component

    if (maxVal == 0.f) { // Achromatic (gray)
        s = 0.f;
        h = 0.f; // Undefined, set to 0
    } else {
        s = delta / maxVal; // S

        if (delta < 1e-6f) { // Achromatic (gray), check with tolerance
             h = 0.f; // Undefined, set to 0
        } else {
            if (maxVal == r) {
                h = 60.f * fmod(((g - b) / delta), 6.f);
            } else if (maxVal == g) {
                h = 60.f * (((b - r) / delta) + 2.f);
            } else { // maxVal == b
                h = 60.f * (((r - g) / delta) + 4.f);
            }
            if (h < 0.f) {
                h += 360.f;
            }
        }
    }
}

[[maybe_unused]]
void hsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
    if (s < 1e-6f) { // Achromatic (gray), check with tolerance
        r = g = b = v;
        return;
    }

    h = fmod(h, 360.0f); // Ensure h is within [0, 360)
    if (h < 0.0f) h += 360.0f;
h /= 60.f; // sector 0 to 5
    int i = static_cast<int>(floor(h));
    float f = h - i; // factorial part of h
    float p = v * (1.f - s);
    float q = v * (1.f - s * f);
    float t = v * (1.f - s * (1.f - f));

    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: // case 5:
            r = v; g = p; b = q; break;
    }
}

// 8x8 Bayer matrix for ordered dithering
static const int bayer8x8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

} // end anonymous namespace


GIFGlitcher::GIFGlitcher() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(BRIGHTNESS_PARAM, 0.0f, 2.0f, 1.0f, "Brightness");
    configParam(CONTRAST_PARAM, 0.0f, 2.0f, 1.0f, "Contrast");
    configParam(SATURATION_PARAM, 0.0f, 2.0f, 1.0f, "Saturation");
    configParam(HUE_SHIFT_PARAM, 0.0f, 1.0f, 0.0f, "Hue Shift");
    configParam(SHARPNESS_PARAM, 0.0f, 5.0f, 0.0f, "Sharpness");
    configParam(PIXELATION_PARAM, 0.0f, 1.0f, 0.0f, "Pixelation");
    configParam(EDGE_DETECT_PARAM, 0.0f, 1.0f, 0.0f, "Edge Detect");
    configParam(RGB_ABERRATION_PARAM, 0.0f, 1.0f, 0.0f, "RGB Aberration");
    configParam(NOISE_PARAM, 0.0f, 1.0f, 0.0f, "Noise");
    configParam(GLITCH_SLICE_PARAM, 0.0f, 1.0f, 0.0f, "Glitch Slice");  // Controls slice intensity
    configParam(POSTERIZE_PARAM, 0.0f, 1.0f, 0.0f, "Color Posterization");
    configParam(DITHER_INTENSITY_PARAM, 0.0f, 1.0f, 0.2f, "Dither Intensity");
    configParam(INTERLACE_INTENSITY_PARAM, 0.0f, 1.0f, 0.5f, "Interlace Intensity");
    configParam(GLITCH_ARTIFACTS_INTENSITY_PARAM, 0.0f, 2.0f, 0.0f, "Glitch Artifacts");
    configParam(GLITCH_BLOCK_SIZE_PARAM, 0.0f, 5.0f, 0.0f, "Glitch Block Size");
    configParam(GLITCH_DISPLACEMENT_PARAM, 0.0f, 1.0f, 0.0f, "Glitch Displacement");
    configParam(BIT_CRUSH_PARAM, 0.0f, 1.0f, 0.0f, "Bit Crush");
    configParam(DATA_SHIFT_PARAM, 0.0f, 1.0f, 0.0f, "Data Shift");
    configParam(PIXEL_SORT_PARAM, 0.0f, 1.0f, 0.0f, "Pixel Sort");

    configInput(BRIGHTNESS_INPUT, "Brightness CV");
    configInput(CONTRAST_INPUT, "Contrast CV");
    configInput(SATURATION_INPUT, "Saturation CV");
    configInput(HUE_SHIFT_INPUT, "Hue Shift CV");
    configInput(SHARPNESS_INPUT, "Sharpness CV");
    configInput(PIXELATION_INPUT, "Pixelation CV");
    configInput(EDGE_DETECT_INPUT, "Edge Detect CV");
    configInput(RGB_ABERRATION_INPUT, "RGB Aberration CV");
    configInput(MIRROR_INPUT, "Mirror");
    configInput(FLIP_INPUT, "Flip");
    configInput(INVERT_INPUT, "Invert Colors");
    configInput(DITHER_INPUT, "Dither");
    configInput(INTERLACE_INPUT, "Interlace");
    configInput(NOISE_INPUT, "Noise CV");
    configInput(GLITCH_SLICE_INPUT, "Glitch Slice CV");
    configInput(POSTERIZE_INPUT, "Posterize CV");
    configInput(HALF_MIRROR_INPUT, "Half Mirror");
    configInput(HALF_MIRROR_VERTICAL_INPUT, "Half Mirror Vertical");
    configInput(GLITCH_ARTIFACTS_INPUT, "Glitch Artifacts");
    configInput(DATA_MOSH_INPUT, "Data Mosh CV");
    configInput(RESET_INPUT, "Reset");
    configInput(RANDOM_INPUT, "Random Effect");

    threadRunning = false;
    startWorkerThread();
}

GIFGlitcher::~GIFGlitcher() {
    // Primero detener el thread worker
    if (threadRunning) {
        threadRunning = false;
        processCV.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    // Limpiar recursos
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (vg) {
            // Limpiar texturas de frames
            for (auto& frame : gifFrames) {
                if (frame.imageHandle) {
                    nvgDeleteImage(vg, frame.imageHandle);
                }
            }
            // Limpiar textura principal si existe
            if (outputImageHandle) {
                nvgDeleteImage(vg, outputImageHandle);
            }
        }
        outputImageHandle = 0;
        imageData.clear();
        processedData.clear();
        gifFrames.clear();
    }
}

void GIFGlitcher::startWorkerThread() {
    if (!threadRunning) {
        threadRunning = true;
        processRequested = false;
        workerThread = std::thread(&GIFGlitcher::workerFunction, this);
    }
}

void GIFGlitcher::stopWorkerThread() {
    if (threadRunning) {
        threadRunning = false;
        processCV.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
}

void GIFGlitcher::process(const ProcessArgs& args) {
    // Check reset input first
    if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
        // Reset all parameters to their default values
        params[BRIGHTNESS_PARAM].setValue(1.0f);
        params[CONTRAST_PARAM].setValue(1.0f);
        params[SATURATION_PARAM].setValue(1.0f);
        params[HUE_SHIFT_PARAM].setValue(0.0f);
        params[SHARPNESS_PARAM].setValue(0.0f);
        params[PIXELATION_PARAM].setValue(0.0f);
        params[EDGE_DETECT_PARAM].setValue(0.0f);
        params[RGB_ABERRATION_PARAM].setValue(0.0f);
        params[NOISE_PARAM].setValue(0.0f);
        params[GLITCH_SLICE_PARAM].setValue(0.0f);
        params[POSTERIZE_PARAM].setValue(0.0f);
        params[DITHER_INTENSITY_PARAM].setValue(0.2f);
        params[INTERLACE_INTENSITY_PARAM].setValue(0.5f);
        params[GLITCH_ARTIFACTS_INTENSITY_PARAM].setValue(0.0f);
        params[GLITCH_BLOCK_SIZE_PARAM].setValue(0.0f);
        params[GLITCH_DISPLACEMENT_PARAM].setValue(0.0f);
        params[BIT_CRUSH_PARAM].setValue(0.0f);
        params[DATA_SHIFT_PARAM].setValue(0.0f);
        params[PIXEL_SORT_PARAM].setValue(0.0f);
    }

    // Randomize parameters on trigger
    if (randomTrigger.process(inputs[RANDOM_INPUT].getVoltage())) {
        params[BRIGHTNESS_PARAM].setValue(random::uniform() * 2.0f);
        params[CONTRAST_PARAM].setValue(random::uniform() * 2.0f);
        params[SATURATION_PARAM].setValue(random::uniform() * 2.0f);
        params[HUE_SHIFT_PARAM].setValue(random::uniform());
        params[SHARPNESS_PARAM].setValue(random::uniform() * 5.0f);
        params[PIXELATION_PARAM].setValue(random::uniform());
        params[EDGE_DETECT_PARAM].setValue(random::uniform());
        params[RGB_ABERRATION_PARAM].setValue(random::uniform());
        params[NOISE_PARAM].setValue(random::uniform());
        params[GLITCH_SLICE_PARAM].setValue(random::uniform());
        params[POSTERIZE_PARAM].setValue(random::uniform());
        params[DITHER_INTENSITY_PARAM].setValue(random::uniform());
        params[INTERLACE_INTENSITY_PARAM].setValue(random::uniform());
        params[GLITCH_ARTIFACTS_INTENSITY_PARAM].setValue(random::uniform() * 2.0f);
        params[GLITCH_BLOCK_SIZE_PARAM].setValue(random::uniform() * 5.0f);
        params[GLITCH_DISPLACEMENT_PARAM].setValue(random::uniform());
        params[BIT_CRUSH_PARAM].setValue(random::uniform());
        params[DATA_SHIFT_PARAM].setValue(random::uniform());
        params[PIXEL_SORT_PARAM].setValue(random::uniform());
    }

    // Actualizar el tiempo acumulado
    accumulatedTime += args.sampleTime;
    if (accumulatedTime > 1000.0f) accumulatedTime = 0.0f;

    ProcessingParams newParams;
    newParams.brightness = rack::math::clamp(params[BRIGHTNESS_PARAM].getValue() + inputs[BRIGHTNESS_INPUT].getVoltage() / 10.0f, 0.0f, 2.0f);
    newParams.contrast = rack::math::clamp(params[CONTRAST_PARAM].getValue() + inputs[CONTRAST_INPUT].getVoltage() / 10.0f, 0.0f, 2.0f);
    newParams.saturation = rack::math::clamp(params[SATURATION_PARAM].getValue() + inputs[SATURATION_INPUT].getVoltage() / 10.0f, 0.0f, 2.0f);
    newParams.hueShift = rack::math::clamp(params[HUE_SHIFT_PARAM].getValue() + inputs[HUE_SHIFT_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.sharpness = rack::math::clamp(params[SHARPNESS_PARAM].getValue() + inputs[SHARPNESS_INPUT].getVoltage() / 10.0f, 0.0f, 5.0f);
    newParams.pixelation = rack::math::clamp(params[PIXELATION_PARAM].getValue() + inputs[PIXELATION_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.edgeDetect = rack::math::clamp(params[EDGE_DETECT_PARAM].getValue() + inputs[EDGE_DETECT_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.rgbAberration = rack::math::clamp(params[RGB_ABERRATION_PARAM].getValue() + inputs[RGB_ABERRATION_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.mirrorEffect = inputs[MIRROR_INPUT].getVoltage() > 2.0f;
    newParams.halfMirrorEffect = inputs[HALF_MIRROR_INPUT].getVoltage() > 2.0f;
    newParams.halfMirrorVerticalEffect = inputs[HALF_MIRROR_VERTICAL_INPUT].getVoltage() > 2.0f;
    newParams.flipEffect = inputs[FLIP_INPUT].getVoltage() > 2.0f;
    newParams.invertColors = inputs[INVERT_INPUT].getVoltage() > 2.0f;
    newParams.ditherEffect = inputs[DITHER_INPUT].getVoltage() > 2.0f;
    newParams.ditherIntensity = params[DITHER_INTENSITY_PARAM].getValue();
    newParams.interlaceEffect = inputs[INTERLACE_INPUT].getVoltage() > 2.0f;
    newParams.interlaceIntensity = params[INTERLACE_INTENSITY_PARAM].getValue();
    newParams.noise = rack::math::clamp(params[NOISE_PARAM].getValue() + inputs[NOISE_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.glitchSlice = rack::math::clamp(params[GLITCH_SLICE_PARAM].getValue() + inputs[GLITCH_SLICE_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
    newParams.posterize = rack::math::clamp(params[POSTERIZE_PARAM].getValue() + inputs[POSTERIZE_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);

    float glitchCv = inputs[GLITCH_ARTIFACTS_INPUT].getVoltage() / 10.0f;
    newParams.glitchArtifacts = rack::math::clamp(params[GLITCH_ARTIFACTS_INTENSITY_PARAM].getValue() + glitchCv, 0.0f, 2.0f);
    newParams.glitchBlockSize = params[GLITCH_BLOCK_SIZE_PARAM].getValue();
    newParams.glitchDisplacement = rack::math::clamp(params[GLITCH_DISPLACEMENT_PARAM].getValue() + glitchCv, 0.0f, 1.0f);

    float dataMoshCv = inputs[DATA_MOSH_INPUT].getVoltage() / 10.0f;
    newParams.bitCrush = rack::math::clamp(params[BIT_CRUSH_PARAM].getValue() + dataMoshCv, 0.0f, 1.0f);
    newParams.dataShift = rack::math::clamp(params[DATA_SHIFT_PARAM].getValue() + dataMoshCv, 0.0f, 1.0f);
    newParams.pixelSort = rack::math::clamp(params[PIXEL_SORT_PARAM].getValue() + dataMoshCv, 0.0f, 1.0f);

    bool paramsChanged = std::memcmp(&currentParams, &newParams, sizeof(ProcessingParams)) != 0;

    if (paramsChanged) {
        std::lock_guard<std::mutex> lock(paramsMutex);
        currentParams = newParams;
        processRequested = true;
        processCV.notify_one();
    }

    // Actualizar animación
    if (isAnimated && !gifFrames.empty()) {
        frameAccumulator += args.sampleTime * playbackSpeed;
        float frameTime = gifFrames[currentFrame].delay / 1000.0f;

        if (frameAccumulator >= frameTime) {
            frameAccumulator -= frameTime;

            // Actualizar el frame según el modo de reproducción
            switch (playbackMode) {
                case FORWARD:
                    currentFrame = (currentFrame + 1) % gifFrames.size();
                    break;

                case PING_PONG:
                    if (!playbackReverse) {
                        currentFrame++;
                        if (currentFrame >= gifFrames.size() - 1) {
                            currentFrame = gifFrames.size() - 1;
                            playbackReverse = true;
                        }
                    } else {
                        currentFrame--;
                        if (currentFrame <= 0) {
                            currentFrame = 0;
                            playbackReverse = false;
                        }
                    }
                    break;

                case RANDOM:
                    currentFrame = static_cast<int>(random::uniform() * gifFrames.size());
                    break;
            }

            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                imageData = getCurrentFrameData();
                processRequested = true;
            }
            processCV.notify_one();
        }
    }
}

void GIFGlitcher::loadImage(std::string path) {
    if (path.empty()) {
        std::cerr << "Empty path provided" << std::endl;
        return;
    }

    std::cout << "Loading image from path: " << path << std::endl;

    // Load image using stb_image first to validate
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        std::cerr << "Failed to load image: " << path << " - " << stbi_failure_reason() << std::endl;
        return;
    }

    // Validate dimensions
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        std::cerr << "Invalid image dimensions: " << width << "x" << height << std::endl;
        stbi_image_free(data);
        return;
    }

    std::cout << "Image loaded successfully: " << width << "x" << height << " channels: " << channels << std::endl;

    // If validation passes, update the path and trigger reload
    imagePath = path;
    stbi_image_free(data);

    if (vg) {
        reloadImage();
    } else {
        std::cerr << "No valid NVG context available" << std::endl;
    }
}

void GIFGlitcher::reloadImage() {
    if (!vg) {
        std::cerr << "No valid NanoVG context" << std::endl;
        return;
    }

    std::cout << "Reloading image from: " << imagePath << std::endl;

    // Clear previous image if it exists
    if (outputImageHandle) {
        nvgDeleteImage(vg, outputImageHandle);
        outputImageHandle = 0;
    }

    // Load the image using stb_image with explicit RGBA format
    int channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(imagePath.c_str(), &imageWidth, &imageHeight, &channels, 4);

    if (!data) {
        std::cerr << "Could not load image " << imagePath << ": " << stbi_failure_reason() << std::endl;
        return;
    }

    try {
        // Allocate buffers
        size_t dataSize = static_cast<size_t>(imageWidth) * imageHeight * 4;
        imageData.resize(dataSize);
        processedData.resize(dataSize);

        // Copy data safely
        std::memcpy(imageData.data(), data, dataSize);
        std::memcpy(processedData.data(), data, dataSize);

        // Create NanoVG image
        outputImageHandle = nvgCreateImageRGBA(vg, imageWidth, imageHeight, NVG_IMAGE_NEAREST, processedData.data());

        if (outputImageHandle == 0) {
            std::cerr << "Failed to create NanoVG image" << std::endl;
            stbi_image_free(data);
            return;
        }

        stbi_image_free(data);
        std::cout << "Successfully loaded image " << imagePath
                  << " with size " << imageWidth << "x" << imageHeight
                  << " and handle " << outputImageHandle << std::endl;

        // Force initial texture update
        textureNeedsUpdate = true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during image loading: " << e.what() << std::endl;
        if (data) stbi_image_free(data);
        outputImageHandle = 0;
        imageData.clear();
        processedData.clear();
    }
}

void GIFGlitcher::applyGeometricEffects(PixelInfo& pixel, int x, int y) {
    // Determinar coordenadas fuente basadas en efectos de espejo
    pixel.sourceX = x;
    pixel.sourceY = y;

    // Aplicar efectos de espejo horizontal
    if (currentParams.mirrorEffect) {
        pixel.sourceX = imageWidth - 1 - x;
    } else if (currentParams.halfMirrorEffect && x >= imageWidth / 2) {
        pixel.sourceX = imageWidth - 1 - x;
    }

    // Aplicar efectos de espejo vertical
    if (currentParams.flipEffect) {
        pixel.sourceY = imageHeight - 1 - y;
    } else if (currentParams.halfMirrorVerticalEffect && y >= imageHeight / 2) {
        pixel.sourceY = imageHeight - 1 - y;
    }
}

void GIFGlitcher::applyPixelation(std::vector<PixelInfo>& pixelBuffer, int y) {
    if (currentParams.pixelation <= 0.0f) return;

    int pixelSize = std::max(1, static_cast<int>(currentParams.pixelation * 40.0f));
    for (int x = 0; x < imageWidth; x += pixelSize) {
        float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f;
        int count = 0;

        for (int px = 0; px < pixelSize && x + px < imageWidth; ++px) {
            avgR += pixelBuffer[x + px].r;
            avgG += pixelBuffer[x + px].g;
            avgB += pixelBuffer[x + px].b;
            count++;
        }

        if (count > 0) {
            avgR /= count;
            avgG /= count;
            avgB /= count;

            for (int px = 0; px < pixelSize && x + px < imageWidth; ++px) {
                pixelBuffer[x + px].r = avgR;
                pixelBuffer[x + px].g = avgG;
                pixelBuffer[x + px].b = avgB;
            }
        }
    }
}

void GIFGlitcher::applyRgbAberration(std::vector<PixelInfo>& pixelBuffer, int y) {
    if (currentParams.rgbAberration <= 0.0f) return;

    int shift = static_cast<int>(currentParams.rgbAberration * 20.0f);
    for (int x = 0; x < imageWidth; ++x) {
        int aberrationX = currentParams.mirrorEffect ?
            (pixelBuffer[x].sourceX - shift) :
            (pixelBuffer[x].sourceX + shift);

        if (aberrationX >= 0 && aberrationX < imageWidth) {
            int aberrationIdx = (pixelBuffer[x].sourceY * imageWidth + aberrationX) * 4;
            float rShifted = imageData[aberrationIdx] / 255.0f;
            pixelBuffer[x].r = pixelBuffer[x].r * (1.0f - currentParams.rgbAberration) + rShifted * currentParams.rgbAberration;
        }
    }
}

void GIFGlitcher::applyColorAdjustments(std::vector<PixelInfo>& pixelBuffer) {
    for (auto& pixel : pixelBuffer) {
        // Aplicar brillo y contraste
        pixel.r = (pixel.r - 0.5f) * currentParams.contrast + 0.5f + (currentParams.brightness - 1.0f);
        pixel.g = (pixel.g - 0.5f) * currentParams.contrast + 0.5f + (currentParams.brightness - 1.0f);
        pixel.b = (pixel.b - 0.5f) * currentParams.contrast + 0.5f + (currentParams.brightness - 1.0f);

        // Convertir a HSV para saturación y ajuste de tono
        float h, s, v;
        rgbToHsv(pixel.r, pixel.g, pixel.b, h, s, v);

        // Aplicar saturación y cambio de tono
        s *= currentParams.saturation;
        h += currentParams.hueShift * 360.f; // Scale hue shift to 0-360 range

        // Convertir de vuelta a RGB
        hsvToRgb(h, s, v, pixel.r, pixel.g, pixel.b);
    }
}

void GIFGlitcher::applyKernelEffects(std::vector<PixelInfo>& pixelBuffer, int y) {
    if (currentParams.edgeDetect <= 0.0f && currentParams.sharpness <= 0.0f) return;

    std::vector<PixelInfo> edgeBuffer = pixelBuffer;
    for (int x = 1; x < imageWidth - 1; ++x) {
        if (currentParams.edgeDetect > 0.0f) {
            float gx = 0.0f, gy = 0.0f;
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    if (x + j >= 0 && x + j < imageWidth && y + i >= 0 && y + i < imageHeight) {
                        float val = (pixelBuffer[x + j].r + pixelBuffer[x + j].g + pixelBuffer[x + j].b) / 3.0f;
                        gx += val * ((j == 0) ? 0 : (j == 1) ? 1 : -1);
                        gy += val * ((i == 0) ? 0 : (i == 1) ? 1 : -1);
                    }
                }
            }
            float edge = std::sqrt(gx * gx + gy * gy) * currentParams.edgeDetect;
            edgeBuffer[x].r = edgeBuffer[x].g = edgeBuffer[x].b = edge;
        }

        if (currentParams.sharpness > 0.0f) {
            float centerR = pixelBuffer[x].r;
            float centerG = pixelBuffer[x].g;
            float centerB = pixelBuffer[x].b;
            float blurR = 0.0f, blurG = 0.0f, blurB = 0.0f;
            int validNeighbors = 0;

            for (int j = -1; j <= 1; j++) {
                if (x + j >= 0 && x + j < imageWidth) {
                    blurR += pixelBuffer[x + j].r;
                    blurG += pixelBuffer[x + j].g;
                    blurB += pixelBuffer[x + j].b;
                    validNeighbors++;
                }
            }

            if (validNeighbors > 0) {
                blurR /= validNeighbors;
                blurG /= validNeighbors;
                blurB /= validNeighbors;
                float normalizedSharpness = currentParams.sharpness;
                edgeBuffer[x].r = rack::math::clamp(centerR + (centerR - blurR) * normalizedSharpness, 0.0f, 1.0f);
                edgeBuffer[x].g = rack::math::clamp(centerG + (centerG - blurG) * normalizedSharpness, 0.0f, 1.0f);
                edgeBuffer[x].b = rack::math::clamp(centerB + (centerB - blurB) * normalizedSharpness, 0.0f, 1.0f);
            }
        }
    }
    pixelBuffer = edgeBuffer;
}

void GIFGlitcher::applyGlitchEffects(std::vector<PixelInfo>& pixelBuffer, int y) {
    if (currentParams.glitchSlice > 0.0f) {
        int sliceHeight = static_cast<int>(10 + currentParams.glitchSlice * 40);
        int maxOffset = static_cast<int>(currentParams.glitchSlice * imageWidth * 0.3f);
        int timeSlice = static_cast<int>(accumulatedTime * 10) % sliceHeight;

        if ((y + timeSlice) / sliceHeight % 2 == 0) {
            int offset = static_cast<int>(random::uniform() * maxOffset);
            std::vector<PixelInfo> shiftedLine = pixelBuffer;
            for (int x = 0; x < imageWidth; ++x) {
                int newX = (x + offset) % imageWidth;
                pixelBuffer[x] = shiftedLine[newX];
                pixelBuffer[x].r *= 1.0f + 0.2f * currentParams.glitchSlice;
                pixelBuffer[x].b *= 1.0f - 0.1f * currentParams.glitchSlice;
            }
        }
    }

    if (currentParams.glitchArtifacts > 0.0f) {
        const std::vector<PixelInfo> originalLine = pixelBuffer;
        float artifactProbability = 0.05f * currentParams.glitchArtifacts;
        int blockSize = 1 + static_cast<int>(currentParams.glitchBlockSize * 31);

        for (int x = 0; x < imageWidth; x += blockSize) {
            if (random::uniform() < artifactProbability) {
                // Si el desplazamiento está activo, decidir si desplazar/manchar o cambiar color
                if (currentParams.glitchDisplacement > 0.0f && random::uniform() < 0.5f) {
                    if (currentParams.glitchDisplacement > 0.5f) {
                        // Modo Smear
                        PixelInfo smearPixel = originalLine[x];
                        for (int bx = 0; bx < blockSize && (x + bx) < imageWidth; ++bx) {
                            pixelBuffer[x + bx] = smearPixel;
                        }
                    } else {
                        // Modo Displacement
                        float displacementAmount = currentParams.glitchDisplacement * 2.0f; // Escalar a 0-1
                        float maxDisplacement = imageWidth * 0.3f * displacementAmount;
                        int xOffset = static_cast<int>((random::uniform() * 2.f - 1.f) * maxDisplacement);

                        for (int bx = 0; bx < blockSize && (x + bx) < imageWidth; ++bx) {
                            int sourceX = x + bx + xOffset;
                            sourceX = (sourceX % imageWidth + imageWidth) % imageWidth; // Wrap around
                            pixelBuffer[x + bx] = originalLine[sourceX];
                        }
                    }
                } else {
                    // Modo Color Shift
                    float shiftAmount = currentParams.glitchArtifacts * 0.5f;
                    float rShift = (random::uniform() * 2.f - 1.f) * shiftAmount;
                    float gShift = (random::uniform() * 2.f - 1.f) * shiftAmount;
                    float bShift = (random::uniform() * 2.f - 1.f) * shiftAmount;

                    for (int bx = 0; bx < blockSize && (x + bx) < imageWidth; ++bx) {
                        pixelBuffer[x + bx].r = rack::math::clamp(originalLine[x + bx].r + rShift, 0.0f, 1.0f);
                        pixelBuffer[x + bx].g = rack::math::clamp(originalLine[x + bx].g + gShift, 0.0f, 1.0f);
                        pixelBuffer[x + bx].b = rack::math::clamp(originalLine[x + bx].b + bShift, 0.0f, 1.0f);
                    }
                }
            }
        }
    }
}

void GIFGlitcher::applyDataMoshEffects(std::vector<PixelInfo>& pixelBuffer, int y) {
    // Bit Crush
    if (currentParams.bitCrush > 0.0f) {
        int bits = 8 - static_cast<int>(currentParams.bitCrush * 7.f);
        if (bits < 8) {
            int mask = 0xFF << (8 - bits);
            for (auto& pixel : pixelBuffer) {
                pixel.r = (static_cast<int>(pixel.r * 255.f) & mask) / 255.f;
                pixel.g = (static_cast<int>(pixel.g * 255.f) & mask) / 255.f;
                pixel.b = (static_cast<int>(pixel.b * 255.f) & mask) / 255.f;
            }
        }
    }

    // Data Shift
    if (currentParams.dataShift > 0.0f) {
        int blockSize = 32;
        for (int x = 0; x < imageWidth; x += blockSize) {
            if (random::uniform() < currentParams.dataShift * 0.1f) { // Probability
                int shift = static_cast<int>(currentParams.dataShift * 7.f); // Shift amount
                for (int bx = 0; bx < blockSize && (x + bx) < imageWidth; ++bx) {
                    int r = static_cast<int>(pixelBuffer[x + bx].r * 255.f);
                    int g = static_cast<int>(pixelBuffer[x + bx].g * 255.f);
                    int b = static_cast<int>(pixelBuffer[x + bx].b * 255.f);
                    unsigned int packed = (r << 16) | (g << 8) | b;
                    packed <<= shift;
                    pixelBuffer[x + bx].r = ((packed >> 16) & 0xFF) / 255.f;
                    pixelBuffer[x + bx].g = ((packed >> 8) & 0xFF) / 255.f;
                    pixelBuffer[x + bx].b = (packed & 0xFF) / 255.f;
                }
            }
        }
    }

    // Pixel Sort
    if (currentParams.pixelSort > 0.0f) {
        float threshold = currentParams.pixelSort;
        int start = -1;

        for (int x = 0; x < imageWidth; ++x) {
            float brightness = (pixelBuffer[x].r + pixelBuffer[x].g + pixelBuffer[x].b) / 3.f;
            if (start == -1 && brightness > threshold) {
                start = x;
            }
            if (start != -1 && (brightness < threshold || x == imageWidth - 1)) {
                std::sort(pixelBuffer.begin() + start, pixelBuffer.begin() + x,
                          [](const PixelInfo& a, const PixelInfo& b) {
                              return (a.r + a.g + a.b) < (b.r + b.g + b.b);
                          });
                start = -1;
            }
        }
    }
}

void GIFGlitcher::applyPostProcessingEffects(PixelInfo& pixel, int x, int y) {
    if (currentParams.interlaceEffect) {
        int lineOffset = static_cast<int>(accumulatedTime * 60) % 2;
        if ((y + lineOffset) % 2 == 0) {
            float intensity = 1.0f - currentParams.interlaceIntensity;
            pixel.r *= intensity; pixel.g *= intensity; pixel.b *= intensity;
        }
    }

    if (currentParams.noise > 0.0f) {
        float noiseR = random::uniform() * 2.0f - 1.0f;
        float noiseG = random::uniform() * 2.0f - 1.0f;
        float noiseB = random::uniform() * 2.0f - 1.0f;
        pixel.r = rack::math::clamp(pixel.r + noiseR * currentParams.noise * 0.5f, 0.0f, 1.0f);
        pixel.g = rack::math::clamp(pixel.g + noiseG * currentParams.noise * 0.5f, 0.0f, 1.0f);
        pixel.b = rack::math::clamp(pixel.b + noiseB * currentParams.noise * 0.5f, 0.0f, 1.0f);
    }

    if (currentParams.invertColors) {
        pixel.r = 1.0f - pixel.r;
        pixel.g = 1.0f - pixel.g;
        pixel.b = 1.0f - pixel.b;
    }
}

void GIFGlitcher::processImage() {
    if (imageData.empty()) return;

    try {
        std::vector<unsigned char> workBuffer(imageData.size());
        std::vector<unsigned char> localImageData;
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            localImageData = imageData;
        }

        const int chunkSize = 64;

        for (int y = 0; y < imageHeight; y += chunkSize) {
            int endY = std::min(y + chunkSize, imageHeight);

            #pragma omp parallel for if(imageWidth > 512)
            for (int cy = y; cy < endY; ++cy) {
                std::vector<PixelInfo> pixelBuffer(imageWidth);

                // 1. Obtener píxeles fuente con efectos geométricos
                for (int x = 0; x < imageWidth; ++x) {
                    PixelInfo& pixel = pixelBuffer[x];
                    applyGeometricEffects(pixel, x, cy);
                    int sourceIdx = (pixel.sourceY * imageWidth + pixel.sourceX) * 4;
                    pixel.r = localImageData[sourceIdx] / 255.0f;
                    pixel.g = localImageData[sourceIdx + 1] / 255.0f;
                    pixel.b = localImageData[sourceIdx + 2] / 255.0f;
                    pixel.a = localImageData[sourceIdx + 3] / 255.0f;
                }

                // 2. Aplicar efectos de bloque (pixelación)
                applyPixelation(pixelBuffer, cy);

                // 3. Aplicar aberración cromática
                applyRgbAberration(pixelBuffer, cy);

                // 4. Aplicar ajustes de color por píxel
                applyColorAdjustments(pixelBuffer);

                // 5. Aplicar posterización y dither
                applyPosterizeAndDither(pixelBuffer, cy);

                // 6. Aplicar efectos de convolución/vecindad
                applyKernelEffects(pixelBuffer, cy);

                // 7. Aplicar efectos de glitch
                applyGlitchEffects(pixelBuffer, cy);

                // 8. Aplicar efectos de Data Mosh
                applyDataMoshEffects(pixelBuffer, cy);

                // 9. Aplicar efectos de post-procesamiento y guardar
                for (int x = 0; x < imageWidth; ++x) {
                    PixelInfo& pixel = pixelBuffer[x];
                    applyPostProcessingEffects(pixel, x, cy);

                    int currentIdx = (cy * imageWidth + x) * 4;
                    workBuffer[currentIdx] = static_cast<unsigned char>(rack::math::clamp(pixel.r * 255.0f, 0.0f, 255.0f));
                    workBuffer[currentIdx + 1] = static_cast<unsigned char>(rack::math::clamp(pixel.g * 255.0f, 0.0f, 255.0f));
                    workBuffer[currentIdx + 2] = static_cast<unsigned char>(rack::math::clamp(pixel.b * 255.0f, 0.0f, 255.0f));
                    workBuffer[currentIdx + 3] = static_cast<unsigned char>(pixel.a * 255.0f);
                }
            }

            if (!threadRunning) return;
        }

        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            processedData = std::move(workBuffer);
            textureNeedsUpdate = true;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception during image processing: " << e.what() << std::endl;
    }
}

void GIFGlitcher::applyPosterizeAndDither(std::vector<PixelInfo>& pixelBuffer, int y) {
    // Si ninguno de los efectos está activo, no hacer nada.
    if (currentParams.posterize <= 0.0f && !currentParams.ditherEffect) {
        return;
    }

    float levels = 0.f;
    if (currentParams.posterize > 0.0f) {
        levels = 2.0f + (currentParams.posterize * 14.0f);
    }

    for (int x = 0; x < imageWidth; ++x) {
        PixelInfo& pixel = pixelBuffer[x];

        if (currentParams.ditherEffect) {
            float bayer_value = bayer8x8[y % 8][x % 8] / 64.0f; // Rango [0, 1)

            if (levels > 0.f) {
                // Dithering activo CON posterización.
                // Añadir ajuste antes de la cuantización.
                float dither_strength = (1.0f / levels) * currentParams.ditherIntensity;
                float dither_adjustment = (bayer_value - 0.5f) * dither_strength;
                pixel.r += dither_adjustment;
                pixel.g += dither_adjustment;
                pixel.b += dither_adjustment;
            } else {
                // Dithering activo SIN posterización.
                // Aplicar un patrón de dither estilístico.
                float dither_mod = (bayer_value - 0.5f) * currentParams.ditherIntensity * 0.2f;
                pixel.r += dither_mod;
                pixel.g += dither_mod;
                pixel.b += dither_mod;
            }
        }

        // Aplicar posterización si está activa
        if (levels > 0.f) {
            pixel.r = std::floor(pixel.r * levels) / levels;
            pixel.g = std::floor(pixel.g * levels) / levels;
            pixel.b = std::floor(pixel.b * levels) / levels;
        }
    }
}


void GIFGlitcher::workerFunction() {
    while (threadRunning) {
        ProcessingParams params;
        {
            std::unique_lock<std::mutex> lock(paramsMutex);
            processCV.wait(lock, [this] {
                return processRequested || !threadRunning;
            });

            if (!threadRunning) break;

            params = currentParams;
            processRequested = false;
        }

        try {
            processImage();
        }
        catch (const std::exception& e) {
            std::cerr << "Error in worker thread: " << e.what() << std::endl;
        }
    }
}


GIFGlitcherWidget::~GIFGlitcherWidget() {
    // Asegurarse de que el módulo libere sus recursos
    if (GIFGlitcher* mod = getModule()) {
        mod->setVG(nullptr);  // Desvincula el contexto NanoVG
    }
}

GIFGlitcherWidget::GIFGlitcherWidget(GIFGlitcher* module) {
    setModule(module);
    box.size = Vec(RACK_GRID_WIDTH * 30, RACK_GRID_HEIGHT);

    // Establecer el contexto NanoVG inmediatamente si el módulo existe
    if (module) {
        module->setVG(APP->window->vg);
    }

    setPanel(createPanel(asset::plugin(pluginInstance, "res/VCV_PANEL_30HP.svg")));

    // Add screws
    addChild(createWidget<ScrewSilver>(Vec(0 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Control layout parameters
    const float startY = 30; // Start Y position for the first control
    const float spacing = 32; // Vertical spacing between controls
    const float inputX = 30; // X position for inputs
    const float knobX = 70; // X position for knobs
    const float rightInputX = 120; // X position for right inputs
    const float resetX = box.size.x - 27.0f; // 25 pixels from right edge

    // Add reset input to the right of display area
    addInput(createInputCentered<PJ301MPort>(Vec(resetX, 25), module, GIFGlitcher::RESET_INPUT));

    // Random Input
    addInput(createInputCentered<PJ301MPort>(Vec(resetX, 60), module, GIFGlitcher::RANDOM_INPUT));


    // Add main inputs and knobs
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 0), module, GIFGlitcher::BRIGHTNESS_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 0), module, GIFGlitcher::BRIGHTNESS_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 1), module, GIFGlitcher::CONTRAST_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 1), module, GIFGlitcher::CONTRAST_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 2), module, GIFGlitcher::SATURATION_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 2), module, GIFGlitcher::SATURATION_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 3), module, GIFGlitcher::HUE_SHIFT_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 3), module, GIFGlitcher::HUE_SHIFT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 4), module, GIFGlitcher::SHARPNESS_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 4), module, GIFGlitcher::SHARPNESS_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 5), module, GIFGlitcher::PIXELATION_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 5), module, GIFGlitcher::PIXELATION_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 6), module, GIFGlitcher::EDGE_DETECT_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 6), module, GIFGlitcher::EDGE_DETECT_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 7), module, GIFGlitcher::RGB_ABERRATION_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 7), module, GIFGlitcher::RGB_ABERRATION_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 8), module, GIFGlitcher::NOISE_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 8), module, GIFGlitcher::NOISE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 9), module, GIFGlitcher::GLITCH_SLICE_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 9), module, GIFGlitcher::GLITCH_SLICE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(inputX, startY + spacing * 10), module, GIFGlitcher::POSTERIZE_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX, startY + spacing * 10), module, GIFGlitcher::POSTERIZE_PARAM));

    // Right side inputs
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 0), module, GIFGlitcher::MIRROR_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 1), module, GIFGlitcher::FLIP_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 2), module, GIFGlitcher::INVERT_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 3), module, GIFGlitcher::DITHER_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(rightInputX + 40, startY + spacing * 3), module, GIFGlitcher::DITHER_INTENSITY_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 4), module, GIFGlitcher::INTERLACE_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(rightInputX + 40, startY + spacing * 4), module, GIFGlitcher::INTERLACE_INTENSITY_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 5), module, GIFGlitcher::HALF_MIRROR_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 6), module, GIFGlitcher::HALF_MIRROR_VERTICAL_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 7), module, GIFGlitcher::GLITCH_ARTIFACTS_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(rightInputX + 40, startY + spacing * 7), module, GIFGlitcher::GLITCH_ARTIFACTS_INTENSITY_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(rightInputX, startY + spacing * 8), module, GIFGlitcher::GLITCH_BLOCK_SIZE_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(rightInputX + 40, startY + spacing * 8), module, GIFGlitcher::GLITCH_DISPLACEMENT_PARAM));

    // Data Mosh Section
    addInput(createInputCentered<PJ301MPort>(Vec(rightInputX, startY + spacing * 9), module, GIFGlitcher::DATA_MOSH_INPUT));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX + 90, startY + spacing * 9), module, GIFGlitcher::BIT_CRUSH_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX + 50, startY + spacing * 10), module, GIFGlitcher::DATA_SHIFT_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(knobX + 90, startY + spacing * 10), module, GIFGlitcher::PIXEL_SORT_PARAM));

}

struct PlaybackSpeedItem : MenuItem {
    GIFGlitcher* module;
    float speed;

    PlaybackSpeedItem(GIFGlitcher* mod, float spd, const std::string& label) {
        module = mod;
        speed = spd;
        text = label;
        rightText = CHECKMARK(module->getPlaybackSpeed() == speed);
    }

    void onAction(const event::Action& e) override {
        module->setPlaybackSpeed(speed);
    }
};

struct PlaybackSpeedMenu : MenuItem {
    GIFGlitcher* module;

    PlaybackSpeedMenu(GIFGlitcher* mod) {
        module = mod;
        text = "Playback Speed";
        rightText = RIGHT_ARROW;
    }

    Menu* createChildMenu() override {
        Menu* menu = new Menu;
        menu->addChild(new PlaybackSpeedItem(module, 0.25f, "0.25x (Very Slow)"));
        menu->addChild(new PlaybackSpeedItem(module, 0.5f, "0.5x (Slow)"));
        menu->addChild(new PlaybackSpeedItem(module, 1.0f, "1.0x (Normal)"));
        menu->addChild(new PlaybackSpeedItem(module, 1.5f, "1.5x (Fast)"));
        menu->addChild(new PlaybackSpeedItem(module, 2.0f, "2.0x (Very Fast)"));
        menu->addChild(new PlaybackSpeedItem(module, 4.0f, "4.0x (Ultra Fast)"));
        return menu;
    }
};

struct PlaybackModeItem : MenuItem {
    GIFGlitcher* module;
    GIFGlitcher::PlaybackMode mode;

    PlaybackModeItem(GIFGlitcher* mod, GIFGlitcher::PlaybackMode m, const std::string& label) {
        module = mod;
        mode = m;
        text = label;
        rightText = CHECKMARK(module->getPlaybackMode() == mode);
    }

    void onAction(const event::Action& e) override {
        module->setPlaybackMode(mode);
    }
};

struct PlaybackModeMenu : MenuItem {
    GIFGlitcher* module;

    PlaybackModeMenu(GIFGlitcher* mod) {
        module = mod;
        text = "Playback Mode";
        rightText = RIGHT_ARROW;
    }

    Menu* createChildMenu() override {
        Menu* menu = new Menu;
        menu->addChild(new PlaybackModeItem(module, GIFGlitcher::FORWARD, "Forward"));
        menu->addChild(new PlaybackModeItem(module, GIFGlitcher::PING_PONG, "Ping-Pong"));
        menu->addChild(new PlaybackModeItem(module, GIFGlitcher::RANDOM, "Random"));
        return menu;
    }
};

void GIFGlitcherWidget::appendContextMenu(Menu* menu) {
    GIFGlitcher* module = dynamic_cast<GIFGlitcher*>(this->module);
    if (!module)
        return;

    menu->addChild(new MenuSeparator());

    menu->addChild(createMenuItem("Load Image", "", [=]() {
        osdialog_filters* filters = osdialog_filters_parse("Image Files:png,jpg,jpeg");
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
        osdialog_filters_free(filters);

        if (path) {
            module->loadImage(path);
            free(path);
        }
    }));

    menu->addChild(createMenuItem("Load GIF", "", [=]() {
        osdialog_filters* filters = osdialog_filters_parse("GIF:gif");
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
        osdialog_filters_free(filters);

        if (path) {
            module->loadGif(path);
            free(path);
        }
    }));

    // Agregar los menús solo si hay un GIF cargado
    if (module->isImageLoaded() && !module->gifFrames.empty()) {
        menu->addChild(new PlaybackSpeedMenu(module));
        menu->addChild(new PlaybackModeMenu(module));
    }
}

void GIFGlitcherWidget::drawLayer(const DrawArgs& args, int layer) {
    if (layer == 1) {
        GIFGlitcher* mod = dynamic_cast<GIFGlitcher*>(module);
        if (!mod) return;

        // Usar APP->engine->getTime() en su lugar
        float time = APP->engine->getSampleTime() * APP->engine->getFrame();
        float hue = fmod(time * 0.2f, 0.5f); // Velocidad de cambio de color
        float r, g, b;

        // Convertir HSV a RGB (hue -> RGB)
        float hueTemp = hue * 6.0f;
        int i = static_cast<int>(hueTemp);
        float f = hueTemp - i;
        float p = 0.0f;
        float q = 1.0f * (1.0f - f);
        float t = 1.0f * f;

        // Calcular RGB basado en el sector de hue
        switch (i % 6) {
            case 0: r = 1.0f; g = t; b = p; break;
            case 1: r = q; g = 1.0f; b = p; break;
            case 2: r = p; g = 1.0f; b = t; break;
            case 3: r = p; g = q; b = 1.0f; break;
            case 4: r = t; g = p; b = 1.0f; break;
            case 5: r = 1.0f; g = p; b = q; break;
            default: r = g = b = 1.0f; break;
        }

        // Dibujar el título "GIF Glitcher"
        nvgSave(args.vg);
        nvgFontSize(args.vg, 16);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
        nvgFillColor(args.vg, nvgRGBf(r, g, b));
        nvgText(args.vg, box.size.x / 2, 15, "GIF Glitcher", NULL);
        nvgRestore(args.vg);

        // Dibujar "DETNOISE" en la parte inferior
        nvgSave(args.vg);
        nvgFontSize(args.vg, 14);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
        nvgFillColor(args.vg, nvgRGBf(r, g, b));
        nvgText(args.vg, box.size.x / 2, RACK_GRID_HEIGHT - 8, "DETNOISE", NULL);
        nvgRestore(args.vg);

        if (!mod->getOutputImageHandle()) return;

        nvgSave(args.vg);

        // Definir el área de visualización maximizada
        // Ajustar estos valores para aprovechar al máximo el espacio disponible
        float topMargin = 25.0f;  // Espacio para el título
        float bottomMargin = 25.0f;  // Espacio para el texto inferior
        float leftMargin = 150.0f;  // Espacio para los controles izquierdos
        float rightMargin = 5.0f;  // Margen derecho mínimo

        // Calcular el área disponible para la visualización
        float displayX = leftMargin;
        float displayWidth = box.size.x - (leftMargin + rightMargin);
        float displayY = topMargin;
        float displayHeight = box.size.y - (topMargin + bottomMargin);

        // Calcular el aspect ratio de la imagen y del área de visualización
        float imageAspect = static_cast<float>(mod->getImageWidth()) / mod->getImageHeight();
        float displayAspect = displayWidth / displayHeight;

        float width, height, posX, posY;

        // Ajustar las dimensiones para mantener el aspect ratio
        if (imageAspect > displayAspect) {
            // La imagen es más ancha que el área de visualización
            width = displayWidth;
            height = width / imageAspect;
            posX = displayX;
            posY = displayY + ((displayHeight - height) / 2);
        } else {
            // La imagen es más alta que el área de visualización
            height = displayHeight;
            width = height * imageAspect;
            posX = displayX + ((displayWidth - width) / 2);
            posY = displayY;
        }

        // Update texture if needed
        if (mod->textureNeedsUpdate) {
            std::lock_guard<std::mutex> lock(mod->bufferMutex);
            nvgUpdateImage(args.vg, mod->getOutputImageHandle(), mod->getProcessedDataPtr());
            mod->textureNeedsUpdate = false;
        }

        // Dibujar un fondo para la imagen
        nvgBeginPath(args.vg);
        nvgRect(args.vg, posX - 2, posY - 2, width + 4, height + 4);
        nvgFillColor(args.vg, nvgRGBA(20, 20, 20, 255));
        nvgFill(args.vg);

        // Dibujar un borde alrededor de la imagen
        nvgBeginPath(args.vg);
        nvgRect(args.vg, posX - 2, posY - 2, width + 4, height + 4);
        nvgStrokeColor(args.vg, nvgRGBA(100, 100, 100, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        // Draw the image
        nvgBeginPath(args.vg);
        NVGpaint imgPaint = nvgImagePattern(args.vg, posX, posY, width, height, 0, mod->getOutputImageHandle(), 1.0f);
        nvgRect(args.vg, posX, posY, width, height);
        nvgFillPaint(args.vg, imgPaint);
        nvgFill(args.vg);

        nvgRestore(args.vg);
    }
    ModuleWidget::drawLayer(args, layer);
}



bool GIFGlitcher::loadGif(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    // Guardar el path incluso si no podemos cargar ahora
    imagePath = path;

    // Si no hay contexto VG, guardar el path para cargarlo más tarde
    if (!vg) {
        INFO("GIFGlitcher: No hay contexto VG disponible, guardando path para carga posterior: %s", path.c_str());
        pendingGifPath = path;
        hasPendingGif = true;
        return false;
    }

    int error;
    GifFileType* gif = DGifOpenFileName(path.c_str(), &error);
    if (!gif) {
        INFO("GIFGlitcher: Error al abrir archivo GIF: %s (error %d)", path.c_str(), error);
        return false;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        INFO("GIFGlitcher: Error al leer contenido del GIF: %s (error %d)", path.c_str(), error);
        DGifCloseFile(gif, &error);
        return false;
    }

    // Verificar dimensiones del GIF
    if (gif->SWidth <= 0 || gif->SHeight <= 0 || gif->SWidth > 4096 || gif->SHeight > 4096) {
        INFO("GIFGlitcher: Dimensiones de GIF inválidas: %dx%d", gif->SWidth, gif->SHeight);
        DGifCloseFile(gif, &error);
        return false;
    }

    // Stop worker thread temporarily
    stopWorkerThread();

    {
        std::lock_guard<std::mutex> lock(bufferMutex);

        // Clear existing resources
        if (vg) {
            // Limpiar texturas de frames existentes
            for (auto& frame : gifFrames) {
                if (frame.imageHandle) {
                    nvgDeleteImage(vg, frame.imageHandle);
                }
            }

            // Limpiar textura principal si existe
            if (outputImageHandle) {
                nvgDeleteImage(vg, outputImageHandle);
                outputImageHandle = 0;
            }
        }

        // Clear existing frames
        gifFrames.clear();
        currentFrame = 0;
        frameAccumulator = 0;

        imageWidth = gif->SWidth;
        imageHeight = gif->SHeight;

        INFO("GIFGlitcher: Dimensiones del GIF: %dx%d, %d frames",
             imageWidth, imageHeight, gif->ImageCount);

        // Verificar si hay frames
        if (gif->ImageCount <= 0) {
            INFO("GIFGlitcher: El GIF no tiene frames");
            DGifCloseFile(gif, &error);
            return false;
        }

        std::vector<unsigned char> canvas(imageWidth * imageHeight * 4, 0);
        std::vector<unsigned char> prevCanvas(imageWidth * imageHeight * 4, 0);

        // Convert each frame
        for (int i = 0; i < gif->ImageCount; i++) {
            SavedImage* image = &gif->SavedImages[i];
            GifFrame frame;

            int transparentColor = -1;
            int disposal = 0;
            for (int j = 0; j < image->ExtensionBlockCount; j++) {
                ExtensionBlock* eb = &image->ExtensionBlocks[j];
                if (eb->Function == GRAPHICS_EXT_FUNC_CODE) {
                    disposal = (eb->Bytes[0] >> 2) & 7;
                    if (eb->Bytes[0] & 0x01)
                        transparentColor = (int)eb->Bytes[3];
                }
            }

            ColorMapObject* colorMap = image->ImageDesc.ColorMap ? image->ImageDesc.ColorMap : gif->SColorMap;

            if (i > 0) {
                if (disposal == DISPOSE_BACKGROUND) {
                    for (int y = 0; y < image->ImageDesc.Height; y++) {
                        for (int x = 0; x < image->ImageDesc.Width; x++) {
                            int idx = ((y + image->ImageDesc.Top) * imageWidth + (x + image->ImageDesc.Left)) * 4;
                            canvas[idx] = 0;
                            canvas[idx + 1] = 0;
                            canvas[idx + 2] = 0;
                            canvas[idx + 3] = 0;
                        }
                    }
                } else if (disposal == DISPOSE_PREVIOUS) {
                    canvas = prevCanvas;
                }
            }
            prevCanvas = canvas;

            for (int y = 0; y < image->ImageDesc.Height; y++) {
                for (int x = 0; x < image->ImageDesc.Width; x++) {
                    int srcIdx = y * image->ImageDesc.Width + x;
                    int dstIdx = ((y + image->ImageDesc.Top) * imageWidth + (x + image->ImageDesc.Left)) * 4;
                    int colorIndex = image->RasterBits[srcIdx];

                    if (colorIndex != transparentColor) {
                        GifColorType& color = colorMap->Colors[colorIndex];
                        canvas[dstIdx] = color.Red;
                        canvas[dstIdx + 1] = color.Green;
                        canvas[dstIdx + 2] = color.Blue;
                        canvas[dstIdx + 3] = 255;
                    }
                }
            }

            frame.data = canvas;
            frame.delay = 100; // Default delay
            for (int j = 0; j < image->ExtensionBlockCount; j++) {
                ExtensionBlock* eb = &image->ExtensionBlocks[j];
                if (eb->Function == GRAPHICS_EXT_FUNC_CODE) {
                    int delay = ((eb->Bytes[2] << 8) | eb->Bytes[1]) * 10;
                    if (delay > 0) {
                        frame.delay = delay;
                    }
                    break;
                }
            }

            if (vg) {
                frame.imageHandle = nvgCreateImageRGBA(vg, imageWidth, imageHeight, 0, frame.data.data());
            }
            gifFrames.push_back(frame);
        }

        isAnimated = gifFrames.size() > 1;

        // Initialize with first frame
        if (!gifFrames.empty()) {
            imageData = gifFrames[0].data;
            processedData = imageData;
            imagePath = path;

            if (vg) {
                outputImageHandle = nvgCreateImageRGBA(vg, imageWidth, imageHeight, 0, processedData.data());
                if (outputImageHandle == 0) {
                    INFO("GIFGlitcher: Error al crear textura principal");
                }
            }
        } else {
            INFO("GIFGlitcher: No se pudieron cargar frames del GIF");
            DGifCloseFile(gif, &error);
            return false;
        }
    }

    DGifCloseFile(gif, &error);

    // Restart worker thread
    startWorkerThread();
    textureNeedsUpdate = true;
    processRequested = true;
    processCV.notify_one();

    INFO("GIFGlitcher: GIF cargado exitosamente: %s (%d frames)",
         path.c_str(), (int)gifFrames.size());
    return true;
}

void GIFGlitcher::onReset() {
    stopWorkerThread();

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (vg && outputImageHandle) {
            nvgDeleteImage(vg, outputImageHandle);
        }
        outputImageHandle = 0;
        imageData.clear();
        processedData.clear();
        imagePath.clear();
        imageWidth = 0;
        imageHeight = 0;
        gifFrames.clear();
        currentFrame = 0;
        frameAccumulator = 0;
        isAnimated = false;
    }

    startWorkerThread();
}

void GIFGlitcher::setVG(NVGcontext* _vg) {
    vg = _vg;

    // Si hay un GIF pendiente de cargar y ahora tenemos el contexto, cargarlo
    if (hasPendingGif && vg) {
        INFO("GIFGlitcher: Cargando GIF pendiente con contexto VG disponible: %s", pendingGifPath.c_str());
        bool success = loadGif(pendingGifPath);
        INFO("GIFGlitcher: Resultado de carga pendiente: %s", success ? "éxito" : "fallido");
        hasPendingGif = false;
    }
}

json_t* GIFGlitcher::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "playbackSpeed", json_real(playbackSpeed));
    json_object_set_new(rootJ, "playbackMode", json_integer(playbackMode));

    // Guardar el path del GIF
    if (!imagePath.empty()) {
        json_object_set_new(rootJ, "imagePath", json_string(imagePath.c_str()));
    }

    return rootJ;
}

void GIFGlitcher::dataFromJson(json_t* rootJ) {
    json_t* speedJ = json_object_get(rootJ, "playbackSpeed");
    if (speedJ)
        playbackSpeed = json_real_value(speedJ);

    json_t* modeJ = json_object_get(rootJ, "playbackMode");
    if (modeJ)
        playbackMode = static_cast<PlaybackMode>(json_integer_value(modeJ));

    // Guardar el path del GIF para cargarlo cuando el contexto esté disponible
    json_t* pathJ = json_object_get(rootJ, "imagePath");
    if (pathJ) {
        pendingGifPath = json_string_value(pathJ);
        hasPendingGif = true;
        INFO("GIFGlitcher: Path de GIF guardado para carga posterior: %s", pendingGifPath.c_str());
    }
}
