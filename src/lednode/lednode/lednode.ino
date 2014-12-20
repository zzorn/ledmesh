
#include <RHDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>

#include "FastLED.h"


// Pins
#define PIN_LEDSTRIP1 2
#define PIN_LEDSTRIP2 4
#define PIN_LEDSTRIP3 7
#define PIN_LEDSTRIP4 8

#define PIN_SPOTLIGHT1 3
#define PIN_SPOTLIGHT2 5
#define PIN_SPOTLIGHT3 6
#define PIN_SPOTLIGHT4 9
#define PIN_SPOTLIGHT5 10
#define PIN_SPOTLIGHT6 A3

#define PIN_RADIO_ENABLE A1
#define PIN_RADIO_CSN A0
#define PIN_RADIO_CLOCK 13
#define PIN_RADIO_MISO 12
#define PIN_RADIO_MOSI 11

#define PIN_SOUND_ANALOG A2
#define PIN_LIGHTLEVEL A7
#define PIN_PROXIMITY A6

#define PIN_EXTRAIO1 A4
#define PIN_EXTRAIO2 A5

#define LEDSTRIP_COUNT 4

int ledstripPins[] = {
  PIN_LEDSTRIP1,
  PIN_LEDSTRIP2,
  PIN_LEDSTRIP3,
  PIN_LEDSTRIP4
};

#define SPOTLIGHT_COUNT 6

int spotlightPins[] = {
  PIN_SPOTLIGHT1,
  PIN_SPOTLIGHT2,
  PIN_SPOTLIGHT3,
  PIN_SPOTLIGHT4,
  PIN_SPOTLIGHT5,
  PIN_SPOTLIGHT6
};


// Address of this node
// TODO: This needs to be incrementally defined for each node when programming them?  Or look for a free address and save / load it from eprom
#define DEFAULT_NODE_ADDRESS 99
uint8_t thisNodeAddress = DEFAULT_NODE_ADDRESS;

// Singleton instance of the radio driver
RH_NRF24 driver(PIN_RADIO_ENABLE, PIN_RADIO_CSN);

// Class to manage message delivery and receipt, using the driver declared above
RHDatagram manager(driver, thisNodeAddress);


void printMessage(char* message) {
  Serial.println(message);
//  Serial.flush();
}

void printValue(char* message, int value) {
  Serial.print(message);
  Serial.println(value, DEC);
//  Serial.flush();
}


// Array with current led colors
#define MAX_LED_COUNT 100
#define DEFAULT_LED_COUNT 20
CRGB ledColors[MAX_LED_COUNT];
byte ledAlpha[MAX_LED_COUNT];

short nextFreeLed = 0;

//--------------------------------------------------------
// Holds data for one led strip
class LedStrip {
  
  short ledCount;
  short startLed;  

  public:
  void init(byte pin_, byte ledCount_) {
    
    startLed = nextFreeLed;
    
    int numAvailable = MAX_LED_COUNT - nextFreeLed;
    if (numAvailable <= 0) {
      // No free leds left
      ledCount = 0;
    }
    else if (numAvailable < ledCount_) {
      // Not enough free leds left
      ledCount = numAvailable;
    }
    else {    
      ledCount = ledCount_;
    }
    
    nextFreeLed += ledCount;

    printValue("pin ", pin_);
    printValue("startLed ", startLed);
    printValue("ledCount ", ledCount);
    printValue("nextFeeLed ", nextFreeLed);
    
    // Register with fastled library
    if (ledCount > 0) {    
      CRGB* colors = &ledColors[nextFreeLed];

      // Kludge to manage pins that need to be passed in as constants
      switch(pin_) {
        case PIN_LEDSTRIP1: FastLED.addLeds<NEOPIXEL, PIN_LEDSTRIP1>(colors, ledCount); break;
        case PIN_LEDSTRIP2: FastLED.addLeds<NEOPIXEL, PIN_LEDSTRIP2>(colors, ledCount); break;
        case PIN_LEDSTRIP3: FastLED.addLeds<NEOPIXEL, PIN_LEDSTRIP3>(colors, ledCount); break;
        case PIN_LEDSTRIP4: FastLED.addLeds<NEOPIXEL, PIN_LEDSTRIP4>(colors, ledCount); break;
        default: printValue("Unsupported led strip pin ", pin_); break;
      }
    }
  }  
  
};

LedStrip ledStrips[LEDSTRIP_COUNT];

//--------------------------------------------------------
// Handles a wave that moves along a ledstrip and updates led colors
class Wave {
  LedStrip* ledStrip;

  short pos;
  byte length;

  // Movement speed (negative to left, positive to right)
  short speed_millisecondsPerLed
  
  



}







//--------------------------------------------------------

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


void setup()  {
  Serial.begin(9600);

  Serial.println("Initializing");
  printMessage("Lednode starting up...");

  // Setup pins
  printMessage("Setting up pins");
  for (int i = 0; i < LEDSTRIP_COUNT; i++) {
     pinMode(ledstripPins[i], OUTPUT);
  }
  for (int i = 0; i < SPOTLIGHT_COUNT; i++) {
     pinMode(spotlightPins[i], OUTPUT);
  }
  pinMode(PIN_RADIO_ENABLE, OUTPUT);
  pinMode(PIN_RADIO_CSN, OUTPUT);
  pinMode(PIN_RADIO_CLOCK, OUTPUT);
  pinMode(PIN_RADIO_MOSI, OUTPUT);
  pinMode(PIN_RADIO_MISO, INPUT);

  pinMode(PIN_SOUND_ANALOG, INPUT);
  pinMode(PIN_LIGHTLEVEL, INPUT);
  pinMode(PIN_PROXIMITY, INPUT);

  pinMode(PIN_EXTRAIO1, INPUT);
  pinMode(PIN_EXTRAIO2, INPUT);

  // Setup ledstrips
  Serial.println("Setting up ledstrips");
  for (int i = 0; i < LEDSTRIP_COUNT; i++) {
      ledStrips[i].init(ledstripPins[i], DEFAULT_LED_COUNT);
  }

/*
  // Init radio  
  printMessage("Setting up radio");
  if (!manager.init()) {
    Serial.println("Radio init failed");
  }  
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm

    
  targetColor = CRGB::Red;
  currentColor = randomColor();   
*/
  printMessage("Lednode started.");
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
    for (int i = 0; i < MAX_LED_COUNT; i++) {
        ledColors[i] = color;
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
    for (int i = MAX_LED_COUNT - 1; i > 0; i--) {
        ledColors[i] += mixColor(ledColors[i], ledColors[i - 1], bleedAmount);
    }
    
    // Bleed current color to first ones
    ledColors[0] = mixColor(ledColors[0], sourceColor1, bleedAmount);

    // Dampen colors
    for (int i = 0; i < MAX_LED_COUNT; i++) {
        ledColors[i].fadeToBlackBy(1);
    }
}


long sparkleIntervall_ms = 200;
long timeToNextSparkle = 1;
CRGB GRADIENT_FIRE_COLORS[] = {CRGB(255, 240, 150), CRGB(220, 180, 0), CRGB(200, 0, 0), CRGB(30, 0, 50), CRGB(0, 0, 60), CRGB(0, 0, 0)};
long GRADIENT_FIRE_DURATIONS[] = {500, 1000, 500, 2000, 100, 3000};


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
  //long deltaTime_ms = calculateDeltaTime();
    
  // Update subsystems
  //updateRadio(deltaTime_ms);  
  //updateLeds(deltaTime_ms);  

  Serial.println("ping");
  
  delay(500);


}




