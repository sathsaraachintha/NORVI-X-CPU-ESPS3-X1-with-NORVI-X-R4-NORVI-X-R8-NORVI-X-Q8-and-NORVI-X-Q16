#include <Wire.h>
#include <SPI.h>
#include <PCA9536D.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// --- LovyanGFX Display Configuration for NORVI X ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      
      cfg.pin_sclk = 12; 
      cfg.pin_mosi = 11; 
      cfg.pin_miso = 13; 
      cfg.pin_dc   = 46; 
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 45; 
      cfg.pin_rst          = 47; 
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true; 
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true; 
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft; 

// --- Pin Definitions ---
#define SDA_PIN 8
#define SCL_PIN 9

// PCA9536 Built-in Buttons (I2C 0x41)
#define IO_PB1  0  // "Next" Button (Not used in this single-page app)
#define IO_PB2  3  // "Toggle Outputs" Button

// ==========================================
// X-Q4 DIRECT GPIO PINS 
// (Typically shares the same 4 direct lines as the DI4)
// ==========================================
#define Q4_OUT1 5
#define Q4_OUT2 6
#define Q4_OUT3 7
#define Q4_OUT4 10
// ==========================================

// --- Objects & State Variables ---
PCA9536 io;

// Track the ON/OFF state of the Q4 (false = OFF, true = ON)
bool q4_state = false; 

bool lastPb2State = HIGH;
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize I2C (Required for the front panel buttons)
  Wire.begin(SDA_PIN, SCL_PIN);

  // --- Initialize Q4 Pins as Outputs ---
  pinMode(Q4_OUT1, OUTPUT);
  pinMode(Q4_OUT2, OUTPUT);
  pinMode(Q4_OUT3, OUTPUT);
  pinMode(Q4_OUT4, OUTPUT);

  // Ensure they are OFF on boot
  digitalWrite(Q4_OUT1, LOW);
  digitalWrite(Q4_OUT2, LOW);
  digitalWrite(Q4_OUT3, LOW);
  digitalWrite(Q4_OUT4, LOW);

  // Initialize Built-in Buttons
  if (io.begin()) {
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_PB2, INPUT);
  } else {
    Serial.println("Front panel buttons not found!");
  }

  // Initialize TFT Display
  tft.init();
  tft.setRotation(0); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2); 
}

void loop() {
  // Read the "Toggle" button
  bool currentPb2 = io.digitalRead(IO_PB2); 

  // --- Toggle Q4 Outputs ON/OFF (Button 2) ---
  if (currentPb2 == LOW && lastPb2State == HIGH) {
    // Flip the state
    q4_state = !q4_state; 
    
    // Apply the new state to all 4 physical pins
    digitalWrite(Q4_OUT1, q4_state ? HIGH : LOW);
    digitalWrite(Q4_OUT2, q4_state ? HIGH : LOW);
    digitalWrite(Q4_OUT3, q4_state ? HIGH : LOW);
    digitalWrite(Q4_OUT4, q4_state ? HIGH : LOW);
    
    delay(50); // Debounce
  }
  lastPb2State = currentPb2;

  // --- Update Display ---
  if (millis() - lastDisplayUpdate >= 100) {
    lastDisplayUpdate = millis();
    
    tft.setCursor(0, 5);
    
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println("   X-Q4 Outputs     ");
    tft.println("--------------------");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Display the current state of each pin
    tft.printf(" OUT 1: %s \n", digitalRead(Q4_OUT1) ? "ON " : "OFF");
    tft.printf(" OUT 2: %s \n", digitalRead(Q4_OUT2) ? "ON " : "OFF");
    tft.printf(" OUT 3: %s \n", digitalRead(Q4_OUT3) ? "ON " : "OFF");
    tft.printf(" OUT 4: %s \n", digitalRead(Q4_OUT4) ? "ON " : "OFF");

    for(int i=0; i<4; i++) tft.println("                    "); 
    
    // UI Navigation Hint 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 260);
    tft.println("--------------------");
    tft.println("[B2:TOGGLE ALL]     ");
  }
}