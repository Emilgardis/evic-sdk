/*
 * This file is part of eVic SDK.
 *
 * eVic SDK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eVic SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eVic SDK.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2016 ReservedField
 */

/**
 * \file
 * Button library.
 * The GPIOs are pulled low when the
 * button is pressed.
 * The following pins are used:
 * - PE.0 (Fire button)
 * - PD.2 (Right button)
 * - PD.3 (Left button)
 */

#include <M451Series.h>
#include <Button.h>
#include <Thread.h>

/**
 * Button callback function pointers.
 */
static volatile Button_Callback_t Button_callbackPtr[3] = {NULL, NULL, NULL};

/**
 * Button callback masks.
 */
static volatile uint8_t Button_callbackMask[3];

/**
 * Global button state.
 */
static volatile uint8_t Button_state;

/**
 * Dirty hack: soft PD.7 interrupt handler.
 */
extern void GPD7_IRQHandler();

/**
 * Updates the global button state for the specified
 * buttons. Access to the global button state must be
 * externally synchronized.
 * This is an internal function.
 *
 * @param mask Button mask specifing which buttons to update.
 */
static void Button_UpdateState(uint8_t mask) {
	uint8_t curState;

	curState  = PE0 ? 0 : BUTTON_MASK_FIRE;
	curState |= PD2 ? 0 : BUTTON_MASK_RIGHT;
	curState |= PD3 ? 0 : BUTTON_MASK_LEFT;

	Button_state &= ~mask;
	Button_state |= curState & mask;
}

/**
 * GPD/GPE interrupt handler.
 * This is an internal function.
 */
static void Button_IRQHandler() {
	int i;
	uint8_t mask;

	// All button ISRs have the same priority, so global
	// state is already synchronized.

	// Dirty hack: invoke soft PD.7 interrupt handler
	if(GPIO_GET_INT_FLAG(PD, BIT7)) {
		GPD7_IRQHandler();
	}

	// Build callback mask
	mask  = GPIO_GET_INT_FLAG(PE, BIT0) ? BUTTON_MASK_FIRE : 0;
	mask |= GPIO_GET_INT_FLAG(PD, BIT2) ? BUTTON_MASK_RIGHT : 0;
	mask |= GPIO_GET_INT_FLAG(PD, BIT3) ? BUTTON_MASK_LEFT : 0;

	if(mask) {
		Button_UpdateState(mask);

		for(i = 0; i < 3; i++) {
			if(Button_callbackPtr[i] != NULL && Button_callbackMask[i] & mask) {
				Button_callbackPtr[i](Button_state);
			}
		}
	}

	// Clear all PD/PE interrupts
	PD->INTSRC = PD->INTSRC;
	PE->INTSRC = PE->INTSRC;
}
__attribute((alias("Button_IRQHandler"))) void GPD_IRQHandler();
__attribute((alias("Button_IRQHandler"))) void GPE_IRQHandler();

void Button_Init() {
	Button_state = 0;

	// Setup GPIOs
	GPIO_SetMode(PE, BIT0, GPIO_MODE_INPUT);
	GPIO_SetMode(PD, BIT2, GPIO_MODE_INPUT);
	GPIO_SetMode(PD, BIT3, GPIO_MODE_INPUT);

	// Enable debounce
	GPIO_ENABLE_DEBOUNCE(PE, BIT0);
	GPIO_ENABLE_DEBOUNCE(PD, BIT2);
	GPIO_ENABLE_DEBOUNCE(PD, BIT3);

	// Enable all interrupts, regardless of which are actually used.
	// Enabling/disabling selectively increases complexity and is not
	// worth the extremely small performance gain.
	GPIO_EnableInt(PE, 0, GPIO_INT_BOTH_EDGE);
	GPIO_EnableInt(PD, 2, GPIO_INT_BOTH_EDGE);
	GPIO_EnableInt(PD, 3, GPIO_INT_BOTH_EDGE);
	NVIC_EnableIRQ(GPE_IRQn);
	NVIC_EnableIRQ(GPD_IRQn);
}

uint8_t Button_GetState() {
	// Atomic
	return Button_state;
}

int8_t Button_CreateCallback(Button_Callback_t callback, uint8_t buttonMask) {
	int i;
	uint32_t primask;

	if(callback == NULL) {
		return -1;
	}

	primask = Thread_IrqDisable();

	// Find an unused callback
	for(i = 0; i < 3 && Button_callbackPtr[i] != NULL; i++);
	if(i == 3) {
		Thread_IrqRestore(primask);
		return -1;
	}

	// Setup callback
	Button_callbackMask[i] = buttonMask;
	Button_callbackPtr[i] = callback;

	Thread_IrqRestore(primask);
	return i;
}

void Button_DeleteCallback(int8_t index) {
	if(index < 0 || index > 2) {
		// Invalid index
		return;
	}

	// Atomic
	Button_callbackPtr[index] = NULL;
}
