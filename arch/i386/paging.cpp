////////////////////////////////////////////////////////////////
//
//	Memory paging for x86
//
//	File:	paging.cpp
//	Date:	06 Jun 2019
//
//	Copyright (c) 2017 - 2019, Igor Baklykov
//	All rights reserved.
//
//


#include <arch/cr.hpp>
#include <arch/exceptions.hpp>
#include <arch/paging.hpp>
#include <arch/taskRegs.hpp>

#include <drivers/vmem.hpp>

#include <klib/kprint.hpp>


// Arch-dependent code zone
namespace arch {


#ifdef	__cplusplus

	// Workaround because of unimplemented yet physical memory allocator
	extern "C" {

#endif	// __cplusplus


		// Page directory
		extern dword_t	pageDirectory[1024];
		// Page table
		extern dword_t	pageTable[1024];


#ifdef	__cplusplus

	}	// extern "C"

#endif	// __cplusplus


	// Setup page directory
	void pagingSetupPD(const pointer_t pageDirAddr) {

		// Write page directory address to CR3
		inCR3(reinterpret_cast<dword_t>(pageDirAddr));

	}

	// Flush page directory
	void pagingFlushPD() {

		// Get current value of CR3
		dword_t cr3 = outCR3();
		// Rewrite it again to CR3
		inCR3(cr3);

	}


	// Enable paging
	void pagingEnable() {

		// Get current value of CR0
		dword_t cr0 = outCR0();
		// Set bit 31 to "1"
		cr0 |= (1 << 31);
		// Rewrite CR0 with new value
		inCR0(cr0);

	}


	// Get address which is caused Page Fault Exception
	dword_t pagingGetFaultAddres() {

		// Simply read CR2 value inside ISR
		return outCR2();

	}


	// Setup paging
	void pagingSetup() {

		// Other page directories are unused
		for (dword_t i = 0; i < 1024; ++i) {

			// Pages marked as clear
			pageDirectory[i] = dword_t(pagingFlags_t::CLEAR);

		}

		// Map first 4 MB of physical RAM to first 4 MB of virtual RAM
		pageDirectory[0]	 = dword_t(pageTable);
		pageDirectory[0]	|= dword_t(pagingFlags_t::WRITABLE | pagingFlags_t::PRESENT);

		// Map all pages of first 4MB to first page table
		for (sdword_t j = 0; j < 1024; ++j) {

			pageTable[j]	 = (j << 12);
			pageTable[j]	|= dword_t(pagingFlags_t::WRITABLE | pagingFlags_t::PRESENT);

		}

		// Also map first 4MB of physical RAM to first 4MB after 3GB in virtual memory
		// (This should be useful for higher-half kernel)
		pageDirectory[768]	 = dword_t(pageTable);
		// Page marked as present and writable
		pageDirectory[768]	|= dword_t(pagingFlags_t::WRITABLE | pagingFlags_t::PRESENT);

		// Map page table itself to the last page of virtual memory
		pageDirectory[1023]	 = dword_t(pageDirectory);
		// Page marked as present and writable
		pageDirectory[1023]	|= dword_t(pagingFlags_t::WRITABLE | pagingFlags_t::PRESENT);

		// Install exception handler for page fault
		exHandlerInstall(PAGE_FAULT, pagingFaultExceptionHandler);

		// Setup page directory
		pagingSetupPD(pageDirectory);

		// Enable paging
		pagingEnable();

	}


	// Convert virtual address to physical address
	pointer_t pagingVirtToPhys(const pointer_t virtAddr) {

		// Page directory entry index from virtual address
		dword_t pdEntryIndex	= (dword_t(virtAddr) & 0xFFC00000) >> 22;
		// Page table entry index from virtual address
		dword_t ptEntryIndex	= (dword_t(virtAddr) & 0x3FF000) >> 12;

		// Physical pointer to page table
		dword_t		pageTablePtr	= pageDirectory[pdEntryIndex];
		pagingFlags_t	pageFlags	= pagingFlags_t(pageTablePtr);

		// Check if page table is present or not
		if ((pageFlags & pagingFlags_t::PRESENT) == pagingFlags_t::CLEAR) {

			// Page or table is not present
			return nullptr;

		}

		// Physical pointer to page
		dword_t pagePtr	= reinterpret_cast<dword_t*>(pageTablePtr & ~0xFFF)[ptEntryIndex];
		pageFlags	= pagingFlags_t(pagePtr);

		// Check if page is present or not
		if ((pageFlags & pagingFlags_t::PRESENT) == pagingFlags_t::CLEAR) {

			// Page or table is not present
			return nullptr;

		}

		// Get physical address of page from page table (20 MSB)
		dword_t physPageAddr	= pagePtr & ~0xFFF;
		// Get physical offset from virtual address`s (12 LSB)
		dword_t physPageOffset	= dword_t(virtAddr) & 0xFFF;

		// Return physical address
		return pointer_t(physPageAddr | physPageOffset);

	}


	// Page Fault Exception handler
	void pagingFaultExceptionHandler(const taskRegs_t* regs) {

		// Print buffer
		sbyte_t text[64];

		// Exception info
		vmemWrite("CAUSED BY:\t");
		vmemWrite(((regs->param & 0x18) == 0) ? "ACCESS VIOLATION" : "");
		vmemWrite(((regs->param & 0x10) == 0) ? "" : "INSTRUCTION FETCH");
		vmemWrite(((regs->param & 0x08) == 0) ? "" : "RESERVED BIT SET");
		vmemWrite("\r\nFROM:\t\t");
		vmemWrite(((regs->param & 0x04) == 0) ? "KERNEL" : "USER");
		vmemWrite(" space\r\n");
		vmemWrite("WHEN:\t\tattempting to ");
		vmemWrite(((regs->param & 0x02) == 0) ? "READ" : "WRITE");
		vmemWrite("\r\nADDRESS:\t0x");
		klib::kitoa(text, 64, static_cast<dword_t>(pagingGetFaultAddres()), klib::base::HEX);
		vmemWrite(text);
		vmemWrite("\r\nWHICH IS:\tNON-");
		vmemWrite(((regs->param & 0x01) == 0) ? "PRESENT\r\n" : "PRIVILEGED\r\n");
		vmemWrite("\r\n");

		// Hang here
		while (true) {};

	}


}	// namespace arch

