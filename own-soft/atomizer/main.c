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

volatile uint32_t timerCounter[3] = {0}; //Space for timers of buttons.
volatile uint8_t buttonPressed[3] = {0}; //Determines if button was just pressed
volatile uint8_t timerSpec[3] = {0}; // How many times button has been "tapped"
volatile uint8_t shouldFire = 0;

void timerCallback() {
	// We use the optional parameter as an index
	// counterIndex is the index in timerCounter
    timerCounter[FIRE]++;
    timerCounter[RIGHT]++;
    timerCounter[LEFT]++;
    return;
}

void updateCounter(uint8_t btn) {
    // Handles timer and other logic functions
    // i.e use buttonPressed[buttonID] instead of btnState & BUTTON_MASK_buttonID
    //
    // Each button handler checks and does atleast this accordingly:
    // - If the button was not pressed in previous iter and is now pressed:
    // -- reset timerCounter[buttonID] and set buttonPressed[buttonID] to True.
    // -- if button was unpressed for 60 10ms (600 ms), reset timerSpec[buttonID] 
    // - If the button is realesed and was pressed in previous iter:
    // -- If button was just pressed and was pressed under 60 10ms (600 ms), increase timerSpec[buttonID]
    // -- reset timerCounter[buttonID] and set buttonPressed[buttonID] to False.
    
    // Fire button timer and counter logic
    for (int buttonID=0; buttonID <=2; buttonID++) {
        int8_t mask = 0xFF; //Should always be set.
        switch (buttonID){
            case 0:
                mask = BUTTON_MASK_FIRE;
                break;
            case 1:
                mask = BUTTON_MASK_RIGHT;
                break;
            case 2:
                mask = BUTTON_MASK_LEFT;
                break;
        }
        if (btn & mask) {
            if (buttonPressed[buttonID] == 0) {
                timerCounter[buttonID] = 0;
            }
            //This if may be damaging to some functions
            if (timerCounter[buttonID] > 60) {
                timerSpec[buttonID] = 0;
            }
        buttonPressed[buttonID] = 1;
        }
        else if (!(btn & mask)) {
            if (timerCounter[buttonID] < 60 && buttonPressed[buttonID]) {
                timerSpec[buttonID]++;
                timerCounter[buttonID] = 0;
            }
            else if (buttonPressed[buttonID] || timerCounter[buttonID] > 60) {
                timerSpec[buttonID] = 0;
            }
        buttonPressed[buttonID] = 0;
        }
    }
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

enum Mode  {
    defaultmode,
    setupmode,
} volatile mode; 



int main(){
    // TODO:    Change FIRE,RIGHT,LEFT to an enum
    //          Add Stealth mode.
    //          Add TCR (Formulas?), with Mode::tcrsetupmode, like in stock.
    //          Implement the same kind of thing that enables stock firm to not have to plug in USB with right button.
    //          Add bypass mode. (?)
    //          Implement a better font.
    //          Fixed fields for values and strings.
    //          Add a small snake game. (?) What would the memory impact be? Trigger? left left right right fire 2x.
    //          Make a implemented button "class". Is this even necessary. (?)
    //          See how puff and time is implemented and use these mem locations for something cool.
    //          Add volt mode. (?)
    char buf[100];
    uint16_t volts, displayVolts, newVolts/*, battVolts*/; // Unit mV
	uint32_t watts, inc; // Unit mW
	uint8_t btnState;/*, battPerc, boardTemp*/;
	Atomizer_Info_t atomInfo;
    
    Atomizer_ReadInfo(&atomInfo);
    
    watts = 20000; // Initial wattage
    volts = wattsToVolts(watts, atomInfo.resistance);
    Atomizer_SetOutputVoltage(volts);
    
    // Display On screen for 0.5 s.
    Display_PutPixels(0, 32, Bitmap_evicSdk, Bitmap_evicSdk_width, Bitmap_evicSdk_height);
    Display_Update();
    Timer_DelayMs(500);
    
    
    Timer_CreateTimeout(10,1, timerCallback, 0); //Fire
    while(1){
        btnState = Button_GetState();
        Atomizer_ReadInfo(&atomInfo);
        
        // Button logic
        updateCounter(btnState);
        
        // If has tapped and time elapsed is less than 60 10ms, disable firing
        if (timerSpec[FIRE] > 1 && timerCounter[FIRE] < 60) { 
            shouldFire = 0;
        } else {
            shouldFire = 1;   
        }
        
        //If should fire
        if (!Atomizer_IsOn() && (buttonPressed[FIRE]) &&
            (atomInfo.resistance != 0) && shouldFire &&
            (Atomizer_GetError() == OK)) {
                // Take power on
                Atomizer_Control(1);
        } //If not firing
        else if((Atomizer_IsOn() && !(buttonPressed[FIRE])) || !shouldFire) {
			// Take power off
			Atomizer_Control(0);
		}
        
        for (int buttonID=1; buttonID <= 2 && buttonID >=1; buttonID++){
            int mod = 1; // Modifier, inc should increase value with RIGHT, decrease with LEFT.
            if (buttonID == LEFT){
              mod = -1;
            }
        
            if (buttonPressed[buttonID]) {
                if (timerCounter[buttonID] <= 110) {
                    inc = round(0.479332 * exp(timerCounter[buttonID]*0.0303612)) * 100;
                } else {
                    inc = 1800;
                }
                newVolts = wattsToVolts(watts + mod*inc, atomInfo.resistance);
                if ((mod == 1) && (watts + inc >= 75000)) {
                    watts = 75000;
                    newVolts = wattsToVolts(watts, atomInfo.resistance);
                } else if ((mod == -1) && watts - inc <= 0) {
                    watts = 0;
                    newVolts = wattsToVolts(watts, atomInfo.resistance);  
                } else if (newVolts <= ATOMIZER_MAX_VOLTS) {
                    watts += mod*inc;
                }
                volts = newVolts;
                Atomizer_SetOutputVoltage(volts);
            }
        }
        
        Atomizer_ReadInfo(&atomInfo);
		
        volts = correctVoltage(volts, watts, atomInfo.resistance);
        Atomizer_SetOutputVoltage(volts);
        
        displayVolts = Atomizer_IsOn() ? atomInfo.voltage : volts;
        
        siprintf(buf, "P:%3lu.%luW\nV:%3d.%02d\n",
        watts / 1000, watts % 1000 / 100,
		displayVolts / 1000, displayVolts % 1000 / 10);
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
    }
    
}