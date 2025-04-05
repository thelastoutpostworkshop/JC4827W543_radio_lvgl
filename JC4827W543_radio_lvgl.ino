// LVGLv9 for the JC4827W543 development board
// Use board "ESP32S3 Dev Module" from esp32 Arduino Core by Espressif (last tested on v3.2.0)
// Do not forget to setup and configure lv_conf.h : https://docs.lvgl.io/master/get-started/platforms/arduino.html

#include <lvgl.h>            // Install "lvgl" with the Library Manager (last tested on v9.2.2)
#include <PINS_JC4827W543.h> // Install "GFX Library for Arduino" with the Library Manager (last tested on v1.5.6)
                             // Install "Dev Device Pins" with the Library Manager (last tested on v0.0.2)
#include "TAMC_GT911.h"      // Install "TAMC_GT911" with the Library Manager (last tested on v1.0.2)
#include "Audio.h"           // Install "ESP32-audioI2S-master" with the library manager (last tested on v3.0.13)
#include <ArduinoJson.h>     // Install "ArduinoJson" with the library manager (last tested on v7.3.1)
#include <SD_MMC.h>          // Included with the Espressif Arduino Core (last tested on v3.2.0)
#include "WiFi.h"            // Included with the Espressif Arduino Core (last tested on v3.2.0)
#include "secrets.h"         // Rename secrets_rename.h to secrets.h and add your SSID and password for your Wifi network
#include "Arduino.h"

// Touch Controller
#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38
#define TOUCH_WIDTH 480
#define TOUCH_HEIGHT 272
TAMC_GT911 touchController = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

// Display global variables
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

// Radio sources global variables
#define jsonRadioSourceMaxSize 4096
#define MAX_RADIO_SOURCES 20
String radioUrlsArray[MAX_RADIO_SOURCES];         // Stores the radio station URLs
String radioNamesArray[MAX_RADIO_SOURCES];        // Stores the radio station names
String radioDescriptionsArray[MAX_RADIO_SOURCES]; // Stores the radio station descriptions
int radioSourcesCount = 0;                        // Count of radio sources

// LCGL Widget global variables
lv_obj_t *rollerWidget = NULL; // Global pointer to the roller widget
lv_obj_t *descriptionLabel;    // Global pointer for the description label

Audio audio; // Audio global variable
String radioOptions = "";

const char *root = "/root"; // Do not change this, it is needed to access files properly on the SD card

void setup()
{
  Serial.begin(115200);
  Serial.println("Arduino_GFX LVGL_Arduino_v9 example ");
  String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  // SD Card initialization
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */);
  if (!SD_MMC.begin(root, true /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_DEFAULT))
  {
    Serial.println("ERROR: SD Card mount failed!");
    while (true)
    {
      /* no need to continue */
    }
  }

  // Connect to Wi-Fi
  connectToWiFi();
  readRadioSources();

  // Audio setup
  audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  audio.setVolume(0); // default 0...21

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
    while (true)
    {
      /* no need to continue */
    }
  }
  // Set the backlight of the screen to High intensity
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  gfx->fillScreen(RGB565_BLACK);

  // Init touch device
  touchController.begin();
  touchController.setRotation(ROTATION_INVERTED); // Change as needed

  lv_init();                      // init LVGL
  lv_tick_set_cb(lvgl_millis_cb); // Set a tick source so that LVGL will know how much time elapsed

  // register print function for debugging
#if LV_USE_LOG != 0
  lv_log_register_print_cb(lvgl_print);
#endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();
  bufSize = screenWidth * 40;

  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf)
  {
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
    while (true)
    {
      /* no need to continue */
    }
  }

  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, lvgl_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Create input device (touchpad of the JC4827W543)
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read);

  createLabelLVGLVersion();

  descriptionLabel = lv_label_create(lv_scr_act());
  lv_obj_set_width(descriptionLabel, 190);
  lv_label_set_long_mode(descriptionLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(descriptionLabel, LV_ALIGN_TOP_RIGHT, -10, 10);
  lv_label_set_text(descriptionLabel, "Station description will appear here");

  // Create the roller and capture its pointer.
  lv_obj_t *roller = createRollerWidget();

  createPlayButtonWidget(roller);

  createVolumeWidget();

  Serial.println("Setup done");
}

void loop()
{
  lv_task_handler(); // let the GUI do its work
  audio.loop();      // let the Audio do its work
  vTaskDelay(1);
}

// Play a radio station stream using its URL
void playRadioStationStream(const char *radioUrl)
{
  Serial.printf("Connection to station %s\n", radioUrl);
  audio.connecttohost(radioUrl);
}

// Connect to Wi-Fi
void connectToWiFi()
{
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
}

// Read the json radio source from the SD Card
void readRadioSources()
{
  File file = SD_MMC.open("/radio.json");
  if (!file)
  {
    Serial.println("Failed to open radio.json");
    return;
  }

  size_t size = file.size();
  if (size > jsonRadioSourceMaxSize)
  {
    Serial.println("radio.json is too large");
    file.close();
    return;
  }

  std::unique_ptr<char[]> buf(new char[size + 1]);
  file.readBytes(buf.get(), size);
  buf[size] = '\0';
  file.close();

  DynamicJsonDocument doc(jsonRadioSourceMaxSize);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray sources = doc["radioSources"].as<JsonArray>();
  radioOptions = "";
  radioSourcesCount = 0; // Reset counter

  for (JsonObject src : sources)
  {
    const char *name = src["name"].as<const char *>();
    const char *url = src["url"].as<const char *>();
    const char *description = src["description"].as<const char *>();
    if (name && url && description && radioSourcesCount < MAX_RADIO_SOURCES)
    {
      // Build the options string with names (separated by newline)
      radioOptions += name;
      radioOptions += "\n";

      // Store the name, URL, and description in parallel arrays.
      radioNamesArray[radioSourcesCount] = String(name);
      radioUrlsArray[radioSourcesCount] = String(url);
      radioDescriptionsArray[radioSourcesCount] = String(description);
      radioSourcesCount++;
    }
  }

  if (radioOptions.endsWith("\n"))
  {
    radioOptions.remove(radioOptions.length() - 1);
  }

  // Print the concatenated radio options (names)
  Serial.println("Radio options loaded:");
  Serial.println(radioOptions);

  // Print detailed information (names, URLs, and descriptions) for each radio source
  Serial.println("Detailed radio source information:");
  for (int i = 0; i < radioSourcesCount; i++)
  {
    Serial.print("Station ");
    Serial.print(i);
    Serial.print(": Name = ");
    Serial.print(radioNamesArray[i]);
    Serial.print(", URL = ");
    Serial.print(radioUrlsArray[i]);
    Serial.print(", Description = ");
    Serial.println(radioDescriptionsArray[i]);
  }
}

// Display the volume control widget
void createVolumeWidget()
{
  // Create a volume arc widget
  lv_obj_t *volume_arc = lv_arc_create(lv_scr_act());
  lv_obj_set_size(volume_arc, 150, 150);

  // Set the arc's rotation and background angles so the gauge has a nice appearance.
  lv_arc_set_rotation(volume_arc, 135);
  lv_arc_set_bg_angles(volume_arc, 0, 270);

  // Set the range of the arc to match the audio volume range (0 to 21).
  lv_arc_set_range(volume_arc, 0, 21);

  // Set the initial value of the arc using the current volume.
  lv_arc_set_value(volume_arc, audio.getVolume());

  // Align the arc widget at the bottom right of the screen.
  lv_obj_align(volume_arc, LV_ALIGN_BOTTOM_RIGHT, -30, -10);

  // Create a label to display the current volume value.
  lv_obj_t *volume_label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(volume_label, "Volume: %d", audio.getVolume());
  // Align the label above the volume arc.
  lv_obj_align_to(volume_label, volume_arc, LV_ALIGN_CENTER, 0, 0);

  // Attach the volume event callback to the arc.
  // The volume_label is passed as user data so the callback can update it.
  lv_obj_add_event_cb(volume_arc, lvgl_volume_event_cb, LV_EVENT_VALUE_CHANGED, volume_label);
}

// Display the play button
void createPlayButtonWidget(lv_obj_t *roller)
{
  // Create the button widget.
  lv_obj_t *btn = lv_button_create(lv_scr_act());
  // Align the button below the roller (with a 10-pixel vertical offset).
  lv_obj_set_size(btn, 120, 50);
  lv_obj_align_to(btn, roller, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  lv_obj_add_event_cb(btn, lvgl_play_btn_event_cb, LV_EVENT_ALL, NULL);

  // Add a label to the button.
  lv_obj_t *btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "Play");
  lv_obj_center(btn_label);
}

// Display a label showing the LVGL version currently used
void createLabelLVGLVersion()
{
  // Create some widgets to see if everything is working
  lv_obj_t *title_label = lv_label_create(lv_screen_active());
  lv_label_set_text(title_label, "LVGL(V" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR) "." GFX_STR(LVGL_VERSION_PATCH) ")");
  lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// LVGL calls this function to print log information
void lvgl_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// LVGL calls this function to retrieve elapsed time
uint32_t lvgl_millis_cb(void)
{
  return millis();
}

// LVGL calls this function when a rendered image needs to copied to the display
void lvgl_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

  lv_disp_flush_ready(disp);
}

// LVGL calls this function to read the touchpad
void lvgl_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  // Update the touch data from the GT911 touch controller
  touchController.read();

  // If a touch is detected, update the LVGL data structure with the first point's coordinates.
  if (touchController.isTouched && touchController.touches > 0)
  {
    data->point.x = touchController.points[0].x;
    data->point.y = touchController.points[0].y;
    data->state = LV_INDEV_STATE_PRESSED; // Touch is pressed
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED; // No touch detected
  }
}

// LVGL calls this function when the play button is pressed
static void lvgl_play_btn_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    // Retrieve the selected index from the roller widget.
    int sel = lv_roller_get_selected(rollerWidget);
    Serial.print("Button pressed, selected index: ");
    Serial.println(sel);

    // Validate the index.
    if (sel < 0 || sel >= radioSourcesCount)
    {
      Serial.println("Invalid selection, using default radio station.");
      sel = 0;
    }
    // Launch the radio stream corresponding to the selected URL.
    playRadioStationStream(radioUrlsArray[sel].c_str());
  }
}

// LVGL calls this function when the user change radio station with the roller widget
static void lvgl_roller_event_handler(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
  {
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    int selectedIndex = lv_roller_get_selected(roller);
    Serial.print("Selected radio station index: ");
    Serial.println(selectedIndex);

    if (selectedIndex >= 0 && selectedIndex < radioSourcesCount)
    {
      lv_label_set_text(descriptionLabel, radioDescriptionsArray[selectedIndex].c_str());
    }
    else
    {
      lv_label_set_text(descriptionLabel, "No description available");
    }
  }
}

// LVGL calls this function when the user change the volume
static void lvgl_volume_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
  {
    // Get the arc widget and its current value.
    lv_obj_t *arc = lv_event_get_target_obj(e);
    int vol = lv_arc_get_value(arc);

    // Set the audio volume.
    audio.setVolume(vol);
    Serial.printf("Volume set to: %d\n", vol);

    // Retrieve the volume label from the event's user data and update its text.
    lv_obj_t *vol_label = (lv_obj_t *)lv_event_get_user_data(e);
    if (vol_label)
    {
      lv_label_set_text_fmt(vol_label, "Volume: %d", vol);
    }
  }
}

// Function to create the roller widget and return its pointer.
lv_obj_t *createRollerWidget()
{
  rollerWidget = lv_roller_create(lv_scr_act());
  lv_roller_set_options(rollerWidget, radioOptions.c_str(), LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(rollerWidget, 4);

  // Align the roller to the top left with a margin.
  lv_obj_align(rollerWidget, LV_ALIGN_TOP_LEFT, 10, 40);
  lv_obj_add_event_cb(rollerWidget, lvgl_roller_event_handler, LV_EVENT_ALL, NULL);

  // Create a label above the roller.
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Choose your radio station:");
  lv_obj_align_to(label, rollerWidget, LV_ALIGN_OUT_TOP_MID, 0, -10);

  return rollerWidget;
}
