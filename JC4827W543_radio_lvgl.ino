// LVGLv9 for the JC4827W543 development board
// Use board "ESP32S3 Dev Module" from esp32 Arduino Core by Espressif (last tested on v3.2.0)
// Do not forget to configure to setup and configure lv_conf.h : https://docs.lvgl.io/master/get-started/platforms/arduino.html

#include <lvgl.h>            // Install "lvgl" with the Library Manager (last tested on v9.2.2)
#include <PINS_JC4827W543.h> // Install "GFX Library for Arduino" with the Library Manager (last tested on v1.5.6)
                             // Install "Dev Device Pins" with the Library Manager (last tested on v0.0.2)
#include "TAMC_GT911.h"      // Install "TAMC_GT911" with the Library Manager (last tested on v1.0.2)
#include "Audio.h"           // Install "ESP32-audioI2S-master" with the library manager (last tested on v3.0.13)
#include <ArduinoJson.h>     // Install "ArduinoJson" with the library manager (last tested on v7.3.1)
#include <SD_MMC.h>          // Included with the Espressif Arduino Core (last tested on v3.2.0)
#include "WiFi.h"            // Included with the Espressif Arduino Core (last tested on v3.2.0)
#include "secrets.h"         // Rename secrets_rename.h to secrets.h and add your SSID and password for your Wifi network

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

Audio audio; // Audio global variable
String radioOptions = "";

const char *root = "/root"; // Do not change this, it is needed to access files properly on the SD card

// LVGL calls this function to print log information
void my_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// LVGL calls this function to retrieve elapsed time
uint32_t millis_cb(void)
{
  return millis();
}

// LVGL calls this function when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

  lv_disp_flush_ready(disp);
}

// LVGL calls this function to read the touchpad
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
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

static void btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  if (code == LV_EVENT_CLICKED)
  {
    static uint8_t cnt = 0;
    cnt++;

    /*Get the first child of the button which is the label and change its text*/
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text_fmt(label, "Button: %d", cnt);
  }
}

static void value_changed_event_cb(lv_event_t *e)
{
  lv_obj_t *arc = lv_event_get_target_obj(e);
  lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

  lv_label_set_text_fmt(label, "%" LV_PRId32 "%%", lv_arc_get_value(arc));

  /*Rotate the label to the current position of the arc*/
  lv_arc_rotate_obj_to_angle(arc, label, 25);
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

void readRadioJson() {
  File file = SD_MMC.open("/radio.json");
  if (!file) {
    Serial.println("Failed to open radio.json");
    return;
  }

  size_t size = file.size();
  if (size > 1024) {
    Serial.println("radio.json is too large");
    file.close();
    return;
  }
  
  std::unique_ptr<char[]> buf(new char[size + 1]);
  file.readBytes(buf.get(), size);
  buf[size] = '\0';
  file.close();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }
  
  JsonArray sources = doc["radioSources"].as<JsonArray>();
  radioOptions = "";
  for (JsonObject src : sources) {
    radioOptions += src["name"].as<const char*>();
    radioOptions += "\n";
  }
  
  if (radioOptions.endsWith("\n")) {
    radioOptions.remove(radioOptions.length() - 1);
  }
  
  Serial.println("Radio options loaded:");
  Serial.println(radioOptions);
}

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
  readRadioJson();

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

  // init LVGL
  lv_init();

  // Set a tick source so that LVGL will know how much time elapsed
  lv_tick_set_cb(millis_cb);

  // register print function for debugging
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
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
  else
  {
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create input device (touchpad of the JC4827W543)
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Create some widgets to see if everything is working
    lv_obj_t *title_label = lv_label_create(lv_screen_active());
    lv_label_set_text(title_label, "Hello Arduino, I'm LVGL!(V" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR) "." GFX_STR(LVGL_VERSION_PATCH) ")");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Button Widget
    lv_obj_t *btn = lv_button_create(lv_screen_active());       /*Add a button to the current screen*/
    lv_obj_set_pos(btn, 10, 10);                                /*Set its position*/
    lv_obj_set_size(btn, 120, 50);                              /*Set its size*/
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL); /*Assign a callback to the button*/

    lv_obj_t *btn_label = lv_label_create(btn); /*Add a label to the button*/
    lv_label_set_text(btn_label, "Button");     /*Set the label's text*/
    lv_obj_center(btn_label);

    createRollerWidget();
    // LV_IMAGE_DECLARE(img_cogwheel_argb);

    // // Arc Widget
    // lv_obj_t * label = lv_label_create(lv_screen_active());

    // lv_obj_t *arc = lv_arc_create(lv_screen_active());
    // lv_obj_set_size(arc, 150, 150);
    // lv_arc_set_rotation(arc, 135);
    // lv_arc_set_bg_angles(arc, 0, 270);
    // lv_arc_set_value(arc, 10);
    // lv_obj_center(arc);
    // lv_obj_add_event_cb(arc, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, label);

    // // Manually update the label for the first time
    // lv_obj_send_event(arc, LV_EVENT_VALUE_CHANGED, NULL);
  }

  Serial.println("Setup done");
}

static void roller_event_handler(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_VALUE_CHANGED) {
      // Explicitly cast the returned void pointer to lv_obj_t*
      lv_obj_t * roller = (lv_obj_t *) lv_event_get_target(e);
      char buf[32];
      lv_roller_get_selected_str(roller, buf, sizeof(buf));
      Serial.print("Selected radio station: ");
      Serial.println(buf);
  }
}

void createRollerWidget() {
  // Create a roller widget on the active screen.
  lv_obj_t * roller1 = lv_roller_create(lv_screen_active());
  
  // Set the options using the radioOptions string obtained from radio.json.
  // The LV_ROLLER_MODE_INFINITE flag makes the options scroll infinitely.
  lv_roller_set_options(roller1, radioOptions.c_str(), LV_ROLLER_MODE_INFINITE);
  
  // Define the number of visible rows in the roller.
  lv_roller_set_visible_row_count(roller1, 4);
  
  // Center the roller widget on the screen.
  lv_obj_center(roller1);
  
  // Attach the event callback to handle interactions.
  lv_obj_add_event_cb(roller1, roller_event_handler, LV_EVENT_ALL, NULL);
}

void loop()
{
  lv_task_handler(); /* let the GUI do its work */

#ifdef DIRECT_MODE
#if defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)
  gfx->flush();
#else  // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#endif // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
#else  // !DIRECT_MODE
#ifdef CANVAS
  gfx->flush();
#endif
#endif // !DIRECT_MODE

  delay(5);
}
