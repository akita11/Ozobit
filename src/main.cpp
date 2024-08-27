#include <Arduino.h>
#include "Wire.h"
#include "veml6040.h"
#include <Adafruit_NeoPixel.h>
#include <MsTimer2.h>

// https://github.com/thewknd/VEML6040/
VEML6040 RGBWSensor;

#define L_A 5 // PD5
#define L_B 6 // PD6
#define R_A 9 // PB1
#define R_B 10 // PB2
#define LEDC 3 // for WS2812
#define LED_EN 2 // PD2
#define PD_R2 A3
#define PD_R1 A2
#define PD_L1 A1
#define PD_L2 A0

//#define DEBUG_COLOR

#define PD_THRESHOLD 600   // threshold for photo diode

Adafruit_NeoPixel pixel(1, LEDC, NEO_GRB + NEO_KHZ800);

void enableSensorLED(uint8_t f)
{
	if (f == 1) digitalWrite(LED_EN, HIGH);
	else digitalWrite(LED_EN, LOW);
}

#define MAX_PWM_L 255
#define MAX_PWM_R 255

uint16_t convMotorPWM(uint8_t max_pwm, float v){
	return((uint16_t)(max_pwm * v));
}

// vL/vR : -1 - +1 / +=FWD, -=BWD
void setMotorSpeed(float vL, float vR)
{
	if (vL > 0){
		analogWrite(L_A, convMotorPWM(MAX_PWM_L, vL));	analogWrite(L_B, 0);
	}
	else{
		analogWrite(L_A, 0); analogWrite(L_B, -convMotorPWM(MAX_PWM_L, vL));
	}
	if (vR > 0){
		analogWrite(R_A, convMotorPWM(MAX_PWM_R, vR));	analogWrite(R_B, 0);
	}
	else{
		analogWrite(R_A, 0); analogWrite(R_B, -convMotorPWM(MAX_PWM_R, vR));
	}
}

#define COLOR_NONE	0
#define COLOR_WHITE 1
#define COLOR_BLACK 2
#define COLOR_RED	  3
#define COLOR_GREEN 4
#define COLOR_BLUE  5

void setLED(uint8_t r, uint8_t g, uint8_t b)
{
	pixel.setPixelColor(0,  pixel.Color(r, g, b)); pixel.show();
}

uint8_t readSensor()
{
	// return: L2:L1:C:R1:R2:{COLOR[2:0]} (1=line detected)
	uint8_t sensorInfo = COLOR_NONE;
	uint16_t sensorR, sensorG, sensorB, sensorW;

	enableSensorLED(1);
	delay(2);

	uint8_t posLine = 0;
	if (analogRead(PD_L2) < PD_THRESHOLD) posLine |= 0x80;
	if (analogRead(PD_L1) < PD_THRESHOLD) posLine |= 0x40;
  if ((sensorInfo & 0x07) != COLOR_WHITE) posLine |= 0x20; 
	if (analogRead(PD_R1) < PD_THRESHOLD) posLine |= 0x10;
	if (analogRead(PD_R2) < PD_THRESHOLD) posLine |= 0x08;

	sensorR = RGBWSensor.getRed();
	sensorG = RGBWSensor.getGreen();
	sensorB = RGBWSensor.getBlue();
	sensorW = RGBWSensor.getWhite();
	// normalize
	float sensorRf, sensorGf, sensorBf;
	sensorRf = (float)sensorR / (float)sensorW * 100.0;
	sensorGf = (float)sensorG / (float)sensorW * 100.0;
	sensorBf = (float)sensorB / (float)sensorW * 100.0;
	//           Rf Gf Bf / W
	// White     59 71 71 / 22000
	// BlackLine 44 51 43
	// Black     43 58 40 / 12000
	// Red       52 43 34
	// Green     43 60 38
	// Blue      49 51 61

	// ToDo: fine tuning for color detect
	if (sensorW > 9000) sensorInfo = COLOR_WHITE;
	else{
		if (sensorBf > 55) sensorInfo = COLOR_BLUE;
		else if (sensorGf > 58 && sensorRf < 45 && sensorBf < 50) sensorInfo = COLOR_GREEN;
		else if (sensorRf > 50 && sensorGf < 58 && sensorBf < 58) sensorInfo = COLOR_RED;
		else sensorInfo = COLOR_BLACK;
	}
	if (sensorInfo == COLOR_BLACK) setLED(0, 0, 0);
	else if (sensorInfo == COLOR_RED) setLED(20, 0, 0);
	else if (sensorInfo == COLOR_GREEN) setLED(0, 20, 0);
	else if (sensorInfo == COLOR_BLUE) setLED(0, 0, 20);
	else if (sensorInfo == COLOR_WHITE) setLED(20, 20, 20);
#ifdef DEBUG_COLOR
	Serial.print(sensorInfo, HEX); Serial.print(":");
 	Serial.print(sensorRf); Serial.print(",");
 	Serial.print(sensorGf); Serial.print(",");
 	Serial.print(sensorBf); Serial.print(",");
 	Serial.print(sensorW); Serial.print(",");
 	Serial.print(analogRead(PD_R2)); Serial.print(",");
 	Serial.print(analogRead(PD_R1)); Serial.print(",");
 	Serial.print(analogRead(PD_L1)); Serial.print(",");
 	Serial.print(analogRead(PD_L2)); Serial.print("::");
#endif
#ifdef DEBUG_COLOR
	Serial.println(sensorInfo, HEX);
#endif
	enableSensorLED(0);
	return(sensorInfo | posLine);
}

uint8_t detectedColor, detectedLine;

// speed=2cm/s, mark=3mm -> 150ms

uint16_t tm = 0;
uint8_t dir = 0;

// every 10ms
void timerISR()
{
	if (dir == 0) setMotorSpeed(0.5, 0.5);
	else if (dir == 1) setMotorSpeed(1.0, 1.0);
	else if (dir == 3) setMotorSpeed(-0.5, -0.5);
	else if (dir == 4) setMotorSpeed(-1.0, -1.0);
	else setMotorSpeed(0.0, 0.0);
	tm++;
	if (tm == 200){
		tm = 0;
		dir = (dir + 1) % 6;
	}
}

void setup() {
	Serial.begin(115200);
  pixel.begin(); pixel.clear();
	setMotorSpeed(0.0, 0.0);
	enableSensorLED(0);
  Wire.begin(); 
	RGBWSensor.setConfiguration(VEML6040_IT_40MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
  if(!RGBWSensor.begin()) {
    Serial.println("VEML6040 init error");
    while(1);
  }
  MsTimer2::set(10, timerISR);
  MsTimer2::start();
}

void loop() {
	uint8_t sensorInfo = readSensor();
	detectedColor = sensorInfo & 0x07;
	detectedLine = sensorInfo >> 3;
	Serial.print(detectedLine, BIN); Serial.print(" "); Serial.println(detectedColor);
	delay(10);
}
