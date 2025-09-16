# GIFGlitcher

**GIFGlitcher** is a powerful and versatile VCV Rack module for real-time image and GIF manipulation. Developed by DETNOISE, this 30HP module allows you to load your own images and animated GIFs and apply a wide range of glitch and color effects, all controllable via CV.

## Features

- **Load Images and GIFs:** Load PNG, JPG, and animated GIF files directly into the module.
- **Real-Time Processing:** All effects are applied in real-time, with a dedicated worker thread to prevent GUI lock-ups.
- **Extensive Effect Library:**
    - **Color Adjustments:** Brightness, Contrast, Saturation, Hue Shift.
    - **Geometric Effects:** Mirror, Flip, Partial Mirror (Horizontal and Vertical).
    - **Glitch Effects:** Slice, Artifacts, Block Size, Displacement.
    - **Data Mosh Effects:** Bit Crush, Data Shift, Pixel Sort.
    - **And more:** Sharpness, Edge Detection, RGB Aberration, Noise, Posterization, Dithering, and Interlacing.
- **CV Control:** Most parameters are controllable via CV inputs, allowing for complex and evolving visuals.
- **Trigger Inputs:** Includes `Reset` and `Random` trigger inputs for instantly resetting parameters to default or randomizing them.
- **GIF Playback Control:** Control the playback speed and mode (Forward, Ping-Pong, Random) of animated GIFs.

## Building

To build the GIFGlitcher plugin, you will need to have the VCV Rack SDK installed. Then, you can build the plugin by running the following command in the project's root directory:

```bash
make install
```

This will build the plugin and install it into your VCV Rack plugins directory.

## License

This plugin is released under a proprietary license. Please see the `plugin.json` file for more details.
