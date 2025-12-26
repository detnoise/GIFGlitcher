# GIFGlitcher

**GIFGlitcher** is a powerful and versatile VCV Rack module for real-time image and GIF manipulation. Developed by DETNOISE, this 30HP module allows you to load your own images and animated GIFs and apply a wide range of glitch and color effects, all controllable via CV.

## Features

* **Load Images and GIFs:** Load PNG, JPG, and animated GIF files directly into the module.
* **Real-Time Processing:** All effects are applied in real-time, with a dedicated worker thread to prevent GUI lock-ups.
* **Extensive Effect Library:**

  * **Color Adjustments:** Brightness, Contrast, Saturation, Hue Shift.
  * **Geometric Effects:** Mirror, Flip, Partial Mirror (Horizontal and Vertical).
  * **Glitch Effects:** Slice, Artifacts, Block Size, Displacement.
  * **Data Mosh Effects:** Bit Crush, Data Shift, Pixel Sort.
  * **And more:** Sharpness, Edge Detection, RGB Aberration, Noise, Posterization, Dithering, and Interlacing.
* **CV Control:** Most parameters are controllable via CV inputs, allowing for complex and evolving visuals.
* **Trigger Inputs:** Includes `Reset` and `Random` trigger inputs for instantly resetting parameters to default or randomizing them.
* **GIF Playback Control:** Control the playback speed and mode (Forward, Ping-Pong, Random) of animated GIFs.

---

## Installation (macOS / Windows / Linux)

### Recommended: Use the precompiled plugin

You do **not** need to build GIFGlitcher yourself.

1. Go to the **Releases** section of this repository.
2. Download the `.zip` file for your platform.
3. Unzip it into your VCV Rack `plugins` folder.
4. Restart VCV Rack.

The plugin will appear automatically in the module browser.

---

## Building from source (developers only)

Only required if you want to modify or develop the plugin.

### Requirements

* **VCV Rack SDK** (official)
* A C++17-compatible compiler

> This plugin is **fully self-contained**.
> It does **not** rely on system-installed libraries such as Homebrew, pkg-config, or giflib.

---

### Build instructions

1. Download and extract the VCV Rack SDK.
2. Set the `RACK_DIR` environment variable to point to the SDK.
3. Build the plugin.

```sh
export RACK_DIR=/path/to/Rack-SDK
make
```

This will build the plugin using the official Rack toolchain.

### Packaging a distributable ZIP

```sh
make dist
```

---

## Notes on GIF support

GIF decoding is provided by a **vendored copy of giflib**, compiled directly into the plugin.
This ensures:

* Reproducible builds
* Compatibility with the Rack Plugin Toolchain
* No external system dependencies
* Fewer build issues for users and CI

---

## License

This plugin is released under a proprietary license.
Please see the `plugin.json` file and bundled license files for more details.

