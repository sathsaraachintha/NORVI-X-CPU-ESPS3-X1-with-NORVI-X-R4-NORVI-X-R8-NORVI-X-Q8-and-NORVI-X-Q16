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
#define IO_PB1  0  // Not used in this single-page code
#define IO_PB2  3  // "Toggle Outputs" Button

// ==========================================
// Q8 EXPANSION MODULE ADDRESS
// Change this if your scanner shows a different address!
// ==========================================
#define Q8_ADDR 0x72 
// ==========================================

// --- Objects & State Variables ---
PCA9536 io;
uint8_t q8_state = 0x00; // Track ON/OFF state (0x00 = OFF, 0xFF = ON)

bool lastPb2State = HIGH;
unsigned long lastDisplayUpdate = 0;

// --- I2C Write Helper Function ---
void write8(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

// --- I2C Scanner Function ---
void I2C_SCAN() {
    byte error, address;
    int deviceCount = 0;
    Serial.println("Scanning I2C Bus...");
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            deviceCount++;
            delay(1);
        }
    }
    if (deviceCount == 0) Serial.println("No I2C devices found\n");
    else Serial.println("Scanning complete\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  // Run scanner on boot to help you verify the address
  I2C_SCAN();

  // --- Initialize Q8 Module as Output ---
  // PCA9538 Config Register is 0x03. Write 0x00 to set all pins to Output.
  write8(Q8_ADDR, 0x03, 0x00);
  
  // Ensure all outputs are OFF on boot (Output Register 0x01)
  write8(Q8_ADDR, 0x01, 0x00);

  // Initialize Built-in Buttons
  if (io.begin()) {
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_PB2, INPUT);
  }

  // Initialize TFT Display
  tft.init();
  tft.setRotation(0); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2); 
}

void loop() {
  bool currentPb2 = io.digitalRead(IO_PB2); // Toggle Outputs

  // --- Toggle Outputs ON/OFF (Button 2) ---
  if (currentPb2 == LOW && lastPb2State == HIGH) {
    // Flip all 8 bits from 00000000 (OFF) to 11111111 (ON)
    q8_state = (q8_state == 0x00) ? 0xFF : 0x00; 
    write8(Q8_ADDR, 0x01, q8_state);
    delay(50); // Debounce
  }
  lastPb2State = currentPb2;

  // --- Update Display ---
  if (millis() - lastDisplayUpdate >= 100) {
    lastDisplayUpdate = millis();
    tft.setCursor(0, 5);

    displayQ8();
    
    // UI Navigation Hint 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 260);
    tft.println("--------------------");
    tft.println("[B2:TOGGLE ALL]     ");
  }
}

// --- Display Function ---
void displayQ8() {
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.println("   X-Q8 Outputs     ");
  tft.println("--------------------");

  // Check if the module is actually connected at Q8_ADDR
  Wire.beginTransmission(Q8_ADDR);
  if (Wire.endTransmission() != 0) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println(" Module Not Found!  ");
    tft.println(" Check Address!     ");
    for(int i=0; i<6; i++) tft.println("                    "); 
    return;
  }

  // If found, display the states
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < 8; i++) {
    bool state = bitRead(q8_state, i);
    tft.printf(" OUT %d: %s \n", i + 1, state ? "ON " : "OFF");
  }
}