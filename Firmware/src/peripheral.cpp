
#include <Arduino.h>
#include "Wire.h"
#include "veml6040.h"
#include <Adafruit_NeoPixel.h>

#include "peripheral.h"

// https://github.com/thewknd/VEML6040/
VEML6040 RGBWSensor;

#define R_B    5 // PD5
#define R_A    6 // PD6
#define L_A    9 // PB1
#define L_B    10 // PB2
#define LEDC   3 // for WS2812
#define LED_EN 2 // PD2
#define PD_R2  A3
#define PD_R1  A2
#define PD_L1  A1
#define PD_L2  A0

// for wide sensor
#define BLACK_PD_R2 36
#define WHITE_PD_R2 360
#define BLACK_PD_R1 42
#define WHITE_PD_R1 342
#define BLACK_PD_L1 45
#define WHITE_PD_L1 400
#define BLACK_PD_L2 35
#define WHITE_PD_L2 350
#define BLACK_COLOR 11000
#define WHITE_COLOR 20000

#define MAX_PWM_L 255
#define MAX_PWM_R 255

Adafruit_NeoPixel pixel(2, LEDC, NEO_GRB + NEO_KHZ800);

void init_peripheral()
{
  pixel.begin(); pixel.clear();
	pinMode(LED_EN, OUTPUT);
	setLED(20, 0, 0);
	setMotorSpeed(0.0, 0.0);
	enableSensorLED(0);
  Wire.begin(); 
	RGBWSensor.setConfiguration(VEML6040_IT_40MS + VEML6040_AF_AUTO + VEML6040_SD_ENABLE);
  if(!RGBWSensor.begin()) {
    Serial.println("VEML6040 init error");
    while(1){ setLED(20, 0, 0); delay(100); setLED(0, 0, 0); delay(100); }
  }
}

void enableSensorLED(uint8_t f)
{
	if (f == 1) digitalWrite(LED_EN, HIGH);
	else digitalWrite(LED_EN, LOW);
}

uint16_t convMotorPWM(uint8_t max_pwm, float v){
	if (v > 1.0) return(max_pwm);
	else return((uint16_t)(max_pwm * v));
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
		analogWrite(R_A, convMotorPWM(MAX_PWM_R, vR * LRratio));	analogWrite(R_B, 0);
	}
	else{
		analogWrite(R_A, 0); analogWrite(R_B, -convMotorPWM(MAX_PWM_R, vR * LRratio));
	}
}

void setLED(uint8_t r, uint8_t g, uint8_t b)
{
	pixel.setPixelColor(0,  pixel.Color(r, g, b)); pixel.setPixelColor(1,  pixel.Color(r, g, b)); pixel.show();
}

uint8_t classify(float R, float G, float B, float W) {
/*
	// SVM model on 241105
	float coeff[4][3] = {
  	{ 0.1129,  0.0743, -0.0001 }, // K
  	{ 2.9690, -0.9396,  0.4897 }, // R
  	{-1.3199,  1.7235, -0.9846 }, // G
  	{-1.9934,  0.5089,  1.5674 } // B
	};
	float intercepts[4] = { -7.7428, -93.6174, -3.8712, -5.7054 };

	float decision_values[4];
	for (int i = 0; i < 4; i++) {
		decision_values[i] = coeff[i][0] * R + coeff[i][1] * G + coeff[i][2] * B + intercepts[i];
	}
*/
/*
	// SVM model on 241219
	const float coeff[5][4] = {
		{2.358767, 0.676650, 0.927163, -0.008750},
		{0.408377, -0.959291, 0.012983, 0.001878},
		{-1.178557, 1.437267, -0.432852, -0.001315},
		{-0.697496, -0.585303, 0.749408, 0.001804},
		{0.000184, 0.000014, -0.000435, 0.024202},
	};

	const float intercepts[5] = {-56.276174, 0.011451, -0.007201, -0.020519, -398.521060};

	float decision_values[4];
	for (int i = 0; i < 4; i++) {
		decision_values[i] = coeff[i][0] * R + coeff[i][1] * G + coeff[i][2] * B + coeff[i][3] * W + intercepts[i];
	}
*/
/*
	// SVM model on 250106
	float coeff[4][4] = {
  	{ 0.62724268,	0.32172001,	0.23411658,	-0.00471049}, // K
		{ 0.13224463, -0.50406597, 	-0.24437417, 0.0024367}, // R
		{-0.4864799,	0.40343498,	-0.37843872,	0.00149977}, // G
		{-0.27300741,	-0.22108903,	0.38869631,	0.000774} // B
	};
	float intercepts[4] = { 0.02183718,	-0.00960655,	-0.00712973,	-0.0051009};
*/
/*
	// SVM model on 250107
	float coeff[4][4] = {
    {0.7915031829126435, 0.21811362291647074, 0.18818497119634045, -0.004734605859142181},
    {0.1832408121741865, -0.7416219302779076, -0.4891411183436856, 0.0035512261935955534},
    {-0.8108150578806952, 1.0712660912933223, -1.052206341908984, 0.0014183069683977592},
    {-0.457087237344295, -0.29783606922443995, 0.8184243113283347, -0.00015674014727286382}
	};
	float intercepts[] = {-0.0022824318860480293, -0.00943883639848339, -0.0019556079696409962, -0.0038688072084012313};
*/
/*
	// SVM model on 250204
	float coeff[4][4] = {
		{0.313390299,-0.016027054,0.065194798,-0.001882512},
		{0.40437232,-0.330667013,-0.18258023,0.001391661},
		{-0.485394388,0.449288413,-0.251409875,0.000450455},
		{-0.232368232,-0.102594347,0.368795308,4.03964E-05}
	};
	float intercepts[] = {4.992144795,-10.03804443,4.910028326,0.135871256};
*/
/*
// SVM model on 250206
	float coeff[4][4] = {
		{0.528539945, -0.187528438, 0.312149697, -0.001893526},
		{0.368227732, -0.2503185, -0.681466694, 0.002008556},
		{0.180587695, 0.292519793, -0.724353139, 0.000359511},
		{-1.077355372, 0.145327145, 1.093681105, -0.000474541}
	};
	float intercepts[] = {-1.867320642, 0.665782609, 2.863887692, -1.662837825};
	*/
/*
	// SVM model on 250318, using Yellow instead of Blue
	const float coeff[4][4] = {
    {-0.1849, -0.3695, 1.1175, -0.0012},
    {0.8419, -1.0080, 0.0009, 0.0007},
    {-1.1418, 0.9577, 0.0443, -0.0001},
    {-0.0742, 0.3076, -0.7294, 0.0006}
	};
	const float intercepts[4] = {0.0081, 0.0027, 0.1049, -0.0339};
*/
/*
	// SVM model on 250318, using Yellow instead of Blue, and detecting W
	const float coeff[5][4] = {
    {-0.6071, -0.2082, 0.8940, 0.0001},
    {1.1006, 0.3727, -0.1502, -0.0048},
    {0.7466, -0.7254, -0.0583, 0.0001},
    {-3.8503, 2.7830, -0.4656, 0.0025},
    {-0.0968, 0.1147, -0.5824, 0.0010}
	};
	const float intercepts[5] = {-0.0519, 0.0628, 0.0287, 0.9933, -0.0132};
*/
	// SVM model on 250318, W/K/R/G/Y
	const float coeff[5][4] = {
    {-0.9065, -0.1997, 0.7918, 0.0014},
    {0.0866, -0.1910, 0.7128, -0.0019},
    {0.7216, -0.6957, -0.0720, 0.00002},
    {-0.8884, 0.8518, -0.0515, -0.0004},
    {0.0086, 0.3788, -1.0278, 0.0006}
	};
	const float intercepts[5] = {-0.0586, 0.0156, 0.0136, 0.0881, -0.0271};


	float decision_values[5];
	for (int i = 0; i < 5; i++) {
		decision_values[i] = coeff[i][0] * R + coeff[i][1] * G + coeff[i][2] * B + coeff[i][3] * W + intercepts[i];
	}

	int max_index = 0;
	float max_score = decision_values[0];
	for (int i = 1; i < 5; i++) {
  	if (decision_values[i] > max_score) {
			max_score = decision_values[i];
    	max_index = i;
  	}
  }
/*
	if (max_index == 0) return(COLOR_BLACK);
	else if (max_index == 1) return(COLOR_RED);
	else if (max_index == 2) return(COLOR_GREEN);
	else return(COLOR_BLUE);
*/
	if (max_index == 0) return(COLOR_WHITE);
	else if (max_index == 1) return(COLOR_BLACK);
	else if (max_index == 2) return(COLOR_RED);
	else if (max_index == 3) return(COLOR_GREEN);
	else return(COLOR_BLUE);
}

// BLACK(1.0) - WHITE(0.0)
float lineValue(uint16_t val, uint16_t vBlack, uint16_t vWhite){
	float v;
	if (val < vBlack) v = 1.0;
	else if (val > vWhite) v = 0.0;
	else v = (float)(vWhite - val) / (float)(vWhite - vBlack);
	return(v);
}

SensorData readSensor(SensorData sd)
{
	// return: L2:L1:C:R1:R2:{COLOR[2:0]} (1=line detected)
	uint8_t sensorInfo = COLOR_NONE;
	uint16_t sensorR, sensorG, sensorB, sensorW;

	enableSensorLED(1);
//	delay(2);

	float s = 0.0;
	float v;
	sd.line = 0.0;
	v = lineValue(analogRead(PD_L2), BLACK_PD_L2, WHITE_PD_L2); sd.line += v * (-2); s += v;
	v = lineValue(analogRead(PD_L1), BLACK_PD_L1, WHITE_PD_L1); sd.line += v * (-1); s += v;
	v = lineValue(analogRead(PD_R1), BLACK_PD_R1, WHITE_PD_R1); sd.line += v * (+1); s += v;
	v = lineValue(analogRead(PD_R2), BLACK_PD_R2, WHITE_PD_R2); sd.line += v * (+2); s += v;

	sensorR = RGBWSensor.getRed();
	sensorG = RGBWSensor.getGreen();
	sensorB = RGBWSensor.getBlue();
	sensorW = RGBWSensor.getWhite();
//	enableSensorLED(0);

	// normalize
	float sensorRf, sensorGf, sensorBf;
	sensorRf = (float)sensorR / (float)sensorW * 100.0;
	sensorGf = (float)sensorG / (float)sensorW * 100.0;
	sensorBf = (float)sensorB / (float)sensorW * 100.0;

/*
	if (sensorW > WHITE_COLOR) sensorInfo = COLOR_WHITE;
	else sensorInfo = classify(sensorRf, sensorGf, sensorBf, sensorW);
*/
	sensorInfo = classify(sensorRf, sensorGf, sensorBf, sensorW); // model including W 
	sd.color = sensorInfo;
	if (stLEDmsg == LEDMSG_NONE){
		if (sensorInfo == COLOR_BLACK) setLED(0, 0, 0);
		else if (sensorInfo == COLOR_RED) setLED(10, 0, 0);
		else if (sensorInfo == COLOR_GREEN) setLED(0, 10, 0);
//		else if (sensorInfo == COLOR_BLUE) setLED(0, 0, 10); // blue (old)
		else if (sensorInfo == COLOR_BLUE) setLED(6, 6, 0); // yellow
		else if (sensorInfo == COLOR_WHITE) setLED(10, 10, 10);
	}
	
	if (sensorInfo != COLOR_WHITE) s += lineValue(sensorW, BLACK_COLOR, WHITE_COLOR);	
//	if (s == 0.0) sd.line = -10.0;
	if (s <  0.3) sd.line = -10.0;
	else sd.line = sd.line / s;
	sd.width = s;
	if (fDebug > 0){
	 	Serial.print(sensorRf); Serial.print(",");
	 	Serial.print(sensorGf); Serial.print(",");
	 	Serial.print(sensorBf); Serial.print(",");
	 	Serial.print(sensorW); Serial.print(",");
		if (fDebug == 6){
	 		Serial.print("|,");
		 	Serial.print(analogRead(PD_R2)); Serial.print(',');
		 	Serial.print(analogRead(PD_R1)); Serial.print(',');
	 		Serial.print(analogRead(PD_L1)); Serial.print(',');
	 		Serial.print(analogRead(PD_L2)); Serial.print(",|,");
			Serial.print(lineValue(analogRead(PD_R2), BLACK_PD_R2, WHITE_PD_R2)); Serial.print(',');
			Serial.print(lineValue(analogRead(PD_R1), BLACK_PD_R1, WHITE_PD_R1)); Serial.print(',');
			Serial.print(lineValue(analogRead(PD_L1), BLACK_PD_L1, WHITE_PD_L1)); Serial.print(',');
			Serial.print(lineValue(analogRead(PD_L2), BLACK_PD_L2, WHITE_PD_L2)); Serial.print(',');
			if (sensorInfo != COLOR_WHITE) Serial.print(lineValue(sensorW, BLACK_COLOR, WHITE_COLOR));
			Serial.print(",|,"); Serial.print(sd.line);
			Serial.print(','); Serial.print(sd.width);
			Serial.print('/'); Serial.print(sd.color);
//			Serial.print('|'); Serial.print(vL); Serial.print(','); Serial.println(vR);
			Serial.print(','); Serial.print(BatteryVoltage);
			Serial.println("");
		}
		else if (fDebug >= 1 && fDebug <= 5) Serial.println(fDebug); // with color label
	}
	return(sd);
}
