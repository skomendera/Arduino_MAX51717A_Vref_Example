#include <SPI.h>
#include <Encoder.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

/*
 * OLED settings
 */

int oled_reset_pin = 4;
Adafruit_SSD1306 display(128, 32, &Wire, oled_reset_pin);

/*
 * MAX5717A settings
 */

// SPI Slave Select pin for MAX5717
int dac_ss_pin = 10;

/*
 * Encoder settings
 */

// Encoder pins
Encoder myEnc(2, 3);

long encoderNewPosition = 0;
long encoderOldPosition  = 0;
int encoderDiffPosition = 0;

/*
 * Runtime DAC settings
 */

/* 
 * Voltage coming out of the REF02 voltage reference chip 
 * This is the actual voltage measured by the reference voltmeter
 */
float referenceVoltage = 4995.20;

// Initial voltage after booting
int startingVoltage = 2500;

// Minimum and maximum allowed voltage setting
int minVoltage = 100;
int maxVoltage = 4900;

// Placeholder for the currently configured device voltage
int setVoltage;

/*
 * Control settings
 */

// Control variables for the granularity selector
int settingGranularityCounter = 0;
int settingGranularityMillis = 100;
int settingGranularityDigit = 0;

// Control variable for the enable/disable button
// Default is disabled
bool dacEnabled = false;

// Pin outputting digital HIGH signal for any buttons to pick up
int digitalHighPin = 5;

// Granularity selector button input pin
int granularitySensePin = 6;

// DAC enable/disable button input pin
int dacEnableSensePin = 7;

int previousGranularitySense = LOW;
int previousDacEnableSense = LOW;

void setup() {
  
  Serial.begin(9600);
  Serial.println("MAX5717A 16-Bit DAC");

  pinMode(dac_ss_pin, OUTPUT);
  
  pinMode(digitalHighPin, OUTPUT);
  digitalWrite(digitalHighPin, HIGH);

  pinMode(granularitySensePin, INPUT);
  pinMode(dacEnableSensePin, INPUT);

  /* 
   *  Configure SPI settings for MAX5717 and initialize SPI bus
   *  SPI mode was deducted from MAX5717 datasheet and wikipedia page for SPI
   */
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.begin();

  /* 
   *  Initialize the display
   */
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  /*
   * Establish voltage settings at boot
   */
  setVoltage = startingVoltage;
  Serial.println("Setting initial voltage to 0 mV");
  setDACVoltage(0);

}

void loop() {

  /*
   * DAC Enable/Disable button
   */

  if (digitalRead(dacEnableSensePin) == HIGH && previousDacEnableSense == LOW) {
    
    if (dacEnabled) {

      Serial.println("Disabling DAC");

      dacEnabled = false;
      setDACVoltage(0);
      
    } else {

      Serial.println("Enabling DAC");

      dacEnabled = true;
      setDACVoltage(setVoltage);
      
    }
    
    previousDacEnableSense = HIGH;
    
  }

  if (digitalRead(dacEnableSensePin) == LOW && previousDacEnableSense == HIGH) {
    previousDacEnableSense = LOW;
  }

  /*
   * Granularity selector button
   */

  if (digitalRead(granularitySensePin) == HIGH && previousGranularitySense == LOW) {
    
    toggleSettingGranularity();
    
    previousGranularitySense = HIGH;
    
  }

  if (digitalRead(granularitySensePin) == LOW && previousGranularitySense == HIGH) {
    previousGranularitySense = LOW;
  }

  /*
   * Rotary encoder
   */

  // Rotary knob uses quadrature encoding i.e. 4 positions per detent
  long encoderNewPosition = myEnc.read() / 4;
  
  if (encoderNewPosition != encoderOldPosition) {

    encoderDiffPosition = encoderNewPosition > encoderOldPosition ? 1 : -1;

    setVoltage = setVoltage + (encoderDiffPosition * settingGranularityMillis);

    if (setVoltage < minVoltage) {
      setVoltage = minVoltage;
    }

    if (setVoltage > maxVoltage) {
      setVoltage = maxVoltage;
    }

    Serial.println("Setting voltage to " + String(setVoltage) + " mV");

    if (dacEnabled) {
      setDACVoltage(setVoltage);
    }

    encoderOldPosition = encoderNewPosition;
    
  }

  updateDisplay();

}

/*
 * Iterate through the voltage setting granularity digits
 * 100 mV -> 10 mV > 1 mV -> 100 mV -> ...
 */
void toggleSettingGranularity() {

  settingGranularityCounter++;

  settingGranularityDigit = settingGranularityCounter % 3;
  settingGranularityMillis = int(pow(10, 2 - settingGranularityDigit) + 0.5);

  // Because why would something as basic as pow() work properly with mere integers
  settingGranularityMillis = int(settingGranularityMillis + 0.5);

  Serial.println("Setting granularity to " + String(settingGranularityMillis) + " mV");
  
}

/*
 * Update the OLED display based on current global settings
 */
void updateDisplay() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print(setVoltage / 1000.0, 3);
  display.println("  DAC");

  int settingHighlightStartPoint = settingGranularityDigit * 12 + 24;
  int settingHighlightEndPoint = settingGranularityDigit * 12 + 32;

  display.drawLine(settingHighlightStartPoint, 15, settingHighlightEndPoint, 15, SSD1306_WHITE);
  display.drawLine(settingHighlightStartPoint, 16, settingHighlightEndPoint, 16, SSD1306_WHITE);
  display.drawLine(settingHighlightStartPoint, 17, settingHighlightEndPoint, 17, SSD1306_WHITE);

  if (dacEnabled) {
    display.print("       ON");
  } else {
    display.print("       OFF");
  }
  
  display.display();
  
}

/*
 * Set DAC code for the desired output voltage
 */
void setDACVoltage(int voltageMillis) {

  float setDACLevel = voltageMillis / referenceVoltage * 65535;

  Serial.println("DAC code set to " + String(setDACLevel));

  unsigned int data = round(setDACLevel);

  digitalWrite(dac_ss_pin, LOW);

  SPI.transfer(highByte(data));
  SPI.transfer(lowByte(data));

  digitalWrite(dac_ss_pin, HIGH);

}
