
#include <RHDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>

#include "FastLED.h"

// Test addresses
#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

// Prints extra many things
#define DEBUG_MODE true


// Singleton instance of the radio driver
RH_NRF24 driver;
// RH_NRF24 driver(8, 7);   // For RFM73 on Anarduino Mini

// Class to manage message delivery and receipt, using the driver declared above
RHDatagram manager(driver, SERVER_ADDRESS);

CRGB targetColor;

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

int currentMode = FIRST_MODE;

#define MODE_BUTTON_PIN A1

unsigned long currentTime_ms = 0;


CRGB randomColor() {
   CRGB c;
   c.r = random8();   
   c.g = random8();   
   c.b = random8();   
   return c;
}


void setup() {
  Serial.begin(9600);
  
  if (!manager.init()) {
    Serial.println("init failed");
  }  
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
    
   targetColor = randomColor(); 
 
   pinMode(A0, INPUT);  
   pinMode(MODE_BUTTON_PIN, INPUT);
   digitalWrite(MODE_BUTTON_PIN, HIGH);

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
    Serial.print("Sending message of type ");
  }
}
  



void sendSetColor(CRGB color) {
  uint8_t rgb[3];
  rgb[0] = color[0];
  rgb[1] = color[1];
  rgb[2] = color[2];
  
  sendMessage(RH_BROADCAST_ADDRESS, MESSAGE_SET_COLOR, rgb, 3);  
}

void sendChangeMode(int currentMode) {
  uint8_t mode[1];
  mode[0] = currentMode;
  
  sendMessage(RH_BROADCAST_ADDRESS, MESSAGE_SET_MODE, mode, 1);  
}



int oldHue;
void readHueWheel(long deltaTime_ms) {
  int hue = analogRead(A0) / 4;

  if (hue != oldHue) {
    oldHue = hue;
    targetColor.setHue(hue);
    
    sendSetColor(targetColor);
  }  
}


boolean modeButtonPressed = false;
long modeButtonHysteresis_ms = 30;
long modeButtonCooldown_ms = 0;

void readModeChange(long deltaTime_ms) {
  modeButtonCooldown_ms -= deltaTime_ms;
  
  if (modeButtonCooldown_ms <= 0) {
    modeButtonCooldown_ms = 0;
    
    boolean pressedNow = (digitalRead(MODE_BUTTON_PIN) == LOW);
  
    if (pressedNow != modeButtonPressed) {
      modeButtonCooldown_ms = modeButtonHysteresis_ms;
      modeButtonPressed = pressedNow;
      
      if (!modeButtonPressed) {
        currentMode++;
        if (currentMode > LAST_MODE) currentMode = FIRST_MODE;
        
        sendChangeMode(currentMode);
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




