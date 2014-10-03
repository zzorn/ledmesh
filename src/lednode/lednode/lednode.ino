
#include <RHDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>

#include "FastLED.h"

// Test addresses
#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2


// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806, define both DATA_PIN and CLOCK_PIN
#define LEDS_DATA_PIN1 3
#define LEDS_DATA_PIN2 4

// Singleton instance of the radio driver
RH_NRF24 driver;
// RH_NRF24 driver(8, 7);   // For RFM73 on Anarduino Mini

// Class to manage message delivery and receipt, using the driver declared above
RHDatagram manager(driver, SERVER_ADDRESS);

// How many leds in your strip?
#define NUM_LEDS1 10
#define NUM_LEDS2 10

// Define the array of leds
CRGB leds1[NUM_LEDS1];
CRGB leds2[NUM_LEDS2];

CRGB currentColor;
CRGB targetColor;
uint8_t targetMode;

#define MODE_OFF 0x00
#define MODE_LIGHT 0x01
#define MODE_COLOR 0x02
#define MODE_WAVES 0x03
#define MODE_SPARKLE 0x04

CRGB LIGHT_COLOR = CRGB(255, 240, 220);


// Dont put this on the stack:
uint8_t buffer[RH_NRF24_MAX_MESSAGE_LEN];


#define MESSAGE_SET_COLOR 0x02
#define MESSAGE_SET_MODE 0x03

long scrollSpeed_ms_per_step = 20;


unsigned long currentTime_ms = 0;
long ledCoolDown_ms = 0;


CRGB randomColor() {
   CRGB c;
   c.r = random8();   
   c.g = random8();   
   c.b = random8();   
   return c;
}


void setup() 
{
  Serial.begin(9600);
  Serial.println("Lednode starting up...");
  
  if (!manager.init()) {
    Serial.println("Radio init failed");
  }  
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm

    
  FastLED.addLeds<NEOPIXEL, LEDS_DATA_PIN1>(leds1, NUM_LEDS1);
  FastLED.addLeds<NEOPIXEL, LEDS_DATA_PIN2>(leds2, NUM_LEDS2);


  targetColor = CRGB::Red;
  currentColor = randomColor();   

  Serial.println("Lednode started.");
}



boolean checkParametrs(uint8_t command, uint8_t len, uint8_t requiredParamLength) {
  if (len < 1 + requiredParamLength) { 
    Serial.print("Too few parameters passed for message type ");    
    Serial.println(command, HEX);    
    Serial.print("Required ");    
    Serial.print(requiredParamLength);    
    Serial.print(" bytes, but got ");    
    Serial.print(len - 1);    
    Serial.println(" bytes.");    
    return false;
  }
  else {
    return true;
  }
}

void debugMessage(char* errorMessage, uint8_t messageType, uint8_t parameters[], int paramLength) {
    Serial.print(errorMessage);
    Serial.print(".  Message Type ");
    Serial.print(messageType, HEX);       
    Serial.print(", parameters: ");
    for (int i = 0; i < paramLength; i++) {
      Serial.print(parameters[i], HEX);       
      Serial.print(" ");       
    }
    Serial.print(" ( parameter length ");
    Serial.print(paramLength);
    Serial.println(" bytes)");

} 


void handleMessage(uint8_t buffer[], uint8_t length) {
  if (length < 1) {
    Serial.print("Received empty message.");    
    return;
  }
  
  uint8_t messageType = buffer[0];
  uint8_t *params = &buffer[1];

  debugMessage("Received message", messageType, params, length - 1);  
  
  switch (messageType) {
    case MESSAGE_SET_COLOR:
      if (!checkParametrs(messageType, length, 3)) return;
      targetColor.r = params[0];
      targetColor.g = params[1];
      targetColor.b = params[2];   
      currentColor = targetColor;   
      break;
    
    case MESSAGE_SET_MODE:
      if (!checkParametrs(messageType, length, 1)) return;
      targetMode = params[0];
      break;
    
    default:
      debugMessage("Unknown message type", messageType, params, length - 1);  
      break;
  }
   
}


void updateRadio(long deltaTime_ms) {
  // Check if a message is available for us
  if (manager.available()) {
    uint8_t len = sizeof(buffer);
    uint8_t from;
    if (manager.recvfrom(buffer, &len, &from)) {  
       handleMessage(buffer, len);      
    }
  }  
}


/**
 * t = from 0 (returns a) to 1024 (returns b).
 */
int mix(int32_t a, int32_t b, int32_t t) {
  return a + ((b - a) * t) >> 10;
}

/**
 * Interpolates between two colors.  
 * amount is from 0 (100% from color) to 1024 (100% to color).
 */
CRGB mixColor(CRGB from, CRGB to, int amount) {

  // Interpolate components 
  int r = mix(from.r, to.r, amount);
  int g = mix(from.g, to.g, amount);
  int b = mix(from.b, to.b, amount);

  // Clamp  
  if (r < 0) r = 0;
  else if (r > 255) r = 255;

  if (g < 0) g = 0;
  else if (g > 255) g = 255;

  if (b < 0) b = 0;
  else if (b > 255) b = 255;

  // Create color and return it
  return CRGB(r, g, b);  
}

void updateSolidColorMode(long deltaTime_ms, CRGB color) {

    // Mix towards target  
    int mixAmount = 200;
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i] = mixColor(leds1[i], color, mixAmount);
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i] = mixColor(leds2[i], color, mixAmount);
    }

}


void updateWaveMode(long deltaTime_ms) {
    // Bleed color down the line  
    int bleedAmount = 200;
    for (int i = NUM_LEDS1 - 1; i > 0; i--) {
        leds1[i] += mixColor(leds1[i], leds1[i - 1], bleedAmount);
    }
    for (int i = 0; i < NUM_LEDS2 - 1; i++) {
        leds2[i] += mixColor(leds2[i], leds2[i + 1], bleedAmount);
    }
    
    // Bleed current color to first ones
    leds1[0] = mixColor(leds1[0], currentColor, bleedAmount);
    leds2[NUM_LEDS2 - 1] = mixColor(leds2[NUM_LEDS2 - 1], currentColor, bleedAmount);

    // Dampen source color
    int colorDampening = 2;
    currentColor.fadeToBlackBy(colorDampening);

    // Dampen colors
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i].fadeToBlackBy(1);
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i].fadeToBlackBy(1);
    }

    // Reset colors if it goes to black
    if (currentColor.r == 0 && 
        currentColor.g == 0 && 
        currentColor.b == 0) {
       currentColor = targetColor;
    }
}


long sparkleIntervall_ms = 200;
long timeToNextSparkle = 1;
void updateSparkleMode(long deltaTime_ms) {

    // Check if it is time to sparkle
    timeToNextSparkle -= deltaTime_ms;
    while (timeToNextSparkle <= 0) {
      timeToNextSparkle += sparkleIntervall_ms;

      // Light random pixel       
      int array = random(2);
      if (array == 0) {
        leds1[random(NUM_LEDS1)] = targetColor;
      }
      else {
        leds2[random(NUM_LEDS2)] = targetColor;
      }           
    }
  
    // Fade to black
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i].fadeToBlackBy(1);
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i].fadeToBlackBy(1);
    }
}


void updateLeds(long deltaTime_ms) {
  // Check if it is time to update the leds yet 
  ledCoolDown_ms -= deltaTime_ms;
  if (ledCoolDown_ms <= 0) { 
    ledCoolDown_ms += scrollSpeed_ms_per_step;

    switch (targetMode) {
      case MODE_OFF:
        updateSolidColorMode(ledCoolDown_ms, CRGB::Black);
        break;
        
      case MODE_LIGHT:
        updateSolidColorMode(ledCoolDown_ms, LIGHT_COLOR);
        break;
        
      case MODE_COLOR:
        updateSolidColorMode(ledCoolDown_ms, targetColor);
        break;
        
      case MODE_WAVES:
        updateWaveMode(ledCoolDown_ms);
        break;
        
      case MODE_SPARKLE:
        updateSparkleMode(ledCoolDown_ms);
        break;
        
      default:
        // Unknown mode
        updateSolidColorMode(ledCoolDown_ms, CRGB::Red);
        break;      
    }

    // Show the updated led colors
    FastLED.show();

  }  
}




/**
 * Returns the number of milliseconds since the last call to this method.
 * Also updates the currentTime_ms variable to be the number of milliseconds since the start of the program, or the last timer wraparound (happens every 50 days or so).
 */
long calculateDeltaTime() {
  unsigned long now = millis();
  long deltaTime_ms;

  // Handle startup and millis() wraparound (happends every 50 days or so)
  if (currentTime_ms == 0 || now < currentTime_ms) {
    deltaTime_ms = 0;
  } 
  else {
    // Calculate number of milliseconds since the last call to this method
    deltaTime_ms = now - currentTime_ms;
  }
  
  // Update current time;
  currentTime_ms = now;
  
  // Return delta
  return deltaTime_ms;
}



/**
 * Main loop.
 */
void loop() {

  // Determine time step
  long deltaTime_ms = calculateDeltaTime();
    
  // Update subsystems
  updateRadio(deltaTime_ms);  
  updateLeds(deltaTime_ms);  

  
}




