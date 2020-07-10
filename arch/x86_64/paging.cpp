////////////////////////////////////////////////////////////////
//
//	Memory paging for x86
//
//	File:	paging.cpp
//	Date:	10 Jul 2020
//
//	Copyright (c) 2017 - 2020, Igor Baklykov
//	All rights reserved.
//
//


#include <msr.hpp>
#include <cr.hpp>
#include <irq.hpp>
#include <exceptions.hpp>
#include <paging.hpp>
#include <taskRegs.hpp>
#include <cpu.hpp>

#include <klib/kalign.hpp>
#include <klib/kmemory.hpp>
#include <klib/kprint.hpp>


// Kernel start and end
extern const igros::byte_t _SECTION_KERNEL_START_;
extern const igros::byte_t _SECTION_KERNEL_END_;


// Arch-dependent code zone
namespace igros::arch {


	// Free pages list
	table_t* paging::mFreePages = reinterpret_cast<table_t*>(&paging::mFreePages);


	// Setup paging
	void paging::init() noexcept {

		// Install exception handler for page fault
		except::install(except::NUMBER::PAGE_FAULT, paging::exHandler);

		// Initialize pages for page tables
		paging::heap(const_cast<byte_t*>(&_SECTION_KERNEL_END_), 4096ull << 6);

		// Create flags
		constexpr auto flags = flags_t::WRITABLE | flags_t::PRESENT;
		// Create page map level 4
		auto pml4 = paging::makePML4();
		// Identity map first 4MB of physical memory to first 4MB in virtual memory
		paging::mapPage(pml4, nullptr, nullptr, flags);
		paging::mapPage(pml4, reinterpret_cast<const page_t*>(0x0000000002000000), reinterpret_cast<const pointer_t>(0x0000000002000000), flags);
		// Also map first 4MB of physical memory to 128TB offset in virtual memory
		paging::mapPage(pml4, nullptr, reinterpret_cast<const pointer_t>(0xFFFFFFFF80000000), flags);
		// Map page directory to itself
		//pml4->pointers[511] = reinterpret_cast<directoryPointer_t*>(reinterpret_cast<std::size_t>(pml4) & 0x7FFFFFFF | static_cast<dword_t>(flags));

		// Setup page directory
		// PD address bits ([0 .. 63] in cr3)
		paging::setDirectory(pml4);
		// Enable Physical Address Extension
		paging::enablePAE();
		// Enable paging
		paging::enable();

	}


	// Enable paging
	void paging::enable() noexcept {
		// Set paging bit on in CR0
		inCR0(outCR0() | 0x0000000080000000);
	}

	// Disable paging
	void paging::disable() noexcept {
		// Set paging bit off in CR0
		inCR0(outCR0() & 0xFFFFFFFF7FFFFFFF);
	}


	// Enable Physical Address Extension
	void paging::enablePAE() noexcept {
		// Set paging bit on in CR0
		inCR4(outCR4() | 0x0000000000000020);
	}

	// Disable Physical Address Extension
	void paging::disablePAE() noexcept {
		// Set paging bit off in CR0
		inCR4(outCR4() & 0xFFFFFFFFFFFFFFDF);
	}


	// Initialize paging heap
	void paging::heap(const pointer_t phys, const std::size_t size) noexcept {

		// Temporary data
		auto tempPhys	= klib::kalignUp(phys, 12ull);
		auto tempSize	= size - (reinterpret_cast<std::size_t>(tempPhys) - reinterpret_cast<std::size_t>(phys));

		// Get number of pages
		auto numOfPages = (tempSize >> 12ull);
		// Check input
		if (0ull == numOfPages) {
			return;
		}

		// Convert to page pointer
		auto page	= static_cast<table_t*>(tempPhys);
		// Link first page to free pages list
		page[0ull].next	= paging::mFreePages;
		// Create linked list of free pages
		for (auto i = 1ull; i < numOfPages; i++) {
			// Link each next page to previous
			page[i].next = &page[i - 1ull];
		}
		// Make last page new list head
		paging::mFreePages = &page[numOfPages - 2ull];

	}


	// Allocate page
	[[nodiscard]] pointer_t paging::allocate() noexcept {
		// Check if pages exist
		if (paging::mFreePages->next != paging::mFreePages) {
			// Get free page
			auto addr		= paging::mFreePages;
			// Update free pages list
			paging::mFreePages	= static_cast<table_t*>(addr->next);
			// Return pointer to free page
			return reinterpret_cast<page_t*>(addr);
		}
		// Nothing to return
		return nullptr;
	}

	// Deallocate page
	void paging::deallocate(const pointer_t page) noexcept {
		// Check alignment
		if (!klib::kalignCheck(page, 12ull)) {
			return;
		}
		// Deallocate page back to heap free list
		static_cast<table_t*>(page)->next = paging::mFreePages;
		paging::mFreePages = static_cast<table_t*>(page);
	}


	// Make PML4
	pml4_t* paging::makePML4() noexcept {
		// Allocate page map level 4
		auto pml4 = static_cast<pml4_t*>(paging::allocate());
		// Zero enties of page map level 4
		klib::kmemset(pml4, (sizeof(pml4_t) >> 2), static_cast<quad_t>(flags_t::CLEAR));
		// Return page map level 4
		return pml4;
	}

	// Make page directory pointer
	directoryPointer_t* paging::makeDirectoryPointer() noexcept {
		// Allocate page directory pointer
		auto dirPtr = static_cast<directoryPointer_t*>(paging::allocate());
		// Zero enties of page directory pointer
		klib::kmemset(dirPtr, (sizeof(directoryPointer_t) >> 2), static_cast<quad_t>(flags_t::CLEAR));
		// Return page directory pointer
		return dirPtr;
	}

	// Make page directory
	directory_t* paging::makeDirectory() noexcept {
		// Allocate page directory
		auto dir = static_cast<directory_t*>(paging::allocate());
		// Zero enties of page directory
		klib::kmemset(dir, (sizeof(directory_t) >> 2), static_cast<quad_t>(flags_t::CLEAR));
		// Return page directory
		return dir;
	}

	// Make page table
	table_t* paging::makeTable() noexcept {
		// Allocate page table
		auto table = static_cast<table_t*>(paging::allocate());
		// Zero enties of page table
		klib::kmemset(table, (sizeof(table_t) >> 2), static_cast<quad_t>(flags_t::CLEAR));
		// Return page table
		return table;
	}


	// Check directory pointer flags
	bool paging::checkFlags(const directoryPointer_t* dirPtr, const flags_t &flags) noexcept {
		// Mask flags
		auto maskedFlags = flags & flags_t::FLAGS_MASK;
		// Check flags
		return maskedFlags != (static_cast<flags_t>(reinterpret_cast<std::size_t>(dirPtr)) & maskedFlags);
	}

	// Check directory flags
	bool paging::checkFlags(const directory_t* dir, const flags_t &flags) noexcept {
		// Mask flags
		auto maskedFlags = flags & flags_t::FLAGS_MASK;
		// Check flags
		return maskedFlags != (static_cast<flags_t>(reinterpret_cast<std::size_t>(dir)) & maskedFlags);
	}

	// Check table flags
	bool paging::checkFlags(const table_t* table, const flags_t &flags) noexcept {
		// Mask flags
		auto maskedFlags = flags & flags_t::FLAGS_MASK;
		// Check flags
		return maskedFlags != (static_cast<flags_t>(reinterpret_cast<std::size_t>(table)) & maskedFlags);
	}

	// Check page flags
	bool paging::checkFlags(const page_t* page, const flags_t &flags) noexcept {
		// Mask flags
		auto maskedFlags = flags & flags_t::FLAGS_MASK;
		// Check flags
		return maskedFlags != (static_cast<flags_t>(reinterpret_cast<std::size_t>(page)) & maskedFlags);
	}


	// Map virtual page to physical page (whole pml4, explicit pml4)
	void paging::mapPML4(pml4_t* pml4, const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {

		// Check alignment
		if (!klib::kalignCheck(phys, PAGE_SHIFT) || !klib::kalignCheck(virt, PAGE_SHIFT)) {
			// Bad align detected
			return;
		}

		// Get page pointer and map physical page
		for (auto i = 0u; i < (sizeof(pml4_t) >> 3); i++) {
			// Get phys directory ID
			auto dirPtr		= static_cast<flags_t>(reinterpret_cast<std::size_t>(phys) + (i << PAGE_SHIFT));
			// Map page
			pml4->pointers[i]	= reinterpret_cast<directoryPointer_t*>(dirPtr | (flags & flags_t::FLAGS_MASK));
		}

	}

	// Map virtual page to physical page (whole pml4)
	void paging::mapPML4(const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {
		// Get pointer to page map level 4
		auto pml4 = reinterpret_cast<pml4_t*>(outCR3());
		// Map page to curent page map level 4
		paging::mapPML4(pml4, phys, virt, flags);
		// Setup page map level 4
		// PML4 address bits ([0 .. 63] in cr3)
		paging::setDirectory(pml4);
		// Enable paging
		paging::enable();
	}


	// Map virtual page to physical page (single directory pointer, explicit pml4)
	void paging::mapDirectoryPointer(pml4_t* pml4, const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {

		// Check alignment
		if (!klib::kalignCheck(phys, PAGE_SHIFT) || !klib::kalignCheck(virt, PAGE_SHIFT)) {
			// Bad align detected
			return;
		}

		// Page map level 4 table table index from virtual address
		auto pml4ID	= (reinterpret_cast<std::size_t>(virt) >> 39) & 0x1FF;

		// Get page directory pointer
		auto &dirPtr	= pml4->pointers[pml4ID];
		// Check if page directory pointer is present or not
		if (!paging::checkFlags(dirPtr, flags_t::PRESENT)) {
			// Allocate page directory pointer
			dirPtr = paging::makeDirectoryPointer();
		}

		// Get page pointer and map physical page
		for (auto i = 0u; i < (sizeof(directoryPointer_t) >> 3); i++) {
			// Get phys directory ID
			auto directory		= static_cast<flags_t>(reinterpret_cast<std::size_t>(phys) + (i << PAGE_SHIFT));
			// Map page
			dirPtr->directories[i]	= reinterpret_cast<directory_t*>(directory | (flags & flags_t::FLAGS_MASK));
		}

		// Insert page directory pointer
		dirPtr	= reinterpret_cast<directoryPointer_t*>((reinterpret_cast<std::size_t>(dirPtr) & 0x7FFFFFFF) | static_cast<std::size_t>(flags & flags_t::FLAGS_MASK));

	}

	// Map virtual page to physical page (single directory pointer)
	void paging::mapDirectoryPointer(const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {
		// Get pointer to page map level 4
		auto pml4 = reinterpret_cast<pml4_t*>(outCR3());
		// Map page to curent page map level 4
		paging::mapDirectoryPointer(pml4, phys, virt, flags);
		// Setup page map level 4
		// PML4 address bits ([0 .. 63] in cr3)
		paging::setDirectory(pml4);
		// Enable paging
		paging::enable();
	}


	// Map virtual page to physical page (single directory, explicit pml4)
	void paging::mapDirectory(pml4_t* pml4, const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {

		// Check alignment
		if (!klib::kalignCheck(phys, 12ull) || !klib::kalignCheck(virt, 12ull)) {
			// Bad align detected
			return;
		}

		// Page map level 4 table table index from virtual address
		auto pml4ID	= (reinterpret_cast<std::size_t>(virt) >> 39) & 0x1FF;
		// Page directory pointer table index from virtual address
		auto dirPtrID	= (reinterpret_cast<std::size_t>(virt) >> 30) & 0x1FF;
		// Page directory table entry index from virtual address
		auto dirID	= (reinterpret_cast<std::size_t>(virt) >> 21) & 0x1FF;

		// Get page directory pointer
		auto &dirPtr	= pml4->pointers[pml4ID];
		// Check if page directory pointer is present or not
		if (!paging::checkFlags(dirPtr, flags_t::PRESENT)) {
			// Allocate page directory pointer
			dirPtr = paging::makeDirectoryPointer();
		}

		// Get page directory
		auto &dir	= dirPtr->directories[dirPtrID];
		// Check if page directory is present or not
		if (!paging::checkFlags(dir, flags_t::PRESENT)) {
			// Allocate page table
			dir = paging::makeDirectory();
		}

		// Get page pointer and map physical page
		for (auto i = 0u; i < (sizeof(page_t) >> 3); i++) {
			// Get phys page ID
			auto page	= static_cast<flags_t>(reinterpret_cast<std::size_t>(phys) + (i << PAGE_SHIFT));
			// Map page
			dir->tables[i]	= reinterpret_cast<table_t*>(page | (flags & flags_t::FLAGS_MASK));
		}

		// Insert page directory pointer
		dirPtr	= reinterpret_cast<directoryPointer_t*>((reinterpret_cast<std::size_t>(dirPtr) & 0x7FFFFFFF) | static_cast<std::size_t>(flags & flags_t::FLAGS_MASK));
		// Insert page directory
		dir	= reinterpret_cast<directory_t*>((reinterpret_cast<std::size_t>(dir) & 0x7FFFFFFF) | static_cast<std::size_t>(flags & flags_t::FLAGS_MASK));

	}

	// Map virtual page to physical page (single directory)
	void paging::mapDirectory(const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {
		// Get pointer to page map level 4
		auto pml4 = reinterpret_cast<pml4_t*>(outCR3());
		// Map page to curent page map level 4
		paging::mapDirectory(pml4, phys, virt, flags);
		// Setup page map level 4
		// PML4 address bits ([0 .. 63] in cr3)
		paging::setDirectory(pml4);
		// Enable paging
		paging::enable();
	}


	// Map virtual page to physical page (single page, explicit page directory)
	void paging::mapPage(pml4_t* pml4, const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {

		// Check alignment
		if (!klib::kalignCheck(phys, 12ull) || !klib::kalignCheck(virt, 12ull)) {
			// Bad align detected
			return;
		}

		// Page map level 4 table table index from virtual address
		auto pml4ID	= (reinterpret_cast<std::size_t>(virt) >> 39) & 0x1FF;
		// Page directory pointer table index from virtual address
		auto dirPtrID	= (reinterpret_cast<std::size_t>(virt) >> 30) & 0x1FF;
		// Page directory table entry index from virtual address
		auto dirID	= (reinterpret_cast<std::size_t>(virt) >> 21) & 0x1FF;

		// Get page directory pointer
		auto &dirPtr	= pml4->pointers[pml4ID];
		// Check if page directory pointer is present or not
		if (!paging::checkFlags(dirPtr, flags_t::PRESENT)) {
			// Allocate page directory pointer
			dirPtr = paging::makeDirectoryPointer();
		}

		// Get page directory
		auto &dir	= dirPtr->directories[dirPtrID];
		// Check if page directory is present or not
		if (!paging::checkFlags(dir, flags_t::PRESENT)) {
			// Allocate page table
			dir = paging::makeDirectory();
		}

		// Insert page directory pointer
		dirPtr = reinterpret_cast<directoryPointer_t*>((reinterpret_cast<std::size_t>(dirPtr) & 0x7FFFFFFF) | static_cast<std::size_t>(flags & flags_t::FLAGS_MASK));

		// Get page pointer
		dir->tables[dirID] = reinterpret_cast<table_t*>(static_cast<flags_t>(reinterpret_cast<std::size_t>(phys)) | flags_t::HUGE | (flags & flags_t::FLAGS_MASK));
		// Insert page directory
		dir = reinterpret_cast<directory_t*>((reinterpret_cast<std::size_t>(dir) & 0x7FFFFFFF) | static_cast<std::size_t>(flags & flags_t::FLAGS_MASK));

	}

	// Map virtual page to physical page (single page)
	void paging::mapPage(const page_t* phys, const pointer_t virt, const flags_t flags) noexcept {
		// Get pointer to page map level 4
		auto pml4 = reinterpret_cast<pml4_t*>(outCR3());
		// Map page to curent page map level 4
		paging::mapPage(pml4, phys, virt, flags);
		// Setup page map level 4
		// PML4 address bits ([0 .. 63] in cr3)
		paging::setDirectory(pml4);
		// Enable paging
		paging::enable();
	}


	// Convert virtual address to physical address
	pointer_t paging::toPhys(const pointer_t virt) noexcept {

		// Page map level 4 table table index from virtual address
		auto pml4ID	= (reinterpret_cast<std::size_t>(virt) >> 39) & 0x1FF;
		// Page directory pointer table index from virtual address
		auto dirPtrID	= (reinterpret_cast<std::size_t>(virt) >> 30) & 0x1FF;
		// Page directory table entry index from virtual address
		auto dirID	= (reinterpret_cast<std::size_t>(virt) >> 21) & 0x1FF;

		// Get pointer to pml4
		auto pml4	= reinterpret_cast<const pml4_t*>(outCR3());
		// Get page directory pointer
		auto dirPtr	= pml4->pointers[pml4ID];
		// Check if page directory pointer is present or not
		if (!paging::checkFlags(dirPtr, flags_t::PRESENT)) {
			// Page or table is not present
			return nullptr;
		}

		// Get page directory
		auto dir	= dirPtr->directories[dirPtrID];
		// Check if page directory is present or not
		if (!paging::checkFlags(dir, flags_t::PRESENT)) {
			// Page or table is not present
			return nullptr;
		}

		// Get page table pointer
		auto page	= dir->tables[dirID];
		// Check if page table is present or not
		if (!paging::checkFlags(page, flags_t::PRESENT)) {
			// Page or table is not present
			return nullptr;
		}

		// Get physical address of page from page table (52 MSB)
		auto address	= static_cast<flags_t>(reinterpret_cast<std::size_t>(page)) & flags_t::PHYS_ADDR_MASK;
		// Get physical offset in psge from virtual address`s (21 LSB)
		auto offset	= static_cast<flags_t>(reinterpret_cast<std::size_t>(virt)) & flags_t::FLAGS_MASK;
		// Return physical address
		return reinterpret_cast<pointer_t>(address | offset);

	}


	// Page Fault Exception handler
	void paging::exHandler(const taskRegs_t* regs) noexcept {

		// Disable IRQ
		irq::disable();

		// Write Multiboot magic error message message
		klib::kprintf(	u8"EXCEPTION [#%d]\t-> (%s)\r\n"
				u8"CAUSED BY:\t%s%s%s\r\n"
				u8"FROM:\t\t%s space\r\n"
				u8"WHEN:\t\tattempting to %s\r\n"
				u8"ADDRESS:\t0x%p\r\n"
				u8"WHICH IS:\tnot %s\r\n",
				static_cast<dword_t>(except::NUMBER::PAGE_FAULT),
				except::NAME[static_cast<dword_t>(except::NUMBER::PAGE_FAULT)],
				((regs->param & 0x18) == 0u) ? u8"ACCESS VIOLATION"	: u8"",
				((regs->param & 0x10) == 0u) ? u8""			: u8"INSTRUCTION FETCH",
				((regs->param & 0x08) == 0u) ? u8""			: u8"RESERVED BIT SET",
				((regs->param & 0x04) == 0u) ? u8"KERNEL"		: u8"USER",
				((regs->param & 0x02) == 0u) ? u8"READ"			: u8"WRITE",
				reinterpret_cast<const pointer_t>(outCR2()),
				((regs->param & 0x01) == 0u) ? u8"PRESENT"		: u8"PRIVILEGED");

		// Hang here
		cpuHalt();

	}


	// Set page directory
	void paging::setDirectory(const pml4_t* dir) noexcept {
		// Set page directory address to CR3
		inCR3(reinterpret_cast<quad_t>(dir) & 0x7FFFFFFF);
	}


}	// namespace igros::arch

