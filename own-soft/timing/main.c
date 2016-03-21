#include <stdio.h>
#include <M451Series.h>
#include <Display.h>
#include <Font.h>
#include <Button.h>

volatile uint32_t timerCounter[1] = {0};
volatile uint8_t stopped = 0;
void timerCallback(uint32_t counterIndex) {
	// We use the optional parameter as an index
	// counterIndex is the index in timerCounter
    if (!stopped) {
	    timerCounter[counterIndex]++;
    }
}
int main() {
	uint8_t state;
	char buf[100];
    int8_t slot = -1;
	while(1) {
		// Build state report
        
        
		state = Button_GetState();
		
        if ((state & BUTTON_MASK_FIRE)) {
            if (stopped) {
                stopped = 0;
                timerCounter[0] = 0;
            }
            slot = Timer_CreateTimeout(1, 1, timerCallback, 0);
        }
        
        if (!(state & BUTTON_MASK_FIRE)) {
            stopped = 1;
            Timer_DeleteTimer(slot);
        }
        siprintf(buf, "Mask: %02X\n\nFire: %d\nFired\n for (ms)\n: %d",
			state,
			(state & BUTTON_MASK_FIRE) ? 1 : 0,
			timerCounter[0]);

		// Clear and blit text
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
	}
}
