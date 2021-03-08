/*	Copyright (C) 2006 yopyop
	Copyright (C) 2011 Loren Merritt
	Copyright (C) 2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "types.h"

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#include "instructions.h"
#include "instruction_attributes.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "arm_jit.h"
#include "bios.h"
#include "armcpu.h"
#include "PSP/emit/psp_emit.h"
#include "PSP/FrontEnd.h"

#include <pspsuspend.h>


typedef bool (FASTCALL* BOOLArmOpCompiled)();

u32 saveBlockSizeJIT = 0;

CACHE_ALIGN JIT_struct JIT;

uintptr_t *JIT_struct::JIT_MEM[2][0x4000] = {{0}};

static uintptr_t *JIT_MEM[2][32] = {
   //arm9
   {
      /* 0X*/  DUP2(JIT.ARM9_ITCM),
      /* 1X*/  DUP2(JIT.ARM9_ITCM), // mirror
      /* 2X*/  DUP2(JIT.MAIN_MEM),
      /* 3X*/  DUP2(JIT.SWIRAM),
      /* 4X*/  DUP2(NULL),
      /* 5X*/  DUP2(NULL),
      /* 6X*/      NULL, 
                JIT.ARM9_LCDC,   // Plain ARM9-CPU Access (LCDC mode) (max 656KB)
      /* 7X*/  DUP2(NULL),
      /* 8X*/  DUP2(NULL),
      /* 9X*/  DUP2(NULL),
      /* AX*/  DUP2(NULL),
      /* BX*/  DUP2(NULL),
      /* CX*/  DUP2(NULL),
      /* DX*/  DUP2(NULL),
      /* EX*/  DUP2(NULL),
      /* FX*/  DUP2(JIT.ARM9_BIOS)
   },
   //arm7
   {
      /* 0X*/  DUP2(JIT.ARM7_BIOS),
      /* 1X*/  DUP2(NULL),
      /* 2X*/  DUP2(JIT.MAIN_MEM),
      /* 3X*/       JIT.SWIRAM,
                   JIT.ARM7_ERAM,
      /* 4X*/       NULL,
                   JIT.ARM7_WIRAM,
      /* 5X*/  DUP2(NULL),
      /* 6X*/      JIT.ARM7_WRAM,      // VRAM allocated as Work RAM to ARM7 (max. 256K)
                NULL,
      /* 7X*/  DUP2(NULL),
      /* 8X*/  DUP2(NULL),
      /* 9X*/  DUP2(NULL),
      /* AX*/  DUP2(NULL),
      /* BX*/  DUP2(NULL),
      /* CX*/  DUP2(NULL),
      /* DX*/  DUP2(NULL),
      /* EX*/  DUP2(NULL),
      /* FX*/  DUP2(NULL)
      }
};

static u32 JIT_MASK[2][32] = {
   //arm9
   {
      /* 0X*/  DUP2(0x00007FFF),
      /* 1X*/  DUP2(0x00007FFF),
      /* 2X*/  DUP2(0x003FFFFF), // FIXME _MMU_MAIN_MEM_MASK
      /* 3X*/  DUP2(0x00007FFF),
      /* 4X*/  DUP2(0x00000000),
      /* 5X*/  DUP2(0x00000000),
      /* 6X*/      0x00000000,
                0x000FFFFF,
      /* 7X*/  DUP2(0x00000000),
      /* 8X*/  DUP2(0x00000000),
      /* 9X*/  DUP2(0x00000000),
      /* AX*/  DUP2(0x00000000),
      /* BX*/  DUP2(0x00000000),
      /* CX*/  DUP2(0x00000000),
      /* DX*/  DUP2(0x00000000),
      /* EX*/  DUP2(0x00000000),
      /* FX*/  DUP2(0x00007FFF)
   },
   //arm7
   {
      /* 0X*/  DUP2(0x00003FFF),
      /* 1X*/  DUP2(0x00000000),
      /* 2X*/  DUP2(0x003FFFFF),
      /* 3X*/       0x00007FFF,
                   0x0000FFFF,
      /* 4X*/       0x00000000,
                   0x0000FFFF,
      /* 5X*/  DUP2(0x00000000),
      /* 6X*/      0x0003FFFF,
                0x00000000,
      /* 7X*/  DUP2(0x00000000),
      /* 8X*/  DUP2(0x00000000),
      /* 9X*/  DUP2(0x00000000),
      /* AX*/  DUP2(0x00000000),
      /* BX*/  DUP2(0x00000000),
      /* CX*/  DUP2(0x00000000),
      /* DX*/  DUP2(0x00000000),
      /* EX*/  DUP2(0x00000000),
      /* FX*/  DUP2(0x00000000)
      }
};

static void init_jit_mem()
{
   static bool inited = false;
   if(inited)
      return;
   inited = true;
   for(int proc=0; proc<2; proc++)
      for(int i=0; i<0x4000; i++)
         JIT.JIT_MEM[proc][i] = JIT_MEM[proc][i>>9] + (((i<<14) & JIT_MASK[proc][i>>9]) >> 1);
}

static bool thumb = false;


#define CODE_SIZE   (4*1024*1024)

u8 __attribute__((aligned(64)))CodeCache[CODE_SIZE];

u32* emit_ptr=0;
u32 LastAddr = 0;
void* emit_GetCCPtr() { return (void*)&CodeCache[LastAddr]; }

void make_address_range_executable(u32 address_start, u32 address_end)
{
	address_start = (address_start + 0) & -64;
	address_end   = (address_end   +63) & -64;

	for (; address_start < address_end; address_start += 64)
	{
		__builtin_allegrex_cache(0x1a, address_start);
		__builtin_allegrex_cache(0x1a, address_start);
		__builtin_allegrex_cache(0x08, address_start);
		__builtin_allegrex_cache(0x08, address_start);
	}
}

void emit_Write32(u32 data)
{
	*(u32*)&CodeCache[LastAddr]=data;
	LastAddr+=4;
}

void insert_instruction(psp_insn_t insn)
{
	emit_Write32(insn.word);
}

inline bool is_s8(u32 v) { return (s8)v==(s32)v; }
inline bool is_u8(u32 v) { return (u8)v==(s32)v; }
inline bool is_s16(u32 v) { return (s16)v==(s32)v; }
inline bool is_u16(u32 v) { return (u16)v==(u32)v; }

//1+n opcodes
static void emit_mpush(u32 n, ...)
{
	va_list ap;
	va_start(ap, n);
	emit_addiu(psp_sp, psp_sp, u16(-4*n));
	while (n--)
	{
		u32 reg = va_arg(ap, u32);
		emit_sw(reg,psp_sp,u16(4*n));
	}
	va_end(ap);
}
//1+n opcodes
static void emit_mpop(u32 n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (u32 i = n; i--> 0;)
    {
        u32 reg = va_arg(ap, u32);
        emit_lw(reg,psp_sp,u16(4*i));
    }
    va_end(ap);
    emit_addiu(psp_sp, psp_sp, u16(+4*n));
}

static bool bit(uint32_t value, uint32_t bit)
{
   return value & (1 << bit);
}

static void emit_prefetch(const u32 pc);
static void sync_r15(u32 opcode, bool is_last, bool force);

static u32 instr_attributes(u32 opcode)
{
   return thumb ? thumb_attributes[opcode>>6]
                : instruction_attributes[INSTRUCTION_INDEX(opcode)];
}

static bool instr_is_branch(u32 opcode)
{
   u32 x = instr_attributes(opcode);
   if(thumb)
      return (x & BRANCH_ALWAYS)
          || ((x & BRANCH_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
          || (x & BRANCH_SWI)
          || (x & JIT_BYPASS);
   else
      return (x & BRANCH_ALWAYS)
          || ((x & BRANCH_POS12) && REG_POS(opcode,12) == 15)
          || ((x & BRANCH_LDM) && BIT15(opcode))
          || (x & BRANCH_SWI)
          || (x & JIT_BYPASS);
}

enum OP_RESULT { OPR_CONTINUE, OPR_INTERPRET, OPR_BRANCHED, OPR_RESULT_SIZE = 2147483647 };
#define OPR_RESULT(result, cycles) (OP_RESULT)((result) | ((cycles) << 16));
#define OPR_RESULT_CYCLES(result) ((result >> 16))
#define OPR_RESULT_ACTION(result) ((result & 0xFF))

typedef OP_RESULT (*ArmOpCompiler)(uint32_t pc, uint32_t opcode);

static u8 recompile_counts[(1<<26)/16];

#define RCPU  psp_k0
//#define RCYC  4
#define RN  psp_s4
#define RZ  psp_s5
#define RC  psp_s6
#define RQ  psp_s7

static uint32_t block_procnum;

#define _ARMPROC (block_procnum ? NDS_ARM7 : NDS_ARM9)
#define reg_offset(x) ((u32)(((u8*)&_ARMPROC.R[x]) - ((u8*)&_ARMPROC)))
#define reg_pos_offset(x) ((u32)(((u8*)&_ARMPROC.R[REG_POS(i,x)]) - ((u8*)&_ARMPROC)))
#define instrAdr_offset ((u32)(((u8*)&_ARMPROC.instruct_adr) - ((u8*)&_ARMPROC)))
#define NextInstr_offset ((u32)(((u8*)&_ARMPROC.next_instruction) - ((u8*)&_ARMPROC)))
#define instr_offset ((u32)(((u8*)&_ARMPROC.instruction) - ((u8*)&_ARMPROC)) )

/*#define flagC_offset ((u32)(((u8*)&_ARMPROC.CPSR.bits.C) - ((u8*)&_ARMPROC)) )
#define flagN_offset ((u32)(((u8*)&_ARMPROC.CPSR.bits.N) - ((u8*)&_ARMPROC)) )
#define flagZ_offset ((u32)(((u8*)&_ARMPROC.CPSR.bits.Z) - ((u8*)&_ARMPROC)) )
#define flagQ_offset ((u32)(((u8*)&_ARMPROC.CPSR.bits.Q) - ((u8*)&_ARMPROC)) )*/

#define changeCPSR { \
			emit_jal(NDS_Reschedule); \
}


template <int PROCNUM>
void arm_jit_prefetch(uint32_t pc, uint32_t opcode, bool thumb)
{
   const u8 isize = thumb ? 2 : 4;

   emit_lui(psp_a0, pc >> 16);
   emit_ori(psp_a0, psp_a0, pc &0xffff);

   emit_sw(psp_a0, psp_k0, instrAdr_offset);

   emit_addiu(psp_a0, psp_a0, isize);
   emit_sw(psp_a0, psp_k0, NextInstr_offset);

   emit_addiu(psp_a0, psp_a0, isize);
   emit_sw(psp_a0, psp_k0, reg_offset(15));

   emit_sw(psp_gp, psp_k0, instr_offset);

   return;
}


/////////
/// ARM
/////////

//rhs = psp_a0
//imm = psp_a1
//tmp = psp_at

#define LSL_IMM \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
	emit_lw(psp_a0,RCPU, reg_pos_offset(0)); \
	if(imm) emit_sll(psp_a0,psp_a0, imm); \
	u32 rhs_first = _ARMPROC.R[REG_POS(i,0)] << imm;

#define LSR_IMM \
	bool rhs_is_imm = false; \
	u32 imm = ((i>>7)&0x1F); \
	if(imm) \
	{ \
      emit_lw(psp_a0,RCPU, reg_pos_offset(0)); \
		emit_srl(psp_a0, psp_a0, imm); \
	} \
	else \
		emit_move(psp_a0, psp_zero); \
	u32 rhs_first = imm ? _ARMPROC.R[REG_POS(i,0)] >> imm : 0;

   #define ASR_IMM \
      bool rhs_is_imm = false; \
      u32 imm = ((i>>7)&0x1F); \
      emit_lw(psp_a0,RCPU, reg_pos_offset(0)); \
      if(!imm) imm = 31; \
      emit_sra(psp_a0,psp_a0,imm); \
      u32 rhs_first = (s32) _ARMPROC.R[REG_POS(i,0)] >> imm;

//rhs = psp_a0
//imm = psp_a1
//tmp = psp_at

//Hlide
#define LSX_REG(name, op)               \
   bool rhs_is_imm = false;              \
   u32 rhs = 0;                           \
   emit_lbu(psp_a1,RCPU,reg_pos_offset(8));\
   emit_sltiu(psp_at,psp_a1, 32);           \
   emit_lw(psp_a0,RCPU,reg_pos_offset(0));   \   
   op(psp_a0, psp_a0, psp_a1);                \
   emit_movz(psp_a0, psp_zero, psp_at);

//Hlide
#define ASX_REG(name, op)               \
   bool rhs_is_imm = false;              \
   u32 rhs = 0;                           \
   emit_lw(psp_a0,RCPU,reg_pos_offset(0)); \
   emit_lbu(psp_a1,RCPU,reg_pos_offset(8)); \
   emit_sltiu(psp_at,psp_a1, 32);            \   
   emit_ext(psp_a2,psp_a1, 31, 1);            \   
   emit_subu(psp_a2,psp_zero, psp_a2);         \   
   op(psp_a0, psp_a0, psp_a1);                  \
   emit_movz(psp_a0, psp_a2, psp_at);

#define LSL_REG LSX_REG(LSL_REG, emit_sllv)
#define LSR_REG LSX_REG(LSR_REG, emit_srlv)
#define ASR_REG ASX_REG(ASR_REG, emit_srav)

#define ARITHM_PSP(arg, op, symmetric) \
   arg\
   if(REG_POS(i,12) == REG_POS(i,16)){ \
      emit_lw(psp_a3, RCPU, reg_pos_offset(12)); \
		op(psp_a3, psp_a3,psp_a0); \
      emit_sw(psp_a3, RCPU, reg_pos_offset(12)); \
   }else if(symmetric && !rhs_is_imm) \
	{ \
		emit_lw(psp_a3, RCPU, reg_pos_offset(16)); \
		op(psp_a3, psp_a0,psp_a3); \
      emit_sw(psp_a3, RCPU, reg_pos_offset(12)); \
	} \
   else \
	{ \
		emit_lw(psp_a3, RCPU, reg_pos_offset(16)); \
		op(psp_a3, psp_a3,psp_a0); \
      emit_sw(psp_a3, RCPU, reg_pos_offset(12)); \
	} \
   if(REG_POS(i,12)==15) \
   { \
      emit_lw(psp_at, RCPU, reg_offset(15)); \
      emit_sw(psp_at, RCPU, NextInstr_offset); \
   } \
   return OPR_RESULT(OPR_CONTINUE, 2);

static OP_RESULT ARM_OP_AND_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_and, 1); }
static OP_RESULT ARM_OP_AND_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_and, 1); }
static OP_RESULT ARM_OP_AND_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_and, 1); }
static OP_RESULT ARM_OP_AND_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_and, 1); }
static OP_RESULT ARM_OP_AND_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_and, 1); }
static OP_RESULT ARM_OP_AND_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_and, 1); }
#define ARM_OP_AND_ROR_IMM 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, and_, 1, 0);*/ }
#define ARM_OP_AND_ROR_REG 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_REG, and_, 1, 0);*/ }
#define ARM_OP_AND_IMM_VAL 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(IMM_VAL, and_, 1, 0);*/ }

static OP_RESULT ARM_OP_EOR_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_xor, 1); }
static OP_RESULT ARM_OP_EOR_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_xor, 1); }
static OP_RESULT ARM_OP_EOR_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_xor, 1); }
static OP_RESULT ARM_OP_EOR_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_xor, 1); }
static OP_RESULT ARM_OP_EOR_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_xor, 1); }
static OP_RESULT ARM_OP_EOR_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_xor, 1); }
#define ARM_OP_EOR_ROR_IMM  0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, xor_, 1, 0);*/ }
#define ARM_OP_EOR_ROR_REG  0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_REG, xor_, 1, 0);*/ }
#define ARM_OP_EOR_IMM_VAL  0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(IMM_VAL, xor_, 1, 0);*/ }
 
static OP_RESULT ARM_OP_ORR_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_or, 1); }
static OP_RESULT ARM_OP_ORR_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_or, 1); }
static OP_RESULT ARM_OP_ORR_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_or, 1); }
static OP_RESULT ARM_OP_ORR_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_or, 1); }
static OP_RESULT ARM_OP_ORR_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_or, 1); }
static OP_RESULT ARM_OP_ORR_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_or, 1); }
#define ARM_OP_ORR_ROR_IMM 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, or_, 1, 0);*/ }
#define ARM_OP_ORR_ROR_REG 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_REG, or_, 1, 0);*/ }
#define ARM_OP_ORR_IMM_VAL 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(IMM_VAL, or_, 1, 0);*/ }
 
static OP_RESULT ARM_OP_ADD_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_addu, 1); }
static OP_RESULT ARM_OP_ADD_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_addu, 1); }
static OP_RESULT ARM_OP_ADD_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_addu, 1); }
static OP_RESULT ARM_OP_ADD_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_addu, 1); }
static OP_RESULT ARM_OP_ADD_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_addu, 1); }
static OP_RESULT ARM_OP_ADD_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_addu, 1); }
#define ARM_OP_ADD_ROR_IMM 0//(uint32_t pc, const u32 i) = 0;//{ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, add, 1, 0);*/ }
#define ARM_OP_ADD_ROR_REG 0//(uint32_t pc, const u32 i) = 0;//{ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_REG, add, 1, 0);*/ }
#define ARM_OP_ADD_IMM_VAL 0//(uint32_t pc, const u32 i) = 0;//{ return OPR_INTERPRET;/*OP_ARITHMETIC(IMM_VAL, add, 1, 0);*/ }

static OP_RESULT ARM_OP_SUB_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_subu, 0); }
static OP_RESULT ARM_OP_SUB_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_subu, 0); }
static OP_RESULT ARM_OP_SUB_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_subu, 0); }
static OP_RESULT ARM_OP_SUB_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_subu, 0); }
static OP_RESULT ARM_OP_SUB_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_subu, 0); }
static OP_RESULT ARM_OP_SUB_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_subu, 0); }
#define ARM_OP_SUB_ROR_IMM 0//(uint32_t pc, const u32 i){ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, sub, 0, 0);*/ }
#define ARM_OP_SUB_ROR_REG 0//(uint32_t pc, const u32 i){ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_REG, sub, 0, 0);*/ }
#define ARM_OP_SUB_IMM_VAL 0//(uint32_t pc, const u32 i){ return OPR_INTERPRET;/*OP_ARITHMETIC(IMM_VAL, sub, 0, 0);*/ }
 
//-----------------------------------------------------------------------------
//   MOV
//-----------------------------------------------------------------------------
#define OP_MOV(arg) \
    arg; \
	emit_sw(psp_a0, RCPU, reg_pos_offset(12)); \ 
	if(REG_POS(i,12)==15) \
	{ \
		emit_lw(psp_at, RCPU, reg_offset(15)); \
      emit_sw(psp_at, RCPU, NextInstr_offset); \
		return OPR_RESULT(OPR_CONTINUE, 1); \
	} \
    return OPR_RESULT(OPR_CONTINUE, 1);

static OP_RESULT ARM_OP_MOV_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); if (i == 0xE1A00000) { return OPR_RESULT(OPR_CONTINUE, 1); } OP_MOV(LSL_IMM); }
static OP_RESULT ARM_OP_MOV_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSL_REG; if (REG_POS(i,0) == 15) emit_addiu(psp_a0, psp_a0, 4);); }
static OP_RESULT ARM_OP_MOV_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSR_IMM); }
static OP_RESULT ARM_OP_MOV_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSR_REG; if (REG_POS(i,0) == 15) emit_addiu(psp_a0, psp_a0, 4);); }
static OP_RESULT ARM_OP_MOV_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(ASR_IMM); }
static OP_RESULT ARM_OP_MOV_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(ASR_REG); }
#define ARM_OP_MOV_ROR_IMM 0
#define ARM_OP_MOV_ROR_REG 0
#define ARM_OP_MOV_IMM_VAL 0
/*
static int OP_MOV_ROR_IMM(const u32 i) { sync_r15(i, false, 0); OP_MOV(ROR_IMM); }
static int OP_MOV_ROR_REG(const u32 i) { sync_r15(i, false, 0); OP_MOV(ROR_REG); }
static int OP_MOV_IMM_VAL(const u32 i) { sync_r15(i, false, 0); OP_MOV(IMM_VAL); }
*/


//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------
static OP_RESULT ARM_OP_MVN_LSL_IMM(uint32_t pc, const u32 i) { OP_MOV(LSL_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSL_REG(uint32_t pc, const u32 i) { OP_MOV(LSL_REG; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSR_IMM(uint32_t pc, const u32 i) { OP_MOV(LSR_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSR_REG(uint32_t pc, const u32 i) { OP_MOV(LSR_REG; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_ASR_IMM(uint32_t pc, const u32 i) { OP_MOV(ASR_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_ASR_REG(uint32_t pc, const u32 i) { OP_MOV(ASR_REG; emit_not(psp_a0, psp_a0)); }
#define ARM_OP_MVN_ROR_IMM 0
#define ARM_OP_MVN_ROR_REG 0
#define ARM_OP_MVN_IMM_VAL 0
/*
static int OP_MVN_ROR_IMM(const u32 i) { OP_MOV(ROR_IMM; emit_not(psp_a0, psp_a0)); }
static int OP_MVN_ROR_REG(const u32 i) { OP_MOV(ROR_REG; emit_not(psp_a0, psp_a0)); }
static int OP_MVN_IMM_VAL(const u32 i) { OP_MOV(IMM_VAL; rhs = ~rhs); }
*/


template <int AT16, int AT12, int AT8, int AT0, bool S, uint32_t CYC>
static OP_RESULT ARM_OP_PATCH(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
}

#define ARM_ALU_OP_DEF(T, D, N, S) \
   static const ArmOpCompiler ARM_OP_##T##_LSL_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_LSL_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_LSR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_LSR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_ASR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_ASR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_ROR_IMM = ARM_OP_PATCH<N, D, 0, 1, S, 1>; \
   static const ArmOpCompiler ARM_OP_##T##_ROR_REG = ARM_OP_PATCH<N, D, 1, 1, S, 2>; \
   static const ArmOpCompiler ARM_OP_##T##_IMM_VAL = ARM_OP_PATCH<N, D, 0, 0, S, 1>

//ARM_ALU_OP_DEF(AND  , 2, 1, false);
ARM_ALU_OP_DEF(AND_S, 2, 1, true);
//ARM_ALU_OP_DEF(EOR  , 2, 1, false);
ARM_ALU_OP_DEF(EOR_S, 2, 1, true);
//ARM_ALU_OP_DEF(SUB  , 2, 1, false);
ARM_ALU_OP_DEF(SUB_S, 2, 1, true);
ARM_ALU_OP_DEF(RSB  , 2, 1, false);
ARM_ALU_OP_DEF(RSB_S, 2, 1, true);
//ARM_ALU_OP_DEF(ADD  , 2, 1, false);
ARM_ALU_OP_DEF(ADD_S, 2, 1, true);
ARM_ALU_OP_DEF(ADC  , 2, 1, false);
ARM_ALU_OP_DEF(ADC_S, 2, 1, true);
ARM_ALU_OP_DEF(SBC  , 2, 1, false);
ARM_ALU_OP_DEF(SBC_S, 2, 1, true);
ARM_ALU_OP_DEF(RSC  , 2, 1, false);
ARM_ALU_OP_DEF(RSC_S, 2, 1, true);
ARM_ALU_OP_DEF(TST  , 0, 1, true);
ARM_ALU_OP_DEF(TEQ  , 0, 1, true);
ARM_ALU_OP_DEF(CMP  , 0, 1, true);
ARM_ALU_OP_DEF(CMN  , 0, 1, true);
//ARM_ALU_OP_DEF(ORR  , 2, 1, false);
ARM_ALU_OP_DEF(ORR_S, 2, 1, true);
//ARM_ALU_OP_DEF(MOV  , 2, 0, false);
ARM_ALU_OP_DEF(MOV_S, 2, 0, true);
ARM_ALU_OP_DEF(BIC  , 2, 1, false);
ARM_ALU_OP_DEF(BIC_S, 2, 1, true);
//ARM_ALU_OP_DEF(MVN  , 2, 0, false);
ARM_ALU_OP_DEF(MVN_S, 2, 0, true);

// HACK: multiply cycles are wrong
#define ARM_OP_MUL         0
#define ARM_OP_MUL_S       0
#define ARM_OP_MLA         0
#define ARM_OP_MLA_S       0
#define ARM_OP_UMULL       0
#define ARM_OP_UMULL_S     0
#define ARM_OP_UMLAL       0
#define ARM_OP_UMLAL_S     0
#define ARM_OP_SMULL       0
#define ARM_OP_SMULL_S     0
#define ARM_OP_SMLAL       0
#define ARM_OP_SMLAL_S     0

#define ARM_OP_SMUL_B_B    0
#define ARM_OP_SMUL_T_B    0
#define ARM_OP_SMUL_B_T    0
#define ARM_OP_SMUL_T_T    0

#define ARM_OP_SMLA_B_B    0
#define ARM_OP_SMLA_T_B    0
#define ARM_OP_SMLA_B_T    0
#define ARM_OP_SMLA_T_T    0

#define ARM_OP_SMULW_B     0
#define ARM_OP_SMULW_T     0
#define ARM_OP_SMLAW_B     0
#define ARM_OP_SMLAW_T     0

#define ARM_OP_SMLAL_B_B   0
#define ARM_OP_SMLAL_T_B   0
#define ARM_OP_SMLAL_B_T   0
#define ARM_OP_SMLAL_T_T   0

#define ARM_OP_QADD        0
#define ARM_OP_QSUB        0
#define ARM_OP_QDADD       0
#define ARM_OP_QDSUB       0

#define ARM_OP_CLZ         0

#define ARM_MEM_OP_DEF2(T, Q) \
   static const ArmOpCompiler ARM_OP_##T##_M_LSL_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_P_LSL_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_M_LSR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_P_LSR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_M_ASR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_P_ASR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_M_ROR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_P_ROR_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_M_##Q = 0; \
   static const ArmOpCompiler ARM_OP_##T##_P_##Q = 0

#define ARM_MEM_OP_DEF(T) \
   ARM_MEM_OP_DEF2(T, IMM_OFF_PREIND); \
   ARM_MEM_OP_DEF2(T, IMM_OFF); \
   ARM_MEM_OP_DEF2(T, IMM_OFF_POSTIND)

ARM_MEM_OP_DEF(STR);
ARM_MEM_OP_DEF(LDR);
ARM_MEM_OP_DEF(STRB);
ARM_MEM_OP_DEF(LDRB);

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STR(u32 adr, u32 data)
{
	WRITE32(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(2,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STRH(u32 adr, u32 data)
{
	WRITE16(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_WRITE>(2,adr);
}

template<int PROCNUM, int memtype>
static u32 FASTCALL OP_STRB(u32 adr, u32 data)
{
	WRITE8(cpu->mem_if->data, adr, data);
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_WRITE>(2,adr);
}


static OP_RESULT ARM_OP_STR(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*emit_lw(psp_a0, RCPU, reg_offset(16));
   emit_lw(psp_a1, RCPU, reg_offset(12));
   
   return OPR_RESULT(OPR_CONTINUE, 3);*/
}

#define ARM_MEM_HALF_OP_DEF2(T, P) \
   static const ArmOpCompiler ARM_OP_##T##_##P##M_REG_OFF = 0; \
   static const ArmOpCompiler ARM_OP_##T##_##P##P_REG_OFF = 0; \
   static const ArmOpCompiler ARM_OP_##T##_##P##M_IMM_OFF = 0; \
   static const ArmOpCompiler ARM_OP_##T##_##P##P_IMM_OFF = 0

#define ARM_MEM_HALF_OP_DEF(T) \
   ARM_MEM_HALF_OP_DEF2(T, POS_INDE_); \
   ARM_MEM_HALF_OP_DEF2(T, ); \
   ARM_MEM_HALF_OP_DEF2(T, PRE_INDE_)

ARM_MEM_HALF_OP_DEF(STRH);
ARM_MEM_HALF_OP_DEF(LDRH);
ARM_MEM_HALF_OP_DEF(STRSB);
ARM_MEM_HALF_OP_DEF(LDRSB);
ARM_MEM_HALF_OP_DEF(STRSH);
ARM_MEM_HALF_OP_DEF(LDRSH);

//
#define SIGNEXTEND_24(i) (((s32)i<<8)>>8)
static OP_RESULT ARM_OP_B_BL(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*
   const AG_COND cond = (AG_COND)bit(opcode, 28, 4);
   const bool has_link = bit(opcode, 24);

   const bool unconditional = (cond == AL || cond == EGG);
   int32_t regs[1] = { (has_link || cond == EGG) ? 14 : -1 };
   regman->get(1, regs);

   uint32_t dest = (pc + 8 + (SIGNEXTEND_24(bit(opcode, 0, 24)) << 2));

   if (!unconditional)
   {
      block->load_constant(0, pc + 4);

      block->b("run", cond);
      block->b("skip");
      block->set_label("run");
   }
   
   if (cond == EGG)
   {
      change_mode(true);

      if (has_link)
      {
         dest += 2;
      }
   }

   if (has_link || cond == EGG)
   {
      block->load_constant(regs[0], pc + 4);
      regman->mark_dirty(regs[0]);
   }

   block->load_constant(0, dest);

   if (!unconditional)
   {
      block->set_label("skip");
      block->resolve_label("run");
      block->resolve_label("skip");
   }

   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));


   // TODO: Timing
   return OPR_RESULT(OPR_BRANCHED, 3);*/
}

#define ARM_OP_B  ARM_OP_B_BL
#define ARM_OP_BL ARM_OP_B_BL

////

#define ARM_OP_LDRD_STRD_POST_INDEX 0
#define ARM_OP_LDRD_STRD_OFFSET_PRE_INDEX 0
#define ARM_OP_MRS_CPSR 0
#define ARM_OP_SWP 0
#define ARM_OP_MSR_CPSR 0
#define ARM_OP_BX 0
#define ARM_OP_BLX_REG 0
#define ARM_OP_BKPT 0
#define ARM_OP_MRS_SPSR 0
#define ARM_OP_SWPB 0
#define ARM_OP_MSR_SPSR 0
#define ARM_OP_STREX 0
#define ARM_OP_LDREX 0
#define ARM_OP_MSR_CPSR_IMM_VAL 0
#define ARM_OP_MSR_SPSR_IMM_VAL 0
#define ARM_OP_STMDA 0
#define ARM_OP_LDMDA 0
#define ARM_OP_STMDA_W 0
#define ARM_OP_LDMDA_W 0
#define ARM_OP_STMDA2 0
#define ARM_OP_LDMDA2 0
#define ARM_OP_STMDA2_W 0
#define ARM_OP_LDMDA2_W 0
#define ARM_OP_STMIA 0
#define ARM_OP_LDMIA 0
#define ARM_OP_STMIA_W 0
#define ARM_OP_LDMIA_W 0
#define ARM_OP_STMIA2 0
#define ARM_OP_LDMIA2 0
#define ARM_OP_STMIA2_W 0
#define ARM_OP_LDMIA2_W 0
#define ARM_OP_STMDB 0
#define ARM_OP_LDMDB 0
#define ARM_OP_STMDB_W 0
#define ARM_OP_LDMDB_W 0
#define ARM_OP_STMDB2 0
#define ARM_OP_LDMDB2 0
#define ARM_OP_STMDB2_W 0
#define ARM_OP_LDMDB2_W 0
#define ARM_OP_STMIB 0
#define ARM_OP_LDMIB 0
#define ARM_OP_STMIB_W 0
#define ARM_OP_LDMIB_W 0
#define ARM_OP_STMIB2 0
#define ARM_OP_LDMIB2 0
#define ARM_OP_STMIB2_W 0
#define ARM_OP_LDMIB2_W 0
#define ARM_OP_STC_OPTION 0
#define ARM_OP_LDC_OPTION 0
#define ARM_OP_STC_M_POSTIND 0
#define ARM_OP_LDC_M_POSTIND 0
#define ARM_OP_STC_P_POSTIND 0
#define ARM_OP_LDC_P_POSTIND 0
#define ARM_OP_STC_M_IMM_OFF 0
#define ARM_OP_LDC_M_IMM_OFF 0
#define ARM_OP_STC_M_PREIND 0
#define ARM_OP_LDC_M_PREIND 0
#define ARM_OP_STC_P_IMM_OFF 0
#define ARM_OP_LDC_P_IMM_OFF 0
#define ARM_OP_STC_P_PREIND 0
#define ARM_OP_LDC_P_PREIND 0
#define ARM_OP_CDP 0
#define ARM_OP_MCR 0
#define ARM_OP_MRC 0
#define ARM_OP_SWI 0
#define ARM_OP_UND 0
static const ArmOpCompiler arm_instruction_compilers[4096] = {
#define TABDECL(x) ARM_##x
#include "instruction_tabdef.inc"
#undef TABDECL
};

////////
// THUMB
////////
static OP_RESULT THUMB_OP_SHIFT(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const uint32_t imm = bit(opcode, 6, 5);
   const AG_ALU_SHIFT op = (AG_ALU_SHIFT)bit(opcode, 11, 2);

   int32_t regs[2] = { rd | 0x10, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   block->movs(nrd, alu2::reg_shift_imm(nrs, op, imm));
   mark_status_dirty();

   regman->mark_dirty(nrd);

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_ADDSUB_REGIMM(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const AG_ALU_OP op = bit(opcode, 9) ? SUBS : ADDS;
   const bool arg_type = bit(opcode, 10);
   const uint32_t arg = bit(opcode, 6, 3);

   int32_t regs[3] = { rd | 0x10, rs, (!arg_type) ? arg : -1 };
   regman->get(3, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   if (arg_type) // Immediate
   {
      block->alu_op(op, nrd, nrs, alu2::imm(arg));
      mark_status_dirty();
   }
   else
   {
      block->alu_op(op, nrd, nrs, alu2::reg(regs[2]));
      mark_status_dirty();
   }

   regman->mark_dirty(nrd);

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_MCAS_IMM8(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const reg_t rd = bit(opcode, 8, 3);
   const uint32_t op = bit(opcode, 11, 2);
   const uint32_t imm = bit(opcode, 0, 8);

   int32_t regs[1] = { rd };
   regman->get(1, regs);
   const reg_t nrd = regs[0];
   
   switch (op)
   {
      case 0: block->alu_op(MOVS, nrd, nrd, alu2::imm(imm)); break;
      case 1: block->alu_op(CMP , nrd, nrd, alu2::imm(imm)); break;
      case 2: block->alu_op(ADDS, nrd, nrd, alu2::imm(imm)); break;
      case 3: block->alu_op(SUBS, nrd, nrd, alu2::imm(imm)); break;
   }

   mark_status_dirty();

   if (op != 1) // Don't keep the result of a CMP instruction
   {
      regman->mark_dirty(nrd);
   }

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_ALU(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rs = bit(opcode, 3, 3);
   const uint32_t op = bit(opcode, 6, 4);
   bool need_writeback = false;

   if (op == 13) // TODO: The MULS is interpreted for now
   {
      return OPR_INTERPRET;
   }

   int32_t regs[2] = { rd, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   switch (op)
   {
      case  0: block->ands(nrd, alu2::reg(nrs)); break;
      case  1: block->eors(nrd, alu2::reg(nrs)); break;
      case  5: block->adcs(nrd, alu2::reg(nrs)); break;
      case  6: block->sbcs(nrd, alu2::reg(nrs)); break;
      case  8: block->tst (nrd, alu2::reg(nrs)); break;
      case 10: block->cmp (nrd, alu2::reg(nrs)); break;
      case 11: block->cmn (nrd, alu2::reg(nrs)); break;
      case 12: block->orrs(nrd, alu2::reg(nrs)); break;
      case 14: block->bics(nrd, alu2::reg(nrs)); break;
      case 15: block->mvns(nrd, alu2::reg(nrs)); break;

      case  2: block->movs(nrd, alu2::reg_shift_reg(nrd, LSL, nrs)); break;
      case  3: block->movs(nrd, alu2::reg_shift_reg(nrd, LSR, nrs)); break;
      case  4: block->movs(nrd, alu2::reg_shift_reg(nrd, ASR, nrs)); break;
      case  7: block->movs(nrd, alu2::reg_shift_reg(nrd, arm_gen::ROR, nrs)); break;

      case  9: block->rsbs(nrd, nrs, alu2::imm(0)); break;
   }

   mark_status_dirty();

   static const bool op_wb[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1 };
   if (op_wb[op])
   {
      regman->mark_dirty(nrd);
   }

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_SPE(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t rd = bit(opcode, 0, 3) + (bit(opcode, 7) ? 8 : 0);
   const uint32_t rs = bit(opcode, 3, 4);
   const uint32_t op = bit(opcode, 8, 2);

   if (rd == 0xF || rs == 0xF)
   {
      return OPR_INTERPRET;
   }

   int32_t regs[2] = { rd, rs };
   regman->get(2, regs);

   const reg_t nrd = regs[0];
   const reg_t nrs = regs[1];

   switch (op)
   {
      case 0: block->add(nrd, alu2::reg(nrs)); break;
      case 1: block->cmp(nrd, alu2::reg(nrs)); break;
      case 2: block->mov(nrd, alu2::reg(nrs)); break;
   }

   if (op != 1)
   {
      regman->mark_dirty(nrd);
   }
   else
   {
      mark_status_dirty();
   }

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_MEMORY_DELEGATE(uint32_t pc, uint32_t opcode, bool LOAD, uint32_t SIZE, uint32_t EXTEND, bool REG_OFFSET)
{
   return OPR_INTERPRET;
   /*const uint32_t rd = bit(opcode, 0, 3);
   const uint32_t rb = bit(opcode, 3, 3);
   const uint32_t ro = bit(opcode, 6, 3);
   const uint32_t off = bit(opcode, 6, 5);

   int32_t regs[3] = { rd | (LOAD ? 0x10 : 0), rb, REG_OFFSET ? ro : -1};
   regman->get(3, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];

   // Calc EA

   if (REG_OFFSET)
   {
      const reg_t offset = regs[2];
      block->mov(0, alu2::reg(base));
      block->add(0, alu2::reg(offset));
   }
   else
   {
      block->add(0, base, alu2::imm(off << SIZE));
   }

   // Load access function
   block->load_constant(2, mem_funcs[(SIZE << 2) + (LOAD ? 0 : 2) + block_procnum]);

   if (!LOAD)
   {
      block->mov(1, alu2::reg(dest));
   }

   call(2);

   if (LOAD)
   {
      if (EXTEND)
      {
         if (SIZE == 0)
         {
            block->sxtb(dest, 0);
         }
         else
         {
            block->sxth(dest, 0);
         }
      }
      else
      {
         block->mov(dest, alu2::reg(0));
      }

      regman->mark_dirty(dest);
   }

   // TODO
   return OPR_RESULT(OPR_CONTINUE, 3);*/
}

// SIZE: 0=8, 1=16, 2=32
template <bool LOAD, uint32_t SIZE, uint32_t EXTEND, bool REG_OFFSET>
static OP_RESULT THUMB_OP_MEMORY(uint32_t pc, uint32_t opcode)
{
   return THUMB_OP_MEMORY_DELEGATE(pc, opcode, LOAD, SIZE, EXTEND, REG_OFFSET);
}

static OP_RESULT THUMB_OP_LDR_PCREL(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[1] = { rd | 0x10 };
   regman->get(1, regs);

   const reg_t dest = regs[0];

   block->load_constant(0, ((pc + 4) & ~2) + (offset << 2));
   block->load_constant(2, mem_funcs[8 + block_procnum]);
   call(2);
   block->mov(dest, alu2::reg(0));

   regman->mark_dirty(dest);
   return OPR_RESULT(OPR_CONTINUE, 3);*/
}

static OP_RESULT THUMB_OP_STR_SPREL(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { rd, 13 };
   regman->get(2, regs);

   const reg_t src = regs[0];
   const reg_t base = regs[1];

   block->add(0, base, alu2::imm_rol(offset, 2));
   block->mov(1, alu2::reg(src));
   block->load_constant(2, mem_funcs[10 + block_procnum]);
   call(2);

   return OPR_RESULT(OPR_CONTINUE, 3);*/
}

static OP_RESULT THUMB_OP_LDR_SPREL(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { rd | 0x10, 13 };
   regman->get(2, regs);

   const reg_t dest = regs[0];
   const reg_t base = regs[1];

   block->add(0, base, alu2::imm_rol(offset, 2));
   block->load_constant(2, mem_funcs[8 + block_procnum]);
   call(2);
   block->mov(dest, alu2::reg(0));

   regman->mark_dirty(dest);
   return OPR_RESULT(OPR_CONTINUE, 3);*/
}

static OP_RESULT THUMB_OP_B_COND(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const AG_COND cond = (AG_COND)bit(opcode, 8, 4);

   block->load_constant(0, pc + 2);
   block->load_constant(0, (pc + 4) + ((u32)((s8)(opcode&0xFF))<<1), cond);
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   block->add(RCYC, alu2::imm(2), cond);

   return OPR_RESULT(OPR_BRANCHED, 1);*/
}

static OP_RESULT THUMB_OP_B_UNCOND(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   int32_t offs = (opcode & 0x7FF) | (bit(opcode, 10) ? 0xFFFFF800 : 0);
   int32_t val =  pc + 4 + (offs << 1);

   printf("UN\n");
   emit_lui(psp_a0,val>>16);
   emit_ori(psp_a0,psp_a0,val&0xffff);

   emit_sw(psp_a0, RCPU, instrAdr_offset);

   return OPR_RESULT(OPR_BRANCHED, 3);
}

static OP_RESULT THUMB_OP_ADJUST_SP(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offs = bit(opcode, 0, 7);

   int32_t regs[1] = { 13 };
   regman->get(1, regs);

   const reg_t sp = regs[0];

   if (bit(opcode, 7)) block->sub(sp, alu2::imm_rol(offs, 2));
   else                block->add(sp, alu2::imm_rol(offs, 2));

   regman->mark_dirty(sp);

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_ADD_2PC(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[1] = { rd | 0x10 };
   regman->get(1, regs);

   const reg_t dest = regs[0];

   block->load_constant(dest, ((pc + 4) & 0xFFFFFFFC) + (offset << 2));
   regman->mark_dirty(dest);

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_ADD_2SP(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const uint32_t offset = bit(opcode, 0, 8);
   const reg_t rd = bit(opcode, 8, 3);

   int32_t regs[2] = { 13, rd | 0x10 };
   regman->get(2, regs);

   const reg_t sp = regs[0];
   const reg_t dest = regs[1];

   block->add(dest, sp, alu2::imm_rol(offset, 2));
   regman->mark_dirty(dest);

   return OPR_RESULT(OPR_CONTINUE, 1);*/
}

static OP_RESULT THUMB_OP_BX_BLX_THUMB(uint32_t pc, uint32_t opcode)
{
   return OPR_INTERPRET;
   /*const reg_t rm = bit(opcode, 3, 4);
   const bool link = bit(opcode, 7);

   if (rm == 15)
      return OPR_INTERPRET;

   block->load_constant(0, pc + 4);

   int32_t regs[2] = { link ? 14 : -1, (rm != 15) ? (int32_t)rm : -1 };
   regman->get(2, regs);

   if (link)
   {
      const reg_t lr = regs[0];
      block->sub(lr, 0, alu2::imm(1));
      regman->mark_dirty(lr);
   }

   reg_t target = regs[1];

   change_mode_reg(target, 2, 3);
   block->bic(0, target, alu2::imm(1));
   block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));

   return OPR_RESULT(OPR_BRANCHED, 3);*/
}

#if 1
#define THUMB_OP_BL_LONG 0
#else
static OP_RESULT THUMB_OP_BL_LONG(uint32_t pc, uint32_t opcode)
{
   static const uint32_t op = bit(opcode, 11, 5);
   int32_t offset = bit(opcode, 0, 11);
   reg_t lr = regman->get(14, op == 0x1E);
   if (op == 0x1E)
   {
      offset |= (offset & 0x400) ? 0xFFFFF800 : 0;
      block->load_constant(lr, (pc + 4) + (offset << 12));
   }
   else
   {
      block->load_constant(0, offset << 1);
      block->add(0, lr, alu2::reg(0));
      block->str(0, RCPU, mem2::imm(offsetof(armcpu_t, instruct_adr)));
      block->load_constant(lr, pc + 3);
      if (op != 0x1F)
      {
         change_mode(false);
      }
   }
   regman->mark_dirty(lr);
   if (op == 0x1E)
   {
      return OPR_RESULT(OPR_CONTINUE, 1);
   }
   else
   {
      return OPR_RESULT(OPR_BRANCHED, (op == 0x1F) ? 3 : 4);
   }
}
#endif

#define THUMB_OP_INTERPRET       0
#define THUMB_OP_UND_THUMB       THUMB_OP_INTERPRET

#define THUMB_OP_LSL             THUMB_OP_SHIFT
#define THUMB_OP_LSL_0           THUMB_OP_SHIFT
#define THUMB_OP_LSR             THUMB_OP_SHIFT
#define THUMB_OP_LSR_0           THUMB_OP_SHIFT
#define THUMB_OP_ASR             THUMB_OP_SHIFT
#define THUMB_OP_ASR_0           THUMB_OP_SHIFT

#define THUMB_OP_ADD_REG         THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_SUB_REG         THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_ADD_IMM3        THUMB_OP_ADDSUB_REGIMM
#define THUMB_OP_SUB_IMM3        THUMB_OP_ADDSUB_REGIMM

#define THUMB_OP_MOV_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_CMP_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_ADD_IMM8        THUMB_OP_MCAS_IMM8
#define THUMB_OP_SUB_IMM8        THUMB_OP_MCAS_IMM8

#define THUMB_OP_AND             THUMB_OP_ALU
#define THUMB_OP_EOR             THUMB_OP_ALU
#define THUMB_OP_LSL_REG         THUMB_OP_ALU
#define THUMB_OP_LSR_REG         THUMB_OP_ALU
#define THUMB_OP_ASR_REG         THUMB_OP_ALU
#define THUMB_OP_ADC_REG         THUMB_OP_ALU
#define THUMB_OP_SBC_REG         THUMB_OP_ALU
#define THUMB_OP_ROR_REG         THUMB_OP_ALU
#define THUMB_OP_TST             THUMB_OP_ALU
#define THUMB_OP_NEG             THUMB_OP_ALU
#define THUMB_OP_CMP             THUMB_OP_ALU
#define THUMB_OP_CMN             THUMB_OP_ALU
#define THUMB_OP_ORR             THUMB_OP_ALU
#define THUMB_OP_MUL_REG         THUMB_OP_INTERPRET
#define THUMB_OP_BIC             THUMB_OP_ALU
#define THUMB_OP_MVN             THUMB_OP_ALU

#define THUMB_OP_ADD_SPE         THUMB_OP_SPE
#define THUMB_OP_CMP_SPE         THUMB_OP_SPE
#define THUMB_OP_MOV_SPE         THUMB_OP_SPE

#define THUMB_OP_ADJUST_P_SP     THUMB_OP_ADJUST_SP
#define THUMB_OP_ADJUST_M_SP     THUMB_OP_ADJUST_SP

#define THUMB_OP_LDRB_REG_OFF    THUMB_OP_MEMORY<true , 0, 0, true>
#define THUMB_OP_LDRH_REG_OFF    THUMB_OP_MEMORY<true , 1, 0, true>
#define THUMB_OP_LDR_REG_OFF     THUMB_OP_MEMORY<true , 2, 0, true>

#define THUMB_OP_STRB_REG_OFF    THUMB_OP_MEMORY<false, 0, 0, true>
#define THUMB_OP_STRH_REG_OFF    THUMB_OP_MEMORY<false, 1, 0, true>
#define THUMB_OP_STR_REG_OFF     THUMB_OP_MEMORY<false, 2, 0, true>

#define THUMB_OP_LDRB_IMM_OFF    THUMB_OP_MEMORY<true , 0, 0, false>
#define THUMB_OP_LDRH_IMM_OFF    THUMB_OP_MEMORY<true , 1, 0, false>
#define THUMB_OP_LDR_IMM_OFF     THUMB_OP_MEMORY<true , 2, 0, false>

#define THUMB_OP_STRB_IMM_OFF    THUMB_OP_MEMORY<false, 0, 0, false>
#define THUMB_OP_STRH_IMM_OFF    THUMB_OP_MEMORY<false, 1, 0, false>
#define THUMB_OP_STR_IMM_OFF     THUMB_OP_MEMORY<false, 2, 0, false>

#define THUMB_OP_LDRSB_REG_OFF   THUMB_OP_MEMORY<true , 0, 1, true>
#define THUMB_OP_LDRSH_REG_OFF   THUMB_OP_MEMORY<true , 1, 1, true>

#define THUMB_OP_BX_THUMB        THUMB_OP_BX_BLX_THUMB
#define THUMB_OP_BLX_THUMB       THUMB_OP_BX_BLX_THUMB
#define THUMB_OP_BL_10           THUMB_OP_BL_LONG
#define THUMB_OP_BL_11           THUMB_OP_BL_LONG
#define THUMB_OP_BLX             THUMB_OP_BL_LONG


// UNDEFINED OPS
#define THUMB_OP_PUSH            THUMB_OP_INTERPRET
#define THUMB_OP_PUSH_LR         THUMB_OP_INTERPRET
#define THUMB_OP_POP             THUMB_OP_INTERPRET
#define THUMB_OP_POP_PC          THUMB_OP_INTERPRET
#define THUMB_OP_BKPT_THUMB      THUMB_OP_INTERPRET
#define THUMB_OP_STMIA_THUMB     THUMB_OP_INTERPRET
#define THUMB_OP_LDMIA_THUMB     THUMB_OP_INTERPRET
#define THUMB_OP_SWI_THUMB       THUMB_OP_INTERPRET

static const ArmOpCompiler thumb_instruction_compilers[1024] = {
#define TABDECL(x) THUMB_##x
#include "thumb_tabdef.inc"
#undef TABDECL
};

// ============================================================================================= IMM

//-----------------------------------------------------------------------------
//   Generic instruction wrapper
//-----------------------------------------------------------------------------

template<int PROCNUM, int thumb>
static u32 FASTCALL OP_DECODE()
{
	u32 cycles;
	u32 adr = ARMPROC.instruct_adr;
	if(thumb)
	{
		ARMPROC.next_instruction = adr + 2;
		ARMPROC.R[15] = adr + 4;
		u32 opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(adr);
		cycles = thumb_instructions_set[PROCNUM][opcode>>6](opcode);
	}
	else
	{
		ARMPROC.next_instruction = adr + 4;
		ARMPROC.R[15] = adr + 8;
		u32 opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(adr);
		if(CONDITION(opcode) == 0xE || TEST_COND(CONDITION(opcode), CODE(opcode), ARMPROC.CPSR))
			cycles = arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)](opcode);
		else
			cycles = 1;
	}
	ARMPROC.instruct_adr = ARMPROC.next_instruction;
	return cycles;
}
template<int PROCNUM>
static u32 FASTCALL DYNAREC_EXEC(u32 pc)
{
   ARMPROC.next_instruction = pc + 4;
   ARMPROC.R[15] = pc + 8;
   u32 opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(pc);

   if(TEST_COND(CONDITION(opcode), CODE(opcode), ARMPROC.CPSR))
      return arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)](opcode);

	return 1;
}

static const ArmOpCompiled op_decode[2][2] = { OP_DECODE<0,0>, OP_DECODE<0,1>, OP_DECODE<1,0>, OP_DECODE<1,1> };



//-----------------------------------------------------------------------------
//   Compiler
//-----------------------------------------------------------------------------

void __debugbreak() { fflush(stdout); *(int*)0=1;}
#define dbgbreak {__debugbreak(); for(;;);}
#define _T(x) x
#define die(reason) { printf("Fatal error : %d\n in %s -> %s : %d \n",_T(reason),_T(__FUNCTION__),_T(__FILE__),__LINE__); dbgbreak;}


static bool instr_is_conditional(u32 opcode)
{
	if(thumb) return false;
	
	return !(CONDITION(opcode) == 0xE
	         || (CONDITION(opcode) == 0xF && CODE(opcode) == 5));
}

static bool instr_uses_r15(u32 opcode, const bool thumb)
{
	u32 x = instr_attributes(opcode);
	if(thumb)
		return ((x & SRCREG_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15)
			|| ((x & SRCREG_POS3) && REG_POS(opcode,3) == 15)
			|| (x & JIT_BYPASS);
	else
		return ((x & SRCREG_POS0) && REG_POS(opcode,0) == 15)
		    || ((x & SRCREG_POS8) && REG_POS(opcode,8) == 15)
		    || ((x & SRCREG_POS12) && REG_POS(opcode,12) == 15)
		    || ((x & SRCREG_POS16) && REG_POS(opcode,16) == 15)
		    || ((x & SRCREG_STM) && BIT15(opcode))
		    || (x & JIT_BYPASS);
}

static void emit_prefetch(const u32 pc){
   const u8 isize = thumb ? 2 : 4;

   emit_addiu(psp_at, psp_gp, isize);
   emit_sw(psp_at, psp_k0, NextInstr_offset);

   emit_addiu(psp_at, psp_at, isize);
   emit_sw(psp_at, psp_k0, reg_offset(15));
}

static bool instr_does_prefetch(u32 opcode)
{
	u32 x = instr_attributes(opcode);
	if(thumb)
		return thumb_instruction_compilers[opcode>>6]
			   && (x & BRANCH_ALWAYS);
	else
		return instr_is_branch(opcode) && arm_instruction_compilers[INSTRUCTION_INDEX(opcode)]
			   && ((x & BRANCH_ALWAYS) || (x & BRANCH_LDM));
}

static void sync_r15(u32 opcode, bool is_last, bool force)
{
   if(instr_does_prefetch(opcode))
	{
		if(force)
		{
         emit_lw(psp_at, RCPU, NextInstr_offset);  //pc + isize, psp_k0, nextinstr offset
         emit_sw(psp_at, RCPU, instrAdr_offset);  //pc + isize, psp_k0, nextinstr offset
			///c.mov(cpu_ptr(instruct_adr), bb_next_instruction);
		}
	}
	else
	{

      if(instr_attributes(opcode) & JIT_BYPASS)
      {
         emit_sw(psp_gp, RCPU, instrAdr_offset);   //pc, psp_k0, nextinstr offset
         //c.mov(cpu_ptr(instruct_adr), bb_adr);
      }
      if(instr_uses_r15(opcode, thumb))
      {
         const u8 isize = thumb ? 4 : 8;
         emit_addiu(psp_at, psp_gp,isize);
         emit_sw(psp_at, RCPU, reg_offset(15));
         //c.mov(reg_ptr(15), bb_r15);
      }
		if(force || (instr_attributes(opcode) & JIT_BYPASS) || (instr_attributes(opcode) & BRANCH_SWI) || (is_last && !instr_is_branch(opcode)))
		{
         const u8 isize = thumb ? 2 : 4;
         emit_addiu(psp_at, psp_gp,isize);
         emit_sw(psp_at, RCPU, NextInstr_offset);
			//c.mov(cpu_ptr(next_instruction), bb_next_instruction);
		}

	}
}


template<int PROCNUM>
static u32 compile_basicblock()
{
   block_procnum = PROCNUM;

   void* code_ptr = emit_GetCCPtr();

   thumb = ARMPROC.CPSR.bits.T == 1;
   const u32 base = ARMPROC.instruct_adr;
   const u32 isize = thumb ? 2 : 4;

   const uint32_t imask = thumb ? 0xFFFFFFFE : 0xFFFFFFFC;

   uint32_t pc = base;

   bool first_op = true;

   uint32_t opcode = 0;

   uint32_t n_op = 0;
   uint32_t interpreted_cycles = 0;

   bool _includeNop = false;
   bool interpreted = false;

   //printf("%x THUMB: %d  %x\n",(u32)code_ptr, thumb, base);

   if (thumb){
      opcode = _MMU_read16<PROCNUM, MMU_AT_CODE>(pc&imask);

      if (instr_is_branch(opcode)){
         JIT_COMPILED_FUNC(base, PROCNUM) = (uintptr_t)op_decode[PROCNUM][true];
         return op_decode[PROCNUM][true]();
      }
   }else{
      opcode = _MMU_read32<PROCNUM, MMU_AT_CODE>(pc&imask);

      if (instr_is_branch(opcode)){
            JIT_COMPILED_FUNC(base, PROCNUM) = (uintptr_t)op_decode[PROCNUM][false];
            return op_decode[PROCNUM][false]();
      }
   }

   emit_mpush(3,
				reg_gpr+psp_gp,
				reg_gpr+psp_k0,
				reg_gpr+psp_ra);

   emit_lui(psp_k0,((u32)&ARMPROC)>>16);
	emit_ori(psp_k0,psp_k0,((u32)&ARMPROC)&0xFFFF);

   for (uint32_t i = 0, has_ended = 0; has_ended == 0; i ++, pc += isize, n_op++){

      opcode = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc&imask) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc&imask);
      has_ended = instr_is_branch(opcode) || (i >= (CommonSettings.jit_max_block_size - 1));

      interpreted = true;
      
      emit_lui(psp_gp, pc >> 16);
      emit_ori(psp_gp, psp_gp, pc &0xffff);

      if (thumb){
         //sync_r15(pc, opcode, has_ended, 1);    
         
         _includeNop = false;

         emit_prefetch(pc);  

         emit_jal(thumb_instructions_set[PROCNUM][opcode>>6]);

         if (opcode == 0 && i == 0) emit_move(psp_a0,psp_zero); 
         else if (opcode != 0) emit_la(psp_a0,opcode&0xFFFF); 
         else _includeNop = true;
      }
      else 
      if (!instr_is_conditional(opcode)){

         if (my_config.FULLDynarec){
            ArmOpCompiler fc = arm_instruction_compilers[INSTRUCTION_INDEX(opcode)];

            if (fc){ 
               int result = fc(pc,opcode);

               if (result != OPR_INTERPRET){
                  interpreted = false;
                  interpreted_cycles += op_decode[PROCNUM][thumb]() + OPR_RESULT_CYCLES(result);
                  continue;
               }
            }
         }

         _includeNop = true;

         if (opcode == 0 && i == 0) emit_move(psp_a0,psp_zero);
         else{
            emit_lui(psp_at,opcode>>16);
            emit_ori(psp_a0,psp_at,opcode&0xFFFF);
         }

         emit_prefetch(pc);

         emit_jal(arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)]);
      }
      else{
         _includeNop = false;
         emit_jal(DYNAREC_EXEC<PROCNUM>);
         emit_move(psp_a0,psp_gp);
      }

      interpreted_cycles += op_decode[PROCNUM][thumb]();
   }

   if (_includeNop) emit_nop();

   if(interpreted || !instr_does_prefetch(opcode))
	{
      emit_lw(psp_at, RCPU, NextInstr_offset);
      emit_sw(psp_at, RCPU, instrAdr_offset);
   }

   emit_mpop(3,
				reg_gpr+psp_gp,
				reg_gpr+psp_k0,
				reg_gpr+psp_ra);
   
   emit_jra();
   emit_la(psp_v0,interpreted_cycles);
   
   make_address_range_executable((u32)code_ptr, (u32)emit_GetCCPtr());
   JIT_COMPILED_FUNC(base, PROCNUM) = (uintptr_t)code_ptr;

   return interpreted_cycles;
}


template<int PROCNUM> u32 arm_jit_compile()
{
   u32 adr = ARMPROC.instruct_adr;
   u32 mask_adr = (adr & 0x07FFFFFE) >> 4;

   if(((recompile_counts[mask_adr >> 1] >> 4*(mask_adr & 1)) & 0xF) > 8)
   {
      ArmOpCompiled f = op_decode[PROCNUM][ARMPROC.CPSR.bits.T];
		JIT_COMPILED_FUNC(adr, PROCNUM) = (uintptr_t)f;
		return f();
   }
   
   recompile_counts[mask_adr >> 1] += 1 << 4*(mask_adr & 1);

   if ((CODE_SIZE - LastAddr) < 16 * 1024){
      printf("Dynarec code reset\n");
      arm_jit_reset(true,false);
   }

   return compile_basicblock<PROCNUM>();
}

template u32 arm_jit_compile<0>();
template u32 arm_jit_compile<1>();

void arm_jit_reset(bool enable, bool suppress_msg)
{
   if (!suppress_msg)
	   printf("CPU mode: %s\n", enable?"JIT":"Interpreter");

   saveBlockSizeJIT = CommonSettings.jit_max_block_size;

   if (enable)
   {
      printf("JIT: max block size %d instruction(s)\n", CommonSettings.jit_max_block_size);

      #define JITFREE(x) memset(x,0,sizeof(x));
         JITFREE(JIT.MAIN_MEM);
         JITFREE(JIT.SWIRAM);
         JITFREE(JIT.ARM9_ITCM);
         JITFREE(JIT.ARM9_LCDC);
         JITFREE(JIT.ARM9_BIOS);
         JITFREE(JIT.ARM7_BIOS);
         JITFREE(JIT.ARM7_ERAM);
         JITFREE(JIT.ARM7_WIRAM);
         JITFREE(JIT.ARM7_WRAM);
      #undef JITFREE

      memset(recompile_counts, 0, sizeof(recompile_counts));
      init_jit_mem();

     memset(CodeCache, 0, CODE_SIZE);
     LastAddr = 0;
   }
}

void arm_jit_close()
{
   memset(CodeCache, 0, CODE_SIZE);
   LastAddr = 0;
}
