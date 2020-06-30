////////////////////////////////////////////////////////////////
//
//	CPU operations
//
//	File:	cpu.hpp
//	Date:	26 Jun 2020
//
//	Copyright (c) 2017 - 2020, Igor Baklykov
//	All rights reserved.
//
//


#pragma once


#include <types.hpp>


// Arch-dependent code zone
namespace arch {


#ifdef	__cplusplus

	extern "C" {

#endif	// __cplusplus


		// Read CR0 register
		inline void cpuHalt() noexcept;


#ifdef	__cplusplus

	}	// extern "C"

#endif	// __cplusplus


}	// namespace arch
