
#include <RHDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>
#include <LiquidCrystal.h>
#include <FastLED.h>
 

#include "FastLED.h"

// Test addresses
#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

// Prints extra many things
#define DEBUG_MODE true

// Pin assignments
#define LCD_RS_PIN        2
#define LCD_ENABLE_PIN    3
#define LCD_DATA4_PIN     4
#define LCD_DATA5_PIN     5
#define LCD_DATA6_PIN     6
#define LCD_DATA7_PIN     7
#define ROTARY_A_PIN      8 
#define ROTARY_B_PIN      9
#define LCD_BACKLIGHT_PIN 10
#define RADIO_MOSI_PIN    11
#define RADIO_MISO_PIN    12
#define RADIO_SCK_PIN     13    
#define RADIO_CSN_PIN     A0
#define RADIO_CE_PIN      A1 
#define ROTARY_PUSH_PIN   A2
#define STATUS_LED_PIN    A3
#define EXTRA_IO_1_PIN    A4
#define EXTRA_IO_2_PIN    A5
#define SHAKE_PIN         A6
#define LIGHTLEVEL_PIN    A7



// LCD library
LiquidCrystal lcd(LCD_RS_PIN, LCD_ENABLE_PIN, LCD_DATA4_PIN, LCD_DATA5_PIN, LCD_DATA6_PIN, LCD_DATA7_PIN);

// Singleton instance of the radio driver
RH_NRF24 driver(RADIO_CE_PIN, RADIO_CSN_PIN);

// Class to manage message delivery and receipt, using the driver declared above
RHDatagram manager(driver, SERVER_ADDRESS);

// Dont put this on the stack:
uint8_t buffer[RH_NRF24_MAX_MESSAGE_LEN];


#define MESSAGE_SET_COLOR 0x02
#define MESSAGE_SET_MODE 0x03

#define MODE_OFF 0x00
#define MODE_LIGHT 0x01
#define MODE_COLOR 0x02
#define MODE_WAVES 0x03
#define MODE_SPARKLE 0x04
#define FIRST_MODE MODE_OFF
#define LAST_MODE MODE_SPARKLE
const int NUM_MODES = LAST_MODE + 1;

char* MODE_NAMES[NUM_MODES] = {
  "Off",
  "Light",
  "Color",
  "Waves",
  "Sparkle",
};

#define NUM_LEDS 1
CRGB previewLeds[NUM_LEDS];

CRGB targetColor;
int currentHue = 0;
int currentMode = FIRST_MODE;


unsigned long currentTime_ms = 0;


CRGB randomColor() {
   CRGB c;
   c.r = random8();   
   c.g = random8();   
   c.b = random8();   
   return c;
}


void setup() {
   /*
   pinMode(LCD_RS_PIN, OUTPUT);
   pinMode(LCD_ENABLE_PIN, OUTPUT);
   pinMode(LCD_DATA4_PIN, OUTPUT);
   pinMode(LCD_DATA5_PIN, OUTPUT);
   pinMode(LCD_DATA6_PIN, OUTPUT);
   pinMode(LCD_DATA7_PIN, OUTPUT);
   */
   pinMode(LCD_BACKLIGHT_PIN, OUTPUT);

   pinMode(STATUS_LED_PIN, OUTPUT);

   pinMode(EXTRA_IO_1_PIN, INPUT);
   pinMode(EXTRA_IO_2_PIN, INPUT);

   pinMode(ROTARY_A_PIN, INPUT);  
   pinMode(ROTARY_B_PIN, INPUT);  
   pinMode(ROTARY_PUSH_PIN, INPUT);

   pinMode(SHAKE_PIN, INPUT);
   pinMode(LIGHTLEVEL_PIN, INPUT);  


  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.print("Light Controller");

  FastLED.addLeds<NEOPIXEL, STATUS_LED_PIN>(previewLeds, NUM_LEDS);

  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!manager.init()) {
    Serial.println("Radio initialization failed");
    lcd.print("Radio init fail");    
  }
    
  targetColor = randomColor(); 

  analogWrite(LCD_BACKLIGHT_PIN, 0);

 
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

void handleMessage(uint8_t buffer[], uint8_t length) {
  Serial.print("Got message: ");
  Serial.println((char*)buffer);  
  
  if (length < 1) {
    Serial.print("Empty message.");    
    return;
  }
  
  uint8_t messageType = buffer[0];

  switch (messageType) {
    
    default:
      Serial.print("Unknown message type: ");    
      Serial.println(messageType, HEX);
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

// TODO: Take variable number of arguments of type unsigned byte, create parameter array in this method.
void sendMessage(uint8_t recipient, uint8_t messageType, uint8_t parameters[], int paramLength) {
  if (paramLength + 1 > sizeof(buffer)) {
    debugMessage("Not enough buffer space to send message", messageType, parameters, paramLength);
    return;
  }

  // Copy to buffer
  buffer[0] = messageType;
  for (int i = 0; i < paramLength; i++) {
    buffer[i + 1] = parameters[i];
  }

  // Send  
  if (!manager.sendto(buffer, paramLength + 1, recipient)) {
    debugMessage("Message too long for the driver to send", messageType, parameters, paramLength);
    return;
  }

  // Print if in debug mode
  if (DEBUG_MODE) {
    debugMessage("Sending message", messageType, parameters, paramLength);
  }
}
  



void sendSetColor(CRGB color) {
  uint8_t rgb[3];
  rgb[0] = color.r;
  rgb[1] = color.b; // TODO: Temporary fix, it seems the lednode mixes blue and green?
  rgb[2] = color.g;
  
  sendMessage(RH_BROADCAST_ADDRESS, MESSAGE_SET_COLOR, rgb, 3);  
}

void sendChangeMode(int currentMode) {
  uint8_t mode[1];
  mode[0] = currentMode;
  
  sendMessage(RH_BROADCAST_ADDRESS, MESSAGE_SET_MODE, mode, 1);  
}

void previewColor(CRGB color) {
  // Send the color to the WS2812B led on pin STATUS_LED_PIN.
  previewLeds[0] = color; 
  FastLED.show(); 
}

void refreshLcd() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(MODE_NAMES[currentMode]);
  lcd.setCursor(0, 1);
  lcd.print("Hue = ");  
  lcd.print(currentHue);  
}


void setMode(int mode) {
  currentMode = mode;  
  previewColor(CRGB::Black);
  refreshLcd();
  sendChangeMode(currentMode); 
}

void setHue(int hue) {
    if (currentHue != hue) {
      currentHue = hue;
      targetColor.setHue(currentHue);

      previewColor(targetColor);
      refreshLcd();
      sendSetColor(targetColor);
    }
}

 

// TODO: Extract to own library
int encoderPinALast = LOW;
long encoderLastChange = 0;
const long ACCELERATION_RESET = 500000;
const long NO_ACCELERATION  = 100000;
const long MAX_ACCELERATION = 10000;
const long MAX_ACCELERATION_FACTOR = 200;
const long ACCELERATION_DAMPENING = 6;
long averageAcceleration = 0;
long readEncoder() {
   long delta = 0; 
   int aPinStatus = digitalRead(ROTARY_A_PIN);
   if ((encoderPinALast == LOW) && (aPinStatus == HIGH)) {
     if (digitalRead(ROTARY_B_PIN) == LOW) {
       delta = -1;
     } else {
       delta = 1;
     }

     // Apply acceleration
     long now = micros();
     long timeSinceLastChange = now - encoderLastChange;
     if (timeSinceLastChange > 0) {
       
        if (timeSinceLastChange > ACCELERATION_RESET) averageAcceleration = 1;
       
        long factor = (MAX_ACCELERATION_FACTOR * (timeSinceLastChange - NO_ACCELERATION)) / (MAX_ACCELERATION - NO_ACCELERATION);
        if (factor > MAX_ACCELERATION_FACTOR) factor = MAX_ACCELERATION_FACTOR;
        else if (factor < 1) factor = 1;
        
        averageAcceleration = (averageAcceleration * (ACCELERATION_DAMPENING - 1) + factor) / ACCELERATION_DAMPENING;
        averageAcceleration = max(1, averageAcceleration);
        
        delta = delta * averageAcceleration;
     }
     encoderLastChange = now;
     
   } 
   encoderPinALast = aPinStatus;
   return delta;
}


void readHueWheel(long deltaTime_ms) {
  
  long hueChange = readEncoder();

  long hue = currentHue + hueChange;

  // Wrap
  hue %= 256;
  if (hue < 0) hue += 256;

  setHue(hue);
}




boolean modeButtonPressed = false;
long modeButtonHysteresis_ms = 30;
long modeButtonCooldown_ms = 0;

void readModeChange(long deltaTime_ms) {
  modeButtonCooldown_ms -= deltaTime_ms;
  
  if (modeButtonCooldown_ms <= 0) {
    modeButtonCooldown_ms = 0;
    
    boolean pressedNow = (digitalRead(ROTARY_PUSH_PIN) == LOW);
  
    if (pressedNow != modeButtonPressed) {
      modeButtonCooldown_ms = modeButtonHysteresis_ms;
      modeButtonPressed = pressedNow;
      
      if (!modeButtonPressed) {
        if (currentMode >= LAST_MODE) setMode(FIRST_MODE);
        else setMode(currentMode + 1);        
      }
    }    
  }  
}





void readInputs(long deltaTime_ms) {
  readHueWheel(deltaTime_ms);
  readModeChange(deltaTime_ms);
  
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
  readInputs(deltaTime_ms);
  updateRadio(deltaTime_ms);  
  
}




