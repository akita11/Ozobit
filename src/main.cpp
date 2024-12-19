#include <Arduino.h>
#include <MsTimer2.h>
#include "peripheral.h"

// Motion Control Parameters
float Kp = 1.0; // P gain for Line trace
float Kd = 0.0; // D gain for Line trace
uint8_t tm1cm = 115; // [ms]
uint8_t tm10deg = 8; // [ms]
#define vNORMAL 0.3
#define vSlow 0.25
#define vFast 0.5
#define vVerySlow 0.2
#define vVeryFast 0.6
float normalV = vNORMAL;
float vL = 0.0, vR = 0.0; // left/right motor speed (0.0 - 1.0)
uint8_t dirTrace = 0;

// motion types in micro:bit cmd mode
#define MOTION_NONE				0 
#define MOTION_FWD				1
#define MOTION_BWD				2
#define MOTION_TURN_LEFT	3
#define MOTION_TURN_RIGHT	4
#define MOTION_ZIGZAG			5
#define MOTION_SKATE			6

// color mark command (3=R / 4=G / 5=B)
#define COLOR_CMD_VERY_SLOW        "435" // GRB
#define COLOR_CMD_SLOW             "345" // RGB
#define COLOR_CMD_NORMAL           "343" // RGR
#define COLOR_CMD_FAST             "543" // BGR
#define COLOR_CMD_VERY_FAST        "534" // BRG
#define COLOR_CMD_PAUSE            "535" // BRB
#define COLOR_CMD_LEFT_AT_CROSS    "453" // GBR
#define COLOR_CMD_FORWARD_AT_CROSS "545" // BGB 
#define COLOR_CMD_RIGHT_AT_CROSS   "454" // GBG
#define COLOR_CMD_UTURN            "353" // RBR
#define COLOR_CMD_GO_BACK          "434" // GRG
// pending commands
//#define COLOR_CMD_JUMP_LEFT        ""
//#define COLOR_CMD_JUMP_FORWARD     ""
//#define COLOR_CMD_JUMP_RIGHT       ""
//#define COLOR_CMD_JUMP_HURRICANE   ""
//#define COLOR_CMD_ZIGZAG           ""
//#define COLOR_CMD_SPIN             ""

// flag for operation
uint8_t fMotion = MOTION_NONE; // currently operating motion
uint8_t fLineTrace = 1; // Line trace mode at power on
uint8_t fDebug = 0; // debug output of sensor data

uint8_t detectedColor;
uint16_t tm10ms = 0;
uint8_t fCross = 0; // "Cross point" detected

#define N_BUF 64 // Serial RX buffer size
char buf[N_BUF];
uint8_t pBuf = 0;

// Valiables for color command in line trace mode 
uint8_t nColorCmd = 0;
#define MAX_COLOR_CMD 10
char ColorCmds[MAX_COLOR_CMD];
uint8_t pColorCmd = COLOR_WHITE;
uint8_t pColor = COLOR_WHITE;
uint8_t ColorCmd = COLOR_WHITE;
uint8_t nColorContinuous = 0;

uint8_t stateColorCmd = 0;
#define COLOR_CMD_ST_PAUSE         1
#define COLOR_CMD_ST_CROSS_LEFT    2
#define COLOR_CMD_ST_CROSS_FORWARD 3
#define COLOR_CMD_ST_CROSS_RIGHT   4
#define COLOR_CMD_ST_UTURN         5
uint8_t cmdTurnAtCross = 0;

double leftDevi = 1.0, rightDevi = 1.0;
uint8_t tSkate = 0;
#define SKATE_CYCLE 100 // [x 10ms]

uint8_t stateZigzag = 0;
#define ZIGZAG_STEP 100 // [x 10ms] of half cycle
#define ZIGZAG_TURN 50 // [x 10ms] of turn time

// every 10ms
void timerISR()
{
	if (tm10ms > 0) tm10ms--;
	if (fMotion == MOTION_SKATE){
		leftDevi = 1.0 + 0.3 * sin((double)tSkate / (double)SKATE_CYCLE * 2 * 3.14);
		rightDevi = 1.0 - 0.3 * sin((double)tSkate / (double)SKATE_CYCLE * 2 * 3.14);
		tSkate = (tSkate + 1) % SKATE_CYCLE;
	}
	if (fMotion == MOTION_ZIGZAG){
		stateZigzag = (stateZigzag + 1) % (ZIGZAG_STEP * 2);
	}
}

void setup() {
	Serial.begin(9600);
	init_peripheral();
	MsTimer2::set(10, timerISR); // every 10ms
	MsTimer2::start();
}

// ToDo: L&R motor calibration using straight move

int16_t getParam(char *s){
	if ((s[0] >= '0' && s[0] <= '9') || s[0] == '-' ) return(atoi(s));
	else return(0);
}

void loop() {
	if (fLineTrace == 1){
		if (stateColorCmd == COLOR_CMD_ST_PAUSE){
			if (tm10ms > 0) setMotorSpeed(0, 0);
			else stateColorCmd = 0;
		}
		else if (stateColorCmd == COLOR_CMD_ST_UTURN){
			if (tm10ms > 0) setMotorSpeed(-vNORMAL, vNORMAL);
			else stateColorCmd = 0;
		}
		else if (fMotion == MOTION_TURN_RIGHT){
			if (tm10ms > 0) setMotorSpeed(vNORMAL, -vNORMAL);
			else{
				stateColorCmd = 0;
				fMotion = MOTION_NONE;
			}
		}
		else if (fMotion == MOTION_TURN_LEFT){
			if (tm10ms > 0) setMotorSpeed(-vNORMAL, vNORMAL);
			else{
				stateColorCmd = 0;
				fMotion = MOTION_NONE;
			}
		}
		else{
			// line trace mode
			SensorData sd;
			sd = readSensor(sd);
			detectedColor = sd.color;

			// differential value of lineValue
			float d_line = sd.line - sd.line_previous;
			sd.line_previous = sd.line;

			// color command detection	
#define COLOR_MARK_TH 10
			if (sd.color == pColor) nColorContinuous++;
			else{
//			if (nColorContinuous > COLOR_MARK_TH){
//				Serial.print(pColor); Serial.print(':'); Serial.println(nColorContinuous);
//			}
				nColorContinuous = 0;
			}
			pColor = sd.color;
			if (nColorContinuous > COLOR_MARK_TH){
				// color mark detected (spike noise removed)
				ColorCmd = sd.color;
				if (ColorCmd != pColorCmd){
					// color mark changed
					if (ColorCmd != COLOR_WHITE && ColorCmd != COLOR_BLACK){
						ColorCmds[nColorCmd] = '0' + ColorCmd;
						nColorCmd++;
						if (nColorCmd == MAX_COLOR_CMD){
							// color command buffer full, ignore buffer
							nColorCmd = 0;
						}
					}
					if ((ColorCmd == COLOR_BLACK || ColorCmd == COLOR_WHITE) && (pColorCmd != COLOR_BLACK && pColorCmd != COLOR_WHITE)){
						ColorCmds[nColorCmd] = '\0';
						// end of color command
						Serial.print(nColorCmd); Serial.print('*'); Serial.println(ColorCmds);
						if (nColorCmd >= 3){
							// execute motion
							if (strncmp(ColorCmds, COLOR_CMD_VERY_SLOW, 3) == 0){
							Serial.println("CMD:veryslow");
							normalV = vVerySlow;
						}
							if (strncmp(ColorCmds, COLOR_CMD_SLOW, 3) == 0){
							Serial.println("CMD:slow");
							normalV = vSlow;
						}
							if (strncmp(ColorCmds, COLOR_CMD_NORMAL, 3) == 0){
							Serial.println("CMD:normal");
							normalV = vNORMAL;
						}
							if (strncmp(ColorCmds, COLOR_CMD_FAST, 3) == 0){
							Serial.println("CMD:fast");
							normalV = vFast;
						}
							if (strncmp(ColorCmds, COLOR_CMD_VERY_FAST, 3) == 0){
							Serial.println("CMD:veryfast");
							normalV = vVeryFast;
						}
							if (strncmp(ColorCmds, COLOR_CMD_PAUSE, 3) == 0){
								Serial.println("CMD:pause");
								tm10ms = 300; // pause time = 300ms
								stateColorCmd = COLOR_CMD_ST_PAUSE;
							}
							if (strncmp(ColorCmds, COLOR_CMD_LEFT_AT_CROSS, 3) == 0){
								Serial.println("CMD:left at cross");
								stateColorCmd = COLOR_CMD_ST_CROSS_LEFT;
								cmdTurnAtCross = COLOR_CMD_ST_CROSS_LEFT;
							}
							if (strncmp(ColorCmds, COLOR_CMD_FORWARD_AT_CROSS, 3) == 0){
								Serial.println("CMD:forward at cross");
								stateColorCmd = COLOR_CMD_ST_CROSS_FORWARD;
								cmdTurnAtCross = COLOR_CMD_ST_CROSS_FORWARD;
							}
							if (strncmp(ColorCmds, COLOR_CMD_RIGHT_AT_CROSS, 3) == 0){
								Serial.println("CMD:right at cross");
								stateColorCmd = COLOR_CMD_ST_CROSS_RIGHT;
								cmdTurnAtCross = COLOR_CMD_ST_CROSS_RIGHT;
							}
							if (strncmp(ColorCmds, COLOR_CMD_UTURN, 3) == 0){
								Serial.println("CMD:u-turn");
								stateColorCmd = COLOR_CMD_ST_UTURN;
								tm10ms = tm10deg * 18; // turn 180deg
							}
							if (strncmp(ColorCmds, COLOR_CMD_GO_BACK, 3) == 0){
								Serial.println("CMD:go back");
								dirTrace = 1 - dirTrace;
							}
						}
						nColorCmd = 0;
					}
					pColorCmd = ColorCmd;
				}
			}

			// P control
//			Serial.println(sd.line);
			if (sd.line < -5.0){
			// seek for line
				vL = normalV; vR = normalV;
		}
			else{
				if (sd.color == COLOR_BLACK){
					if (sd.line > 0.0){
						// line at right, turn right
						vL = normalV;
						vR = normalV - Kp * sd.line;
					}
					else if (sd.line < 0.0){
						// line at left, turn left
						vL = normalV + Kp * sd.line;
						vR = normalV;
					}
					else{
						vL = normalV;
						vR = normalV;
					}
				}
				else{
					// go straint on color marker
					vL = normalV;
					vR = normalV;
				}
			}
			// D control
			vL += d_line * Kd;
			vR += d_line * Kd;
			if (vL > MAX_V) vL = MAX_V;
			else if (vL < MIN_V) vL = MIN_V;
			if (vR > MAX_V) vR = MAX_V;
			else if (vR < MIN_V) vR = MIN_V;
			if (dirTrace == 0) setMotorSpeed(vL, vR);
			else setMotorSpeed(-vR, -vL);
			// cross detection
			if (stateColorCmd != COLOR_CMD_ST_UTURN){
#define LINE_CROSS_TH 3.0
				if (sd.width > LINE_CROSS_TH){
					if (fCross == 0){
						fCross = 1;
						Serial.print("cross "); Serial.println(cmdTurnAtCross);
						if (cmdTurnAtCross == COLOR_CMD_ST_CROSS_LEFT){
							// turn left at cross
								fMotion = MOTION_TURN_LEFT;
							tm10ms = tm10deg * 9; // turn 90deg
							Serial.println("turn left at cross");
							cmdTurnAtCross = 0;
						}
						if (cmdTurnAtCross == COLOR_CMD_ST_CROSS_RIGHT){
							// turn right at cross
							fMotion = MOTION_TURN_RIGHT;
							tm10ms = tm10deg * 9; // turn 90deg
							Serial.println("turn right at cross");
							cmdTurnAtCross = 0;
						}
					}
				}
				else fCross = 0;
			}
		}
	}
	else{
		// micro:bit command motion
		enableSensorLED(0);
		switch(fMotion){
			case MOTION_NONE : setMotorSpeed(0, 0); break;
			case MOTION_TURN_RIGHT:
				if (tm10ms > 0) setMotorSpeed(vNORMAL, -vNORMAL);
				else{ setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
			case MOTION_TURN_LEFT:
				if (tm10ms > 0) setMotorSpeed(-vNORMAL, vNORMAL);
				else{ setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
			case MOTION_FWD:
				if (tm10ms > 0) setMotorSpeed(vNORMAL, vNORMAL);
				else{ setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
			case MOTION_BWD:
				if (tm10ms > 0) setMotorSpeed(-vNORMAL, -vNORMAL);
				else{ setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
			case MOTION_ZIGZAG:
				if (tm10ms > 0){
					if (stateZigzag >= 0 && stateZigzag < ZIGZAG_TURN) setMotorSpeed(vNORMAL, -vNORMAL); // turn right
					else if (stateZigzag >= ZIGZAG_STEP && stateZigzag < ZIGZAG_STEP + ZIGZAG_TURN) setMotorSpeed(-vNORMAL, vNORMAL); // turn right
					else setMotorSpeed(vNORMAL, vNORMAL); // go straight
				}
				else {setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
			case MOTION_SKATE:
//				Serial.print(tSkate); Serial.print(' '); Serial.print(vNORMAL*leftDevi); Serial.print(' '); Serial.println(vNORMAL*rightDevi);
				if (tm10ms > 0) setMotorSpeed(vNORMAL * leftDevi, vNORMAL * rightDevi);
				else {setMotorSpeed(0, 0); fMotion = MOTION_NONE;}
				break;
		}
	}

	int16_t param;
	while(Serial.available()){
		char c = Serial.read();
		if (c > 0){ // ignore non-ASCII characters
			if (c == '\r' || c == '\n'){
				buf[pBuf] = '\0';
				pBuf = 0;
				if (buf[0] == 'P'){
					Serial.print("Kp(k)="); Serial.print(Kp);
					Serial.print(" Kd(K)="); Serial.print(Kd); 
					Serial.print(" V(v)="); Serial.print(normalV); 
					Serial.print(" tm1cm(f)="); Serial.print(tm1cm); 
					Serial.print(" tm10deg(g)="); Serial.println(tm10deg);
				}
				if (buf[0] == 'T') {fLineTrace = 1; pColorCmd = COLOR_WHITE; nColorCmd = 0; }
				if (buf[0] == 't') {fLineTrace = 0; setMotorSpeed(0, 0); }
				if (buf[0] == 'D') fDebug = 1;
				if (buf[0] == 'd') fDebug = 0;
				if (buf[0] == 'k'){ Kp = atof(&buf[1]); Serial.println(Kp); }
				if (buf[0] == 'K'){ Kd = atof(&buf[1]); Serial.println(Kd); }
				if (buf[0] == 'v'){ normalV = atof(&buf[1]); Serial.println(normalV); }
				if (buf[0] == 'f'){ tm1cm = atoi(&buf[1]); Serial.println(tm1cm); }
				if (buf[0] == 'g'){ tm10deg = atoi(&buf[1]); Serial.println(tm10deg); }

				// micro:bit command
				if (buf[0] == '$'){
					Serial.println("enter micro:bit command mode");
					fLineTrace = 0;
					setMotorSpeed(0, 0);
					setLED(20, 0, 20); // purple
				}
				if (buf[0] == '#'){
					Serial.println("exit micro:bit command mode");
					fLineTrace = 1;
					setLED(0, 0, 0); // black
				}
				if (buf[0] == 'R'){
					// Rxx turn right(+) or left(-) xx degree
					fLineTrace = 0;
					param = getParam(buf+1);
					if (param >0){
						fMotion = MOTION_TURN_RIGHT;
						tm10ms = tm10deg * param / 10;
						Serial.println(tm10ms);
					}
					else{
						fMotion = MOTION_TURN_LEFT;
						tm10ms = tm10deg * (-param) / 10;
					}
				}
				if (buf[0] == 'B'){
					// B stop
					fMotion = MOTION_NONE;
				}
				if (buf[0] == 'F'){
					// Fxx go straight xxcm
					param = getParam(buf+1);
					if (param > 0){
						fMotion = MOTION_FWD;
						tm10ms = tm1cm * param / 10;
					}
					else{
						fMotion = MOTION_BWD;
						tm10ms = tm1cm * (-param) / 10;
					}
				}
				if (buf[0] == 'Z'){
					// Zxx zig-zag xxcm
					param = getParam(buf+1);
					fMotion = MOTION_ZIGZAG;
					stateZigzag = 0;
					tm10ms = param; // tentative
				}
				if (buf[0] == 'S'){
					// Sxx skate xxcm
					param = getParam(buf+1);
					fMotion = MOTION_SKATE;
					tSkate = 0;
					tm10ms = param; // tentative
				}
			}
			else{ buf[pBuf++] = c; if (pBuf == N_BUF) pBuf = 0; }
		}
	}
}
