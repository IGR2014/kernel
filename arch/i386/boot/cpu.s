################################################################
#
#	CPU operations
#
#	File:	cpu.s
#	Date:	26 Jun 2020
#
#	Copyright (c) 2017 - 2020, Igor Baklykov
#	All rights reserved.
#
#


.code32

.section .text
.balign 4
.global cpuHalt			# halt CPU


# Halt CPU
cpuHalt:
1:
	hlt
	jmp 1b;
