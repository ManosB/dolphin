// Copyright (C) 2010 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _DSP_JIT_UTIL_H
#define _DSP_JIT_UTIL_H

#include "../DSPMemoryMap.h"
#include "../DSPHWInterface.h"
#include "../DSPEmitter.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;


// HORRIBLE UGLINESS, someone please fix.
// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
void DSPEmitter::increment_addr_reg(int reg)
{
	//	u16 tmb = g_dsp.r[DSP_REG_WR0 + reg];
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_WR0 + reg]));

	//	tmb = tmb | (tmb >> 8);
	MOV(16, R(ECX), R(EAX));
	SHR(16, R(ECX), Imm8(8));
	OR(16, R(EAX), R(ECX));

	//	tmb = tmb | (tmb >> 4);
	MOV(16, R(ECX), R(EAX));
	SHR(16, R(ECX), Imm8(4));
	OR(16, R(EAX), R(ECX));

	//	tmb = tmb | (tmb >> 2);	
	MOV(16, R(ECX), R(EAX));
	SHR(16, R(ECX), Imm8(2));
	OR(16, R(EAX), R(ECX));

	//	tmb = tmb | (tmb >> 1);	
	MOV(16, R(ECX), R(EAX));
	SHR(16, R(ECX), Imm8(1));
	OR(16, R(EAX), R(ECX));
	
	//	s16 tmp = g_dsp.r[reg];
	MOVZX(32, 16, ECX, M(&g_dsp.r[reg]));

	// 	if ((tmp & tmb) == tmb)
	AND(16, R(ECX), R(EAX));
	CMP(16, R(EAX), R(ECX));
	FixupBranch not_equal = J_CC(CC_NZ);

	//	tmp ^= g_dsp.r[DSP_REG_WR0 + reg];
	MOVZX(32, 16, ECX, M(&g_dsp.r[reg]));
	XOR(16, R(ECX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	FixupBranch end = J();
	SetJumpTarget(not_equal);

	//		tmp++;
	MOVZX(32, 16, ECX, M(&g_dsp.r[reg]));
	ADD(16, R(ECX), Imm16(1));

	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(ECX));
}

// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
void DSPEmitter::decrement_addr_reg(int reg)
{
	//	s16 tmp = g_dsp.r[reg];
	MOVZX(32, 16, EAX, M(&g_dsp.r[reg]));

	//	if ((tmp & g_dsp.r[DSP_REG_WR0 + reg]) == 0)
	MOV(16, R(ECX), R(EAX));
	AND(16, R(ECX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
	//	CMP(16, R(ECX), Imm8(0));
	FixupBranch not_equal = J_CC(CC_NZ);

	//	tmp |= g_dsp.r[DSP_REG_WR0 + reg];
	OR(16, R(EAX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	FixupBranch end = J();
	SetJumpTarget(not_equal);
	//		tmp--;
	SUB(16, R(EAX), Imm8(1));

	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(EAX));

}

// Increase addr register according to the correspond ix register
void DSPEmitter::increase_addr_reg(int reg)
{	
	//	s16 value = (s16)g_dsp.r[DSP_REG_IX0 + reg];
	MOVSX(32, 16, EDX, M(&g_dsp.r[DSP_REG_IX0 + reg]));
	
	//	if (value > 0)
	CMP(16, R(EDX), Imm16(0));
	FixupBranch end = J_CC(CC_Z);
	FixupBranch negValue = J_CC(CC_L);
		
	//	for (int i = 0; i < value; i++) 
	XOR(32, R(ESI), R(ESI)); // i = 0

	FixupBranch posloop;
	SetJumpTarget(posloop);

	increment_addr_reg(reg);   

	ADD(32, R(ESI), Imm32(1)); // i++	
	CMP(32, R(ESI), R(EDX)); // i < value

	FixupBranch posValue = J();
	// FIXME: get normal abs with cdq
	IMUL(32, EDX, Imm16(-1)); 

	//	for (int i = 0; i < (int)(-value); i++)
	XOR(32, R(ESI), R(ESI)); // i = 0

	FixupBranch negloop;
	SetJumpTarget(negloop);

	decrement_addr_reg(reg);
	ADD(32, R(ESI), Imm32(1)); // i++
	CMP(32, R(ESI), R(EDX)); // i < -value

	negloop = J_CC(CC_L);

	SetJumpTarget(posValue);
	SetJumpTarget(end);

}

// Decrease addr register according to the correspond ix register
void DSPEmitter::decrease_addr_reg(int reg)
{
	//	s16 value = (s16)g_dsp.r[DSP_REG_IX0 + reg];
	MOVSX(32, 16, EDX, M(&g_dsp.r[DSP_REG_IX0 + reg]));
	
	//	if (value > 0)
	CMP(16, R(EDX), Imm16(0));
	FixupBranch end = J_CC(CC_Z);
	FixupBranch negValue = J_CC(CC_L);
		
	//	for (int i = 0; i < value; i++) 
	XOR(32, R(ESI), R(ESI)); // i = 0

	FixupBranch posloop;
	SetJumpTarget(posloop);

	decrement_addr_reg(reg);

	ADD(32, R(ESI), Imm32(1)); // i++	
	CMP(32, R(ESI), R(EDX)); // i < value

	FixupBranch posValue = J();
	// FIXME: get normal abs with cdq
	IMUL(32, EDX, Imm16(-1)); 

	//	for (int i = 0; i < (int)(-value); i++)
	XOR(32, R(ESI), R(ESI)); // i = 0

	FixupBranch negloop;
	SetJumpTarget(negloop);

	increment_addr_reg(reg);
	ADD(32, R(ESI), Imm32(1)); // i++
	CMP(32, R(ESI), R(EDX)); // i < -value

	negloop = J_CC(CC_L);

	SetJumpTarget(posValue);
	SetJumpTarget(end);
}

void DSPEmitter::ext_dmem_write(u32 dest, u32 src)
{

	u16 addr = g_dsp.r[dest];
	u16 val = g_dsp.r[src];
	u16 saddr = addr >> 12; 

	if (saddr == 0)
		g_dsp.dram[addr & DSP_DRAM_MASK] = val;
	else if (saddr == 0xf)
		gdsp_ifx_write(addr, val);

}

u16 DSPEmitter::ext_dmem_read(u16 addr)
{
	u16 saddr = addr >> 12; 
	if (saddr == 0)
		return g_dsp.dram[addr & DSP_DRAM_MASK];
	else if (saddr == 0x1)
		return g_dsp.coef[addr & DSP_COEF_MASK];
	else if (saddr == 0xf)
		return gdsp_ifx_read(addr);

	return 0;
}

#endif
