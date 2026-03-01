# JC4827W543 Internet Radio

<a href="https://www.buymeacoffee.com/thelastoutpostworkshop" target="_blank">
<img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee">
</a>

## Youtube Tutorial
[<img src="https://github.com/thelastoutpostworkshop/images/blob/main/internet_radio.png" width="500">](https://youtu.be/mnOzfRFQJIM)

## Application Description
This project turns a JC4827W543 (ESP32-S3) touchscreen board into a standalone internet radio using LVGL v9 for the interface and ESP32 Audio for stream playback.  
At startup, it connects to Wi-Fi, loads station data from an SD card JSON file, and lets the user browse/play stations from the touch UI.

## Features
- Touchscreen LVGL interface built for the 480x272 panel.
- Wi-Fi internet radio streaming.
- Station list loaded from `/radio.json` on the SD card (name, URL, description).
- Scrollable station selector (roller) with a dedicated Play button.
- Live station description area that updates when selection changes.
- On-screen volume arc control (range 0 to 21) with real-time label updates.
- Serial output for connection status and station/debug information.

## What You Can Customize
- Wi-Fi credentials in `secrets.h` (rename `secrets_rename.h` to `secrets.h` first).
- Station entries in `SD Content/radio.json`:
  - `name` (displayed in the station roller)
  - `description` (shown on the right panel)
  - `url` (stream endpoint used for playback)
- Maximum station capacity and JSON buffer limits in `JC4827W543_radio_lvgl.ino`:
  - `MAX_RADIO_SOURCES` (currently `20`)
  - `jsonRadioSourceMaxSize` (currently `4096`)
- Default startup volume in `audio.setVolume(0);`.
- Touch orientation in `touchController.setRotation(ROTATION_INVERTED);`.
