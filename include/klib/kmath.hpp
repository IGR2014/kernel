////////////////////////////////////////////////////////////////
//
//	Kernel math functions definitions
//
//	File:	kmath.hpp
//	Date:	12 Feb 2020
//
//	Copyright (c) 2017 - 2020, Igor Baklykov
//	All rights reserved.
//
//


#pragma once
#ifndef IGROS_KLIB_KERNEL_MATH_HPP
#define IGROS_KLIB_KERNEL_MATH_HPP


#include <cstdint>

#include <types.hpp>


// Kernel library code zone
namespace klib {


	// Combined division & modulo result (unsigned)
	struct udivmod_t {
		quad_t quotient;
		quad_t reminder;
	};

	// Combined division & modulo result (signed)
	struct divmod_t {
		squad_t quotient;
		squad_t reminder;
	};


	// Divide 64-bit integer by 32-bit integer
	// Returns 64-bit quotient and 64-bit reminder
	udivmod_t	kudivmod(quad_t dividend, dword_t divisor) noexcept;

	// Divide 64-bit integer by 32-bit integer
	// Returns 64-bit quotient and 64-bit reminder
	divmod_t	kdivmod(squad_t dividend, sdword_t divisor) noexcept;


}	// namespace klib


#endif	// IGROS_KLIB_KERNEL_MATH_HPP

