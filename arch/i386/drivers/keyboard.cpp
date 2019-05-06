////////////////////////////////////////////////////////////////
//
//	Keyboard generic handling
//
//	File:	keyboard.cpp
//	Date:	06 May 2019
//
//	Copyright (c) 2017 - 2019, Igor Baklykov
//	All rights reserved.
//
//


#include <arch/port.hpp>
#include <arch/interrupts.hpp>

#include <drivers/vmem.hpp>


// Arch-dependent code zone
namespace arch {


	// Keyboard interrupt (#1) handler
	void keyboardInterruptHandler(const taskRegs_t* regs) {

		vmemWrite("IRQ\t\t-> KEYBOARD\r\n");
		vmemWrite("KEY STATE:\t");

		byte_t keyStatus = inPort8(KEYBOARD_CONTROL);

		// Check keyboard data port
		if (keyStatus & 0x01) {

			byte_t keyCode = inPort8(KEYBOARD_DATA);

			if (keyCode > 0x80) {

				vmemWrite("KEY_RELEASED\r\n");

			} else {

				vmemWrite("KEY_PRESSED\r\n");

			}

			vmemWrite("Key CODE: ");
			//vmemWriteHex(keyCode & 0x7F);
			vmemWrite("\r\n\r\n");

		}

	}


	// Setip keyboard function
	void keyboardSetup() {

		// Install keyboard interrupt handler
		irqHandlerInstall(arch::irqNumber_t::KEYBOARD, arch::keyboardInterruptHandler);
		// Mask Keyboard interrupts
		irqMask(arch::irqNumber_t::KEYBOARD);

	}


}	// namespace arch

