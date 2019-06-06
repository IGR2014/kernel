////////////////////////////////////////////////////////////////
//
//	IgrOS platform description
//
//	File:	platform.hpp
//	Date:	06 Jun 2019
//
//	Copyright (c) 2017 - 2019, Igor Baklykov
//	All rights reserved.
//
//


#pragma once
#ifndef IGROS_PLATFORM_HPP
#define IGROS_PLATFORM_HPP


#include <type_traits>

#include <arch/types.hpp>


// Platform init function pointer type
using funcInit_t	= std::add_pointer<void()>::type;
// Platform shutdown function pointer type
using funcShutdown_t	= std::add_pointer<void()>::type;
// Platform reboot function pointer type
using funcReboot_t	= std::add_pointer<void()>::type;


// Platform desciption structure
struct platform {

	sbyte_t*		name;			// Platform name

        funcInit_t		init;			// Init function
        funcShutdown_t		shutdown;		// Shutdown function
        funcReboot_t		reboot;			// Reboot function

};


// Platform description
extern const platform	platformDescription;


#endif	// IGROS_PLATFORM_HPP
