
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

#define NUM_LEDSTRIPS 2

// Define the array of leds
CRGB leds1[NUM_LEDS1];
CRGB leds2[NUM_LEDS2];

CRGB* ledStrips[NUM_LEDSTRIPS];

uint16_t ledStripSizes[NUM_LEDSTRIPS];

CRGB currentColor;
CRGB targetColor;
uint8_t targetMode;

#define MODE_OFF 0x00
#define MODE_LIGHT 0x01
#define MODE_COLOR 0x02
#define MODE_WAVES 0x03
#define MODE_SPARKLE 0x04

CRGB LIGHT_COLOR = CRGB(150, 190, 255);


// Dont put this on the stack:
uint8_t buffer[RH_NRF24_MAX_MESSAGE_LEN];


#define MESSAGE_SET_COLOR 0x02
#define MESSAGE_SET_MODE 0x03

long scrollSpeed_ms_per_step = 20;

boolean enableParticles = false;

unsigned long currentTime_ms = 0;
long ledCoolDown_ms = 0;

#define SUBPOS_RESOLUTION 10000

struct Particle {
  boolean active = false;
  uint8_t ledStrip;
  uint16_t pos;  
  CRGB color = CRGB::White;
  int numTargets = 0;
  int currentTarget = 0;
  CRGB *targetColors = NULL;
  long *colorFadeTimes_ms = NULL;    
  long currentFadeDuration_ms = 0;
  long age_ms = 0;
  
  int velocity = 0; // Sub pos changes per ms.
  int subPos = 0; // +/- SUBPOS_RESOLUTION
};


#define NUM_PARTICLES 32

Particle particles[NUM_PARTICLES];


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
  Serial.flush();

  ledStrips[0] = &leds1[0];
  ledStrips[1] = &leds2[0];
  ledStripSizes[0] = NUM_LEDS1;
  ledStripSizes[1] = NUM_LEDS2;

  
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
      Serial.print("Mode changed to ");    
      Serial.println(targetMode);    

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

    // Set to target  
    int mixAmount = 200;
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i] = color;
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i] = color;
    }

}


int findEmptyParticle() {
  // First try to find inactive
  for (int i = 0; i < NUM_PARTICLES; i++) {
     if (!particles[i].active) return i;
  } 
  
  // If not found, take oldest
  long oldestAge = -1;
  int oldestParticle = 0;
  for (int i = 0; i < NUM_PARTICLES; i++) {
     if (particles[i].age_ms > oldestAge) {
       oldestParticle = i;
       oldestAge = particles[i].age_ms;
     }
  } 
  
  return oldestParticle;
}

int addParticle(int ledStrip, int pos, int velocity, CRGB *colors, long *times, int num) {
  int i = findEmptyParticle();
  particles[i].active = true;
  
  particles[i].ledStrip = ledStrip;
  particles[i].pos = pos;
  particles[i].velocity = velocity;
  particles[i].subPos = 0;
  particles[i].color = colors[0];
  particles[i].targetColors = colors;
  particles[i].colorFadeTimes_ms = times;
  particles[i].numTargets = num;
  particles[i].currentFadeDuration_ms = 0;
  particles[i].age_ms = 0;
  
  return i;  
}

void updateParticles(long deltaTime_ms) {
  if (!enableParticles) return;
  
  for (int i = 0; i < NUM_PARTICLES; i++) { 
    if (particles[i].active) {
      // Update color fade 
      int target = particles[i].currentTarget;
      if (target >= particles[i].numTargets) particles[i].active = false;
      else {
        // Update age
        particles[i].age_ms += deltaTime_ms;
        
        // Calculate color
        CRGB sourceColor;
        if (particles[i].currentTarget <= 0) sourceColor =  particles[i].color;
        else sourceColor = particles[i].targetColors[target - 1];
        CRGB targetColor = particles[i].targetColors[target];
        particles[i].color = mixColor(sourceColor, targetColor, (1024L * (particles[i].colorFadeTimes_ms[target] - particles[i].currentFadeDuration_ms)) / particles[i].colorFadeTimes_ms[target]);
        
        // Update ledstrip
        ledStrips[particles[i].ledStrip][particles[i].pos] = particles[i].color;

        // Update fade
        particles[i].currentFadeDuration_ms += deltaTime_ms;
        long phaseDuration = particles[i].colorFadeTimes_ms[target];
        if (particles[i].currentFadeDuration_ms >= phaseDuration) {
          // Move to next phase
          particles[i].currentFadeDuration_ms = 0;
          particles[i].currentTarget++;
          if (particles[i].currentTarget >= particles[i].numTargets) particles[i].active = false;
        } 

/*      
        // Update pos
        particles[i].subPos += particles[i].velocity * deltaTime_ms;
        if (particles[i].subPos >= SUBPOS_RESOLUTION) {
          particles[i].pos++;
          particles[i].subPos = 0;
        }
        else if (particles[i].subPos <= -SUBPOS_RESOLUTION) {
          particles[i].pos--;
          particles[i].subPos = 0;
        }

        // Kill if it went outide
        if (particles[i].pos >= ledStripSizes[particles[i].ledStrip]) particles[i].active = false;
        if (particles[i].pos < 0) particles[i].active = false;
        // if (particles[i].pos >= ledStripSizes[particles[i].ledStrip]) particles[i].pos = 0;
        // else if (particles[i].pos < 0]) particles[i].pos =  ledStripSizes[particles[i].ledStrip] - 1;
*/        
      }    
    }
  }
  
  
}


long waveSpeed = 16;
long wavePos = 0;
void updateWaveMode(long deltaTime_ms) {
    // Calculate source colors
    wavePos += deltaTime_ms * waveSpeed;
    if (wavePos > 32767) wavePos -= 32767;
    int pos = sin16(wavePos * waveSpeed) / (65535 / 512) + 512;
    CRGB sourceColor1 = mixColor(targetColor, CRGB::Black, pos);
    CRGB sourceColor2 = mixColor(targetColor, CRGB::Black, 1024 - pos);
  
    // Bleed color down the line  
    int bleedAmount = 300;
    for (int i = NUM_LEDS1 - 1; i > 0; i--) {
        leds1[i] += mixColor(leds1[i], leds1[i - 1], bleedAmount);
    }
    for (int i = 0; i < NUM_LEDS2 - 1; i++) {
        leds2[i] += mixColor(leds2[i], leds2[i + 1], bleedAmount);
    }
    
    // Bleed current color to first ones
    leds1[0] = mixColor(leds1[0], sourceColor1, bleedAmount);
    leds2[NUM_LEDS2 - 1] = mixColor(leds2[NUM_LEDS2 - 1], sourceColor2, bleedAmount);

    // Dampen colors
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i].fadeToBlackBy(1);
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i].fadeToBlackBy(1);
    }
}


long sparkleIntervall_ms = 200;
long timeToNextSparkle = 1;
CRGB GRADIENT_FIRE_COLORS[] = {CRGB(255, 240, 150), CRGB(220, 180, 0), CRGB(200, 0, 0), CRGB(30, 0, 50), CRGB(0, 0, 60), CRGB(0, 0, 0)};
long GRADIENT_FIRE_DURATIONS[] = {500, 1000, 500, 2000, 100, 3000};
void updateSparkleMode(long deltaTime_ms) {

    // Check if it is time to sparkle
    timeToNextSparkle -= deltaTime_ms;
    while (timeToNextSparkle <= 0) {
      timeToNextSparkle += sparkleIntervall_ms;
      int ledStrip = random(NUM_LEDSTRIPS);
      int pos = random(ledStripSizes[ledStrip]);
      int velocity = random(400) -200;
      
      addParticle(ledStrip, pos, velocity, GRADIENT_FIRE_COLORS, GRADIENT_FIRE_DURATIONS, 6);

      
/*
      // Light random pixel       
      int array = random(2);
      if (array == 0) {
        leds1[random(NUM_LEDS1)] = targetColor;
      }
      else {
        leds2[random(NUM_LEDS2)] = targetColor;
      }           
*/
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
        enableParticles = false;
        updateSolidColorMode(ledCoolDown_ms, CRGB::Black);
        break;
        
      case MODE_LIGHT:
        enableParticles = false;
        updateSolidColorMode(ledCoolDown_ms, LIGHT_COLOR);
        break;
        
      case MODE_COLOR:
        enableParticles = false;
        updateSolidColorMode(ledCoolDown_ms, targetColor);
        break;
        
      case MODE_WAVES:
        enableParticles = true;
        updateWaveMode(ledCoolDown_ms);
        break;
        
      case MODE_SPARKLE:
        enableParticles = true;
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

  updateParticles(deltaTime_ms); 
  
}




