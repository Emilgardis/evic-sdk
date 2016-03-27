#include <stdio.h>
#include <math.h>
#include <M451Series.h>
#include <Display.h>
#include <Font.h>
#include <Atomizer.h>
#include <Button.h>
#include <TimerUtils.h>
#include <Battery.h>

#include "Bitmap_eVicSDK.h"

#define FIRE 0
#define RIGHT 1
#define LEFT 2

volatile uint8_t shouldFire = 0;
volatile uint32_t buttonSpec[3][3] = {{0},{0},{0}}; // [timesPressed, justPressed, ?uncertain?]
volatile uint32_t timer = 0; // TODO: Handle overflow.
volatile uint32_t newWatts = 0; //Is this safe?
void timerCallback(){
    timer++;
}


uint16_t wattsToVolts(uint32_t watts, uint16_t res) {
	// Units: mV, mW, mOhm
	// V = sqrt(P * R)
	// Round to nearest multiple of 10
	uint16_t volts = (sqrt(watts * res) + 5) / 10;
	return volts * 10;
}

uint16_t correctVoltage(uint16_t currentVolt, uint32_t currentWatt, uint16_t res) {
    // Resistance fluctuates, this corrects voltage to the correct setting.
    uint16_t newVolts = wattsToVolts(currentWatt, res);
    
    if (newVolts != currentVolt) {
        if (Atomizer_IsOn()) {
            // Update output voltage to correct res variations:
            // If the new voltage is lower, we only correct it in
            // 10mV steps, otherwise a flake res reading might
            // make the voltage plummet to zero and stop.
            // If the new voltage is higher, we push it up by 100mV
            // to make it hit harder on TC coils, but still keep it
            // under control.
            if (newVolts < currentVolt) { newVolts = currentVolt - (currentVolt >= 10 ? 10 : 0); }
            else { newVolts = currentVolt + 100; }
        }
    }
    return newVolts;
}

void buttonRightCallback(uint8_t state){ // Only gets called when something happens.
    if (state & BUTTON_MASK_RIGHT){
        buttonSpec[RIGHT][0]++;
        buttonSpec[RIGHT][1] = 1;
        buttonSpec[RIGHT][2] = timer;
        newWatts += 100;
    } else {
        buttonSpec[RIGHT][1] = 0;
        buttonSpec[RIGHT][2] = timer;
    }
}


void buttonLeftCallback(uint8_t state){ // Only gets called when something happens.
    if (state & BUTTON_MASK_LEFT){
        buttonSpec[LEFT][0]++;
        buttonSpec[LEFT][1] = 1;
        buttonSpec[LEFT][2] = timer;
        newWatts -= 100;
    } else {
        buttonSpec[LEFT][1] = 0;
        buttonSpec[LEFT][2] = timer;
    }
}


int main(){
    char buf[100];
	uint16_t volts, displayVolts, newVolts/*, battVolts*/; // Unit mV
	uint32_t watts; // Unit mW
	uint8_t btnState;/*, battPerc, boardTemp*/;
	Atomizer_Info_t atomInfo;
    
    Atomizer_ReadInfo(&atomInfo);
    
    watts = 20000; // Initial wattage
    volts = wattsToVolts(watts, atomInfo.resistance);
    Atomizer_SetOutputVoltage(volts);
    
    Timer_CreateTimeout(10, 1, timerCallback, 0);
    Button_CreateCallback(buttonRightCallback, BUTTON_MASK_RIGHT);
    Button_CreateCallback(buttonLeftCallback, BUTTON_MASK_LEFT);

    // Main loop!
    newWatts = watts;
    while(1){
        Atomizer_ReadInfo(&atomInfo);
        btnState = Button_GetState(); // Unsure if needed.
        
        
        if (newWatts > 75000){
            newWatts = 75000;
        } else if (newWatts < 1000){
            newWatts = 1000;
        }
        watts = newWatts;

        for(int i=1; i<=2; i++){
            uint32_t mod = 1; 
            if (i == LEFT)
                mod = -1;

            if (buttonSpec[i][1] == 1){
                uint32_t elapsed = timer - buttonSpec[i][2];

                if (elapsed > 60 && elapsed < 180) {
                    newWatts += mod * 25;
                } else if (elapsed > 180){
                    newWatts += mod * 350; 
                }
            }
        }

        if(!Atomizer_IsOn() && (btnState == BUTTON_MASK_FIRE) &&
            (atomInfo.resistance != 0) && (Atomizer_GetError() == OK)){
                Atomizer_Control(1);
        } else if (Atomizer_IsOn()){
                Atomizer_Control(0);
        }
        
        Atomizer_ReadInfo(&atomInfo);
        
        volts = correctVoltage(volts, watts, atomInfo.resistance);
        Atomizer_SetOutputVoltage(volts);

		displayVolts = Atomizer_IsOn() ? atomInfo.voltage : volts;
        
        siprintf(buf, "P:%3lu.%luW\nV:%3d.%02d\n%d",
        watts / 1000, watts % 1000 / 100,
		displayVolts / 1000, displayVolts % 1000 / 10,
        newWatts);
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
    }
}
