#include <Wire.h>         // <--- This fixes the "Wire not declared" error!
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
#define IO_PB1  0  // "Next" Button
#define IO_PB2  3  // "Toggle Outputs" Button

// --- Auto-Scan Variables ---
#define MAX_MODULES 4
uint8_t extModules[MAX_MODULES];       // Stores discovered addresses
uint16_t moduleStates[MAX_MODULES];    // Stores the ON/OFF state for each module
int numModulesFound = 0;

PCA9536 io;
int currentPage = 0; 
bool lastPb1State = HIGH;
bool lastPb2State = HIGH;
unsigned long lastDisplayUpdate = 0;

// --- I2C Write Helper Functions ---
void write8(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void write16(uint8_t addr, uint8_t reg, uint16_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data & 0xFF);         // LSB
  Wire.write((data >> 8) & 0xFF);  // MSB
  Wire.endTransmission();
}

// --- Your Verbose I2C Scanner (Modified to auto-detect modules) ---
void I2C_SCAN() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("Scanning I2C Bus...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      
      // Check if it's an internal NORVI CPU component or an Expansion Module
      if (address == 0x15 || address == 0x41 || address == 0x68 || address == 0x75) {
        Serial.println(" (Internal Component)");
      } else {
        Serial.println(" ! <-- Expansion Module Detected!");
        // Save the expansion module address for our UI
        if (numModulesFound < MAX_MODULES) {
          extModules[numModulesFound] = address;
          moduleStates[numModulesFound] = 0x0000; // Default to OFF
          numModulesFound++;
        }
      }
      deviceCount++;
      delay(1);
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (deviceCount == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("Scanning complete\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize the I2C bus BEFORE scanning!
  Wire.begin(SDA_PIN, SCL_PIN);

  // Run the scanner
  I2C_SCAN();

  // --- Auto-Configure Discovered Modules ---
  for (int i = 0; i < numModulesFound; i++) {
    uint8_t addr = extModules[i];
    if (addr >= 0x20 && addr <= 0x2F) {
      // It's a 16-bit module (e.g., Q16 at 0x27)
      write16(addr, 0x06, 0x0000); // Config as Output
      write16(addr, 0x02, 0x0000); // Set OFF
    } else {
      // It's an 8-bit module (e.g., R4 at 0x73, R8 at 0x71, Q8 at 0x72)
      write8(addr, 0x03, 0x00);    // Config as Output
      write8(addr, 0x01, 0x00);    // Set OFF
    }
  }

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
  bool currentPb1 = io.digitalRead(IO_PB1); // Next Page
  bool currentPb2 = io.digitalRead(IO_PB2); // Toggle Outputs

  // --- Navigate Pages (Button 1) ---
  if (currentPb1 == LOW && lastPb1State == HIGH) {
    if (numModulesFound > 0) {
      currentPage++;
      if (currentPage >= numModulesFound) currentPage = 0; 
      tft.fillScreen(TFT_BLACK); 
    }
    delay(50); 
  }
  lastPb1State = currentPb1;

  // --- Toggle Outputs ON/OFF (Button 2) ---
  if (currentPb2 == LOW && lastPb2State == HIGH) {
    if (numModulesFound > 0) {
      uint8_t addr = extModules[currentPage];
      
      // Determine if 16-bit or 8-bit module based on address range
      if (addr >= 0x20 && addr <= 0x2F) {
        // Toggle 16 bits
        moduleStates[currentPage] = (moduleStates[currentPage] == 0x0000) ? 0xFFFF : 0x0000;
        write16(addr, 0x02, moduleStates[currentPage]);
      } else {
        // Toggle 8 bits
        moduleStates[currentPage] = (moduleStates[currentPage] == 0x00) ? 0xFF : 0x00;
        write8(addr, 0x01, moduleStates[currentPage] & 0xFF);
      }
    }
    delay(50); 
  }
  lastPb2State = currentPb2;

  // --- Update Display ---
  if (millis() - lastDisplayUpdate >= 100) {
    lastDisplayUpdate = millis();
    tft.setCursor(0, 5);

    if (numModulesFound == 0) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("  No Expansion      ");
      tft.println("  Modules Found!    ");
    } else {
      displayModule(currentPage);
    }
    
    // UI Navigation Hint 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 260);
    tft.println("--------------------");
    tft.println("[B2:TOGGLE] [B1:NXT]");
  }
}

// --- Dynamic Display Function ---
void displayModule(int pageIndex) {
  uint8_t addr = extModules[pageIndex];
  uint16_t state = moduleStates[pageIndex];
  
  bool is16Bit = (addr >= 0x20 && addr <= 0x2F);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.printf(" Module at 0x%02X     \n", addr);
  tft.println("--------------------");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (is16Bit) {
    // Display 16 Channels (Q16)
    for (int i = 0; i < 8; i++) {
      bool stateA = bitRead(state, i);       
      bool stateB = bitRead(state, i + 8);   
      tft.printf(" OUT%02d:%-3s OUT%02d:%-3s\n", 
                  i + 1, stateA ? "ON" : "OFF", 
                  i + 9, stateB ? "ON" : "OFF");
    }
  } else {
    // Display 8 Channels (R4, R8, Q8)
    for (int i = 0; i < 8; i++) {
      bool chState = bitRead(state, i);
      tft.printf(" OUT %d: %s \n", i + 1, chState ? "ON " : "OFF");
    }
  }
}