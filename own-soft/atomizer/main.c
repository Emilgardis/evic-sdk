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


volatile uint32_t timerCounter[3] = {0}; //Space for timers of buttons, max 3
volatile uint8_t buttonPressed[3] = {0}; //Determines if button was just pressed
volatile uint8_t fireSpec = 0; // How many times button has been "tapped"
volatile uint8_t shouldFire = 0;

void timerCallback(uint32_t counterIndex) {
	// We use the optional parameter as an index
	// counterIndex is the index in timerCounter
    timerCounter[counterIndex]++;
    return;
}

void updateCounter(uint8_t btn) {
    if (btn & BUTTON_MASK_FIRE) {
        if (buttonPressed[0] == 0) {
            timerCounter[0] = 0;
        }
        //This if may be damaging to some functions
        if (timerCounter[0] > 600) {
            fireSpec = 0;
        }
        buttonPressed[0] = 1;

    }
    else if (!(btn & BUTTON_MASK_FIRE)) {
        if (timerCounter[0] < 600 && buttonPressed[0]) {
        fireSpec++;
        timerCounter[0] = 0;
        } else if (buttonPressed[0] || timerCounter[0] > 600) {
            fireSpec = 0;
        }

        buttonPressed[0] = 0;

        
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

int main(){
    char buf[100];
    uint16_t volts/*, newVolts, battVolts, displayVolts*/;
	uint32_t watts;
	uint8_t btnState/*, battPerc, boardTemp*/;
	Atomizer_Info_t atomInfo;
    
    Atomizer_ReadInfo(&atomInfo);
    
    watts = 20000; // Initial wattage
    volts = wattsToVolts(watts, atomInfo.resistance);
    Atomizer_SetOutputVoltage(volts);
    
    // Display On screen for 0.5 s.
    Display_PutPixels(0, 32, Bitmap_evicSdk, Bitmap_evicSdk_width, Bitmap_evicSdk_height);
    Display_Update();
    Timer_DelayMs(500);
    
    Timer_CreateTimeout(1,1, timerCallback, 0);
    while(1){
        btnState = Button_GetState();
        updateCounter(btnState);
        
        if (fireSpec > 1 && timerCounter[0] < 600) {
            shouldFire = 0;
        } else {
            shouldFire = 1;   
        }
        // Button logic, "don't" use switch, I've heard it's not efficient.
        //http://embeddedgurus.com/stack-overflow/2010/04/efficient-c-tip-12-be-wary-of-switch-statements/
        //
        
        //If should fire
        //TODO: Fix so that only fire if button has been pressed for x ms, x is time a normal user would press the button.
        if (!Atomizer_IsOn() && (btnState & BUTTON_MASK_FIRE) &&
            (atomInfo.resistance != 0) && shouldFire &&
            (Atomizer_GetError() == OK)) {
                // Take power on
                Atomizer_Control(1);
        } //If not firing
        else if(Atomizer_IsOn() && !(btnState & BUTTON_MASK_FIRE) || !shouldFire) {
			// Take power off
			Atomizer_Control(0);
		}
        Atomizer_ReadInfo(&atomInfo);
		
        volts = correctVoltage(volts, watts, atomInfo.resistance);
        Atomizer_SetOutputVoltage(volts);
        
        siprintf(buf, "Fired: %d\n %d", fireSpec,timerCounter[0]);
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
    }
    
}