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
#include <vector>

#include "instructions.h"
#include "instruction_attributes.h"
#include "MMU.h"
#include "MMU_timing.h"
#include "arm_jit.h"
#include "bios.h"
#include "armcpu.h"
#include "PSP/emit/psp_emit.h"
#include "PSP/FrontEnd.h"
#include "mips_code_emiter.h"

#include "Disassembler.h"

#include <pspsuspend.h>

u32 saveBlockSizeJIT = 0;
uint32_t interpreted_cycles = 0;

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

std::vector<optimiz> bblock_optmizer;

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
static bool skip_prefeth = false;
static uint32_t pc = 0;
static uint32_t n_ops = 0;

static bool skip_load = false;
static bool skip_save = false;


static void emit_prefetch();
static void sync_r15(u32 opcode, bool is_last, bool force);
static void emit_branch(int cond,u32 opcode, u32 sz,bool EndBlock);

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

//static u8 recompile_counts[(1<<26)/16];

#define _REG_NUM(i, n)		((i>>(n))&0x7)

#define RCPU   psp_k0
#define RCYC   psp_s0

static uint32_t block_procnum;

#define _ARMPROC (block_procnum ? NDS_ARM7 : NDS_ARM9)

#define _cond_table(x) arm_cond_table[x]

#define _cp15(x) ((u32)(((u8*)&cp15.x) - ((u8*)&_ARMPROC)))
#define _MMU(x) ((u32)(((u8*)&MMU.x) - ((u8*)&_ARMPROC)))
#define _NDS_ARM9(x) ((u32)(((u8*)&NDS_ARM9.x) - ((u8*)&_ARMPROC)))
#define _NDS_ARM7(x) ((u32)(((u8*)&NDS_ARM7.x) - ((u8*)&_ARMPROC)))

#define _reg(x) ((u32)(((u8*)&_ARMPROC.R[x]) - ((u8*)&_ARMPROC)))
#define _reg_pos(x) ((u32)(((u8*)&_ARMPROC.R[REG_POS(i,x)]) - ((u8*)&_ARMPROC)))
#define _thumb_reg_pos(x) ((u32)(((u8*)&_ARMPROC.R[_REG_NUM(i,x)]) - ((u8*)&_ARMPROC)))

#define _R15 _reg(15)

#define _instr_adr ((u32)(((u8*)&_ARMPROC.instruct_adr) - ((u8*)&_ARMPROC)))
#define _next_instr ((u32)(((u8*)&_ARMPROC.next_instruction) - ((u8*)&_ARMPROC)))
#define _instr ((u32)(((u8*)&_ARMPROC.instruction) - ((u8*)&_ARMPROC)))

#define mem_if_data (((u8*)&_ARMPROC.mem_if->data) - ((u8*)&_ARMPROC))

#define _flags ((u32)(((u8*)&_ARMPROC.CPSR.val) - ((u8*)&_ARMPROC)))
#define _flag_N 31
#define _flag_Z 30
#define _flag_C 29
#define _flag_V 28
#define _flag_T  5

//LBU 
#define _flag_N8 7
#define _flag_Z8 6
#define _flag_C8 5
#define _flag_V8 4

enum {
	MEMTYPE_GENERIC = 0, // no assumptions
	MEMTYPE_MAIN = 1,
	MEMTYPE_DTCM = 2,
	MEMTYPE_ERAM = 3,
	MEMTYPE_SWIRAM = 4,
	MEMTYPE_OTHER = 5, // memory that is known to not be MAIN, DTCM, ERAM, or SWIRAM
};



static void emit_Variableprefetch(const u32 pc,const u8 t){
   const u8 isize = (thumb ? 2 : 4) * t;

   emit_addiu(psp_at, psp_gp, isize);
   emit_sw(psp_at, psp_k0, _next_instr);

   emit_addiu(psp_at, psp_at, isize);
   emit_sw(psp_at, psp_k0, _R15);
}

static u32 classify_adr(u32 adr, bool store)
{
	if(block_procnum==ARMCPU_ARM9 && (adr & ~0x3FFF) == MMU.DTCMRegion)
		return MEMTYPE_DTCM;
	else if((adr & 0x0F000000) == 0x02000000)
		return MEMTYPE_MAIN;
	else if(block_procnum==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03800000)
		return MEMTYPE_ERAM;
	else if(block_procnum==ARMCPU_ARM7 && !store && (adr & 0xFF800000) == 0x03000000)
		return MEMTYPE_SWIRAM;
	else
		return MEMTYPE_GENERIC;
}


/////////
/// ARM
/////////

#define LSL_IMM                           \
	u32 imm = ((i>>7)&0x1F);               \
	emit_lw(psp_a0,RCPU, _reg_pos(0));     \
	if(imm) emit_sll(psp_a0,psp_a0, imm);

#define LSR_IMM                           \
	u32 imm = ((i>>7)&0x1F);               \
	if(imm)                                \
	{                                      \
      emit_lw(psp_a0,RCPU, _reg_pos(0));  \
		emit_srl(psp_a0, psp_a0, imm);      \
	}                                      \
	else                                   \
		emit_move(psp_a0, psp_zero); 

#define ASR_IMM                        \
   u32 imm = ((i>>7)&0x1F);            \
   emit_lw(psp_a0,RCPU, _reg_pos(0));  \
   if(!imm) imm = 31;                  \
   emit_sra(psp_a0,psp_a0,imm); 

#define IMM_VAL                           \
	u32 rhs = ROR((i&0xFF), (i>>7)&0x1E);  \
   emit_lui(psp_a0,rhs>>16);              \
   emit_ori(psp_a0,psp_a0,rhs&0xFFFF);

//Hlide
#define LSX_REG(name, op)                 \
   emit_lbu(psp_a1,RCPU,_reg_pos(8));     \
   emit_sltiu(psp_at,psp_a1, 32);         \
   emit_lw(psp_a0,RCPU,_reg_pos(0));      \
   op(psp_a0, psp_a0, psp_a1);            \
   emit_movz(psp_a0, psp_zero, psp_at);

//Hlide
#define ASX_REG(name, op)               \
   emit_lw(psp_a0,RCPU,_reg_pos(0));    \
   emit_lbu(psp_a1,RCPU,_reg_pos(8));   \
   emit_sltiu(psp_at,psp_a1, 32);       \
   emit_ext(psp_a2,psp_a1, 31, 31);     \
   emit_subu(psp_a2,psp_zero, psp_a2);  \
   op(psp_a0, psp_a0, psp_a1);          \
   emit_movz(psp_a0, psp_a2, psp_at);

#define LSL_REG LSX_REG(LSL_REG, emit_sllv)
#define LSR_REG LSX_REG(LSR_REG, emit_srlv)
#define ASR_REG ASX_REG(ASR_REG, emit_srav)

#define ROR_REG                        \
	emit_lw(psp_a0,RCPU,_reg_pos(0));   \
   emit_lbu(psp_a1,RCPU,_reg_pos(8));  \
	emit_rotrv(psp_a0,psp_a0,psp_a1);

#define ARITHM_PSP(arg, op)                                    \
   arg                                                         \
   if (!skip_load)   emit_lw(psp_a3, RCPU, _reg_pos(16));      \
   op(psp_a3, psp_a3,psp_a0);                                  \
   if (!skip_save)   emit_sw(psp_a3, RCPU, _reg_pos(12));      \
   if(REG_POS(i,12)==15)                                       \
      emit_sw(psp_a3, RCPU, _next_instr);                      \
   return OPR_RESULT(OPR_CONTINUE, 2);

#define ARITHM_PSP_R(arg, op)                                  \
   arg                                                         \
   if (!skip_load)   emit_lw(psp_a3, RCPU, _reg_pos(16));      \
   op(psp_a3, psp_a0,psp_a3);                                  \
   if (!skip_save)   emit_sw(psp_a3, RCPU, _reg_pos(12));      \
   if(REG_POS(i,12)==15)                                       \
      emit_sw(psp_a3, RCPU, _next_instr);                      \
   return OPR_RESULT(OPR_CONTINUE, 2);

#define ARITHM_PSP_C(arg, op, extra_op, c_op)                  \
   arg                                                         \
   emit_lw(psp_at,RCPU,_flags);                                \
   emit_ext(psp_at,psp_at,_flag_C,_flag_C);                    \
   extra_op                                                    \
   if (!skip_load) emit_lw(psp_a3, RCPU, _reg_pos(16));        \
   op(psp_a3, psp_a3,psp_a0);                                  \
   emit_##c_op(psp_a3,psp_a3,psp_at);                          \
   if (!skip_save)   emit_sw(psp_a3, RCPU, _reg_pos(12));      \
   if(REG_POS(i,12)==15)                                       \
      emit_sw(psp_a3, RCPU, _next_instr);                      \
   return OPR_RESULT(OPR_CONTINUE, 2);


static void emit_MMU_aluMemCycles(int alu_cycles, int population)
{
	if(block_procnum==ARMCPU_ARM9)
	{
		if(population < alu_cycles)
		{
         emit_movi(psp_t0, alu_cycles);
         emit_sltu(psp_at, RCYC, psp_t0);
         emit_movn(RCYC, psp_t0, psp_at);
		}
	}
	else
		emit_addiu(RCYC, RCYC, alu_cycles);
}

static OP_RESULT ARM_OP_AND_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_and); }
static OP_RESULT ARM_OP_AND_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_and); }
static OP_RESULT ARM_OP_AND_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_and); }
static OP_RESULT ARM_OP_AND_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_and); }
static OP_RESULT ARM_OP_AND_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_and); }
static OP_RESULT ARM_OP_AND_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_and); }
#define ARM_OP_AND_ROR_IMM 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, and_, 1, 0);*/ }
static OP_RESULT ARM_OP_AND_ROR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG, emit_and); }
static OP_RESULT ARM_OP_AND_IMM_VAL (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL, emit_and); }

static OP_RESULT ARM_OP_EOR_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_xor); }
static OP_RESULT ARM_OP_EOR_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_xor); }
static OP_RESULT ARM_OP_EOR_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_xor); }
static OP_RESULT ARM_OP_EOR_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_xor); }
static OP_RESULT ARM_OP_EOR_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_xor); }
static OP_RESULT ARM_OP_EOR_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_xor); }
#define ARM_OP_EOR_ROR_IMM  0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, xor_, 1, 0);*/ }
static OP_RESULT ARM_OP_EOR_ROR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG, emit_xor); }
static OP_RESULT ARM_OP_EOR_IMM_VAL (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL, emit_xor); }
 
static OP_RESULT ARM_OP_ORR_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_or); }
static OP_RESULT ARM_OP_ORR_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_or); }
static OP_RESULT ARM_OP_ORR_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_or); }
static OP_RESULT ARM_OP_ORR_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_or); }
static OP_RESULT ARM_OP_ORR_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_or); }
static OP_RESULT ARM_OP_ORR_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_or); }
#define ARM_OP_ORR_ROR_IMM 0//(uint32_t pc, const u32 i) { return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, or_, 1, 0);*/ }
static OP_RESULT ARM_OP_ORR_ROR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG, emit_or); }
static OP_RESULT ARM_OP_ORR_IMM_VAL (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL, emit_or); }
 
static OP_RESULT ARM_OP_ADD_LSL_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_addu); }
static OP_RESULT ARM_OP_ADD_LSL_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_addu); }
static OP_RESULT ARM_OP_ADD_LSR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_addu); }
static OP_RESULT ARM_OP_ADD_LSR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_addu); }
static OP_RESULT ARM_OP_ADD_ASR_IMM (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_addu); }
static OP_RESULT ARM_OP_ADD_ASR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_addu); }
#define ARM_OP_ADD_ROR_IMM 0//(uint32_t pc, const u32 i) = 0;//{ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, add, 1, 0);*/ }
static OP_RESULT ARM_OP_ADD_ROR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG, emit_addu); }
static OP_RESULT ARM_OP_ADD_IMM_VAL (uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL, emit_addu); }

static OP_RESULT ARM_OP_SUB_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM, emit_subu); }
static OP_RESULT ARM_OP_SUB_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG, emit_subu); }
static OP_RESULT ARM_OP_SUB_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM, emit_subu); }
static OP_RESULT ARM_OP_SUB_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG, emit_subu); }
static OP_RESULT ARM_OP_SUB_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM, emit_subu); }
static OP_RESULT ARM_OP_SUB_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG, emit_subu); }
#define ARM_OP_SUB_ROR_IMM 0//(uint32_t pc, const u32 i){ return OPR_INTERPRET;/*OP_ARITHMETIC(ROR_IMM, sub, 0, 0);*/ }
static OP_RESULT ARM_OP_SUB_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG, emit_subu); }
static OP_RESULT ARM_OP_SUB_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL, emit_subu);  }

static OP_RESULT ARM_OP_BIC_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_REG; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_IMM; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSR_REG; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_IMM; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ASR_REG; emit_not(psp_a0,psp_a0);, emit_and); }
//static OP_RESULT ARM_OP_BIC_ROR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(LSL_IMM; emit_not(psp_a0,psp_a0), emit_and, 0); }
static OP_RESULT ARM_OP_BIC_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(ROR_REG; emit_not(psp_a0,psp_a0);, emit_and); }
static OP_RESULT ARM_OP_BIC_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL; emit_not(psp_a0,psp_a0);, emit_and); }
#define ARM_OP_BIC_ROR_IMM 0



static OP_RESULT ARM_OP_SBC_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_IMM, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_REG, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_IMM, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_REG, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_IMM, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_REG, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
#define ARM_OP_SBC_ROR_IMM 0
static OP_RESULT ARM_OP_SBC_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ROR_REG, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }
static OP_RESULT ARM_OP_SBC_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(IMM_VAL, emit_subu, emit_xori(psp_at, psp_at, 1);, subu); }



static OP_RESULT ARM_OP_RSB_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(LSL_IMM, emit_subu); }
static OP_RESULT ARM_OP_RSB_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(LSL_REG, emit_subu); }
static OP_RESULT ARM_OP_RSB_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(LSR_IMM, emit_subu); }
static OP_RESULT ARM_OP_RSB_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(LSR_REG, emit_subu); }
static OP_RESULT ARM_OP_RSB_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(ASR_IMM, emit_subu); }
static OP_RESULT ARM_OP_RSB_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(ASR_REG, emit_subu); }
#define ARM_OP_RSB_ROR_IMM 0
static OP_RESULT ARM_OP_RSB_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(ROR_REG, emit_subu); }
static OP_RESULT ARM_OP_RSB_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_R(IMM_VAL, emit_subu); }



static OP_RESULT ARM_OP_RSC_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_IMM, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_REG, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_IMM, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_REG, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_IMM, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_REG, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
#define ARM_OP_RSC_ROR_IMM 0
static OP_RESULT ARM_OP_RSC_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ROR_REG, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }
static OP_RESULT ARM_OP_RSC_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(IMM_VAL, emit_subu, emit_addiu(psp_at, psp_at, -1);, addu); }

static OP_RESULT ARM_OP_ADC_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_IMM, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSL_REG, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_IMM, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(LSR_REG, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_IMM, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ASR_REG, emit_addu,, addu); }
#define ARM_OP_ADC_ROR_IMM 0//(const u32 i) { OP_ARITHMETIC(ROR_IMM; GET_CARRY(0), adc, 1, 0); }
static OP_RESULT ARM_OP_ADC_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP_C(ROR_REG, emit_addu,, addu); }
static OP_RESULT ARM_OP_ADC_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); ARITHM_PSP(IMM_VAL; emit_lw(psp_at,RCPU,_flags);
   emit_ext(psp_at,psp_at,_flag_C,_flag_C); emit_addu(psp_a0,psp_a0,psp_at);, emit_addu); }
 
//-----------------------------------------------------------------------------
//   MOV
//-----------------------------------------------------------------------------
#define OP_MOV(arg)                          \
   arg;                                      \
   emit_sw(psp_a0, RCPU, _reg_pos(12));      \
   if(REG_POS(i,12)==15)                     \
      emit_sw(psp_a0, RCPU, _next_instr);    \
    return OPR_RESULT(OPR_CONTINUE, 1);

static OP_RESULT ARM_OP_MOV_LSL_IMM(uint32_t pc, const u32 i) { if (i == 0xE1A00000) { /*NOP*/ skip_prefeth = true; emit_Variableprefetch(pc,2); return OPR_RESULT(OPR_CONTINUE, 1); } sync_r15(i, false, 0); OP_MOV(LSL_IMM); }
static OP_RESULT ARM_OP_MOV_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSL_REG; if (REG_POS(i,0) == 15) emit_addiu(psp_a0, psp_a0, 4);); }
static OP_RESULT ARM_OP_MOV_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSR_IMM); }
static OP_RESULT ARM_OP_MOV_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(LSR_REG; if (REG_POS(i,0) == 15) emit_addiu(psp_a0, psp_a0, 4);); }
static OP_RESULT ARM_OP_MOV_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(ASR_IMM); }
static OP_RESULT ARM_OP_MOV_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(ASR_REG); }
static OP_RESULT ARM_OP_MOV_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(IMM_VAL); }
#define ARM_OP_MOV_ROR_IMM 0
static OP_RESULT ARM_OP_MOV_ROR_REG (uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_MOV(ROR_REG); }


//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------
static OP_RESULT ARM_OP_MVN_LSL_IMM(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(LSL_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSL_REG(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(LSL_REG; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSR_IMM(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(LSR_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_LSR_REG(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(LSR_REG; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_ASR_IMM(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(ASR_IMM; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_ASR_REG(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(ASR_REG; emit_not(psp_a0, psp_a0)); }
static OP_RESULT ARM_OP_MVN_IMM_VAL(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(IMM_VAL; emit_not(psp_a0, psp_a0)); }
#define ARM_OP_MVN_ROR_IMM 0
static OP_RESULT ARM_OP_MVN_ROR_REG(uint32_t pc, const u32 i) {sync_r15(i, false, 0); OP_MOV(ROR_REG; emit_not(psp_a0, psp_a0)); }


//-----------------------------------------------------------------------------
//   CMP
//-----------------------------------------------------------------------------

#define OP_CMP(arg) \
   arg \
   emit_lw(psp_a2,RCPU,_reg_pos(16));                 \
   emit_subu(psp_a3,psp_a2,psp_a0);                   \
   emit_lbu(psp_at,RCPU,_flags+3);                    \
   emit_ext(psp_a1,psp_a3,31,31);                     \
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);        \
   emit_slt(psp_t0,psp_a2,psp_a0);                    \
   emit_xor(psp_a1, psp_t0, psp_a1);                  \
   emit_ins(psp_at,psp_a1,_flag_V8,_flag_V8);         \
   emit_sltiu(psp_a1,psp_a3,1);                       \
   emit_ins(psp_at,psp_a1,_flag_Z8,_flag_Z8);         \
   emit_sltu(psp_a1,psp_a2,psp_a0);                   \
   emit_xori(psp_a1,psp_a1, 1);                       \
   emit_ins(psp_at,psp_a1,_flag_C8,_flag_C8);         \
   emit_sb(psp_at,RCPU,_flags+3);                     \
   return OPR_RESULT(OPR_CONTINUE, 1);           

static OP_RESULT ARM_OP_CMP_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(LSL_IMM); }
static OP_RESULT ARM_OP_CMP_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(LSL_REG); }
static OP_RESULT ARM_OP_CMP_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(LSR_IMM); }
static OP_RESULT ARM_OP_CMP_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(LSR_REG); }
static OP_RESULT ARM_OP_CMP_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(ASR_IMM); }
static OP_RESULT ARM_OP_CMP_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(ASR_REG); }
#define          ARM_OP_CMP_ROR_IMM                                                                     0 
static OP_RESULT ARM_OP_CMP_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(ROR_REG); }
static OP_RESULT ARM_OP_CMP_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_CMP(IMM_VAL); }

#define OP_TST(arg) \
   arg \
   emit_lw(psp_a2,RCPU,_reg_pos(16));                 \
   emit_and(psp_a3,psp_a2,psp_a0);                   \
   emit_lbu(psp_at,RCPU,_flags+3);                    \
   emit_ext(psp_a1,psp_a3,31,31);                     \
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);        \
   emit_sltiu(psp_a1,psp_a3,1);                       \
   emit_ins(psp_at,psp_a1,_flag_Z8,_flag_Z8);         \
   emit_slt(psp_a1,psp_a0,psp_a2);                    \
   emit_ins(psp_at,psp_a1,_flag_C8,_flag_C8);         \
   emit_sb(psp_at,RCPU,_flags+3);                     \
   return OPR_RESULT(OPR_CONTINUE, 1);           

static OP_RESULT ARM_OP_TST_LSL_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(LSL_IMM); }
static OP_RESULT ARM_OP_TST_LSL_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(LSL_REG); }
static OP_RESULT ARM_OP_TST_LSR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(LSR_IMM); }
static OP_RESULT ARM_OP_TST_LSR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(LSR_REG); }
static OP_RESULT ARM_OP_TST_ASR_IMM(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(ASR_IMM); }
static OP_RESULT ARM_OP_TST_ASR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(ASR_REG); }
#define          ARM_OP_TST_ROR_IMM                                                                     0 
static OP_RESULT ARM_OP_TST_ROR_REG(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(ROR_REG); }
static OP_RESULT ARM_OP_TST_IMM_VAL(uint32_t pc, const u32 i) { sync_r15(i, false, 0); OP_TST(IMM_VAL); }

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

ARM_ALU_OP_DEF(AND_S, 2, 1, true);
ARM_ALU_OP_DEF(EOR_S, 2, 1, true);
ARM_ALU_OP_DEF(SUB_S, 2, 1, true);
ARM_ALU_OP_DEF(RSB_S, 2, 1, true);
ARM_ALU_OP_DEF(ADD_S, 2, 1, true);
ARM_ALU_OP_DEF(ADC_S, 2, 1, true);
ARM_ALU_OP_DEF(SBC_S, 2, 1, true);
ARM_ALU_OP_DEF(RSC_S, 2, 1, true);
ARM_ALU_OP_DEF(TEQ  , 0, 1, true);
ARM_ALU_OP_DEF(CMN  , 0, 1, true);
ARM_ALU_OP_DEF(ORR_S, 2, 1, true);
ARM_ALU_OP_DEF(MOV_S, 2, 0, true);
ARM_ALU_OP_DEF(BIC_S, 2, 1, true);
ARM_ALU_OP_DEF(MVN_S, 2, 0, true);

static void MUL_Mxx_END(bool sign, int cycles)
{
	if(sign)
	{
      emit_move(psp_at, psp_a1);
      emit_sra(psp_a1, psp_a1, 31);
      emit_xor(psp_a1, psp_a1, psp_at);
	}

   emit_ori(psp_a1, psp_a1, 1);

   emit_clz(RCYC, psp_a1);
   emit_srl(RCYC, RCYC, 3);
   emit_addiu(RCYC, RCYC, cycles+1);
}

static OP_RESULT ARM_OP_MUL(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_lw(psp_a1,RCPU,_reg_pos(8));

   emit_mult(psp_a0,psp_a1);

   emit_mflo(psp_a0);
   emit_sw(psp_a0,RCPU,_reg_pos(16));

   MUL_Mxx_END(1, 3);

   return OPR_RESULT(OPR_CONTINUE, 5);
}

static OP_RESULT ARM_OP_MLA(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_lw(psp_a1,RCPU,_reg_pos(8));

   emit_mult(psp_a0,psp_a1);

   emit_mflo(psp_a0);

   emit_lw(psp_a2,RCPU,_reg_pos(12));

   emit_addu(psp_a0,psp_a0,psp_a2);

   emit_sw(psp_a0,RCPU,_reg_pos(16));

   MUL_Mxx_END(1, 4);

   return OPR_RESULT(OPR_CONTINUE, 5);
}


static OP_RESULT ARM_OP_UMULL(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_lw(psp_a1,RCPU,_reg_pos(8));

   emit_multu(psp_a0,psp_a1);

   emit_mflo(psp_a0);
   emit_sw(psp_a0,RCPU,_reg_pos(12));

   emit_mfhi(psp_at);
   emit_sw(psp_at,RCPU,_reg_pos(16));

   MUL_Mxx_END(0, 4);

   return OPR_RESULT(OPR_CONTINUE, 6);
}

static OP_RESULT ARM_OP_SMULL(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_lw(psp_a1,RCPU,_reg_pos(8));

   emit_mult(psp_a0,psp_a1);

   emit_mflo(psp_a0);
   emit_sw(psp_a0,RCPU,_reg_pos(12));

   emit_mfhi(psp_at);
   emit_sw(psp_at,RCPU,_reg_pos(16));

   MUL_Mxx_END(1, 4);

   return OPR_RESULT(OPR_CONTINUE, 6);
}

static OP_RESULT ARM_OP_UMLAL(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_lw(psp_a1,RCPU,_reg_pos(8));

   emit_multu(psp_a0,psp_a1);

   emit_mflo(psp_a0);

   emit_lw(psp_a2,RCPU,_reg_pos(12));

   emit_addu(psp_a0,psp_a0,psp_a2);

   emit_sw(psp_a0,RCPU,_reg_pos(16));

   MUL_Mxx_END(0, 4);

   return OPR_RESULT(OPR_CONTINUE, 5);
}

#define ARM_OP_MUL_S 0
#define ARM_OP_MLA_S 0
#define ARM_OP_MLA_S 0
#define ARM_OP_MUL_S 0

#define ARM_OP_UMULL_S     0
#define ARM_OP_UMLAL_S     0
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

//#define ARM_OP_CLZ         0
static OP_RESULT ARM_OP_CLZ(uint32_t pc, const u32 i) {

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg_pos(0));
   emit_clz(psp_a1,psp_a0);
   emit_sw(psp_a1,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

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
//ARM_MEM_HALF_OP_DEF(LDRH);
ARM_MEM_HALF_OP_DEF(STRSB);
ARM_MEM_HALF_OP_DEF(LDRSB);
ARM_MEM_HALF_OP_DEF(STRSH);
ARM_MEM_HALF_OP_DEF(LDRSH);

static OP_RESULT ARM_OP_LDRH_P_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   emit_addiu(psp_a0,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_M_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   emit_subiu(psp_a0,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_P_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   emit_addu(psp_a0,psp_a0,psp_a1);

   emit_sw(psp_v0,RCPU,_reg_pos(12));

  return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_M_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   emit_subu(psp_a0,psp_a0,psp_a1);

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_PRE_INDE_P_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   emit_addiu(psp_a0,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a0,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_PRE_INDE_M_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   emit_subiu(psp_a0,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a0,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_PRE_INDE_P_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   emit_addu(psp_a0,psp_a0,psp_a1);

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a0,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_PRE_INDE_M_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   emit_subu(psp_a0,psp_a0,psp_a1);

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a0,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_POS_INDE_P_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   emit_addiu(psp_a2,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a2,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_POS_INDE_M_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));

   emit_subiu(psp_a2,psp_a0,(((i>>4)&0xF0)+(i&0xF)));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a2,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_POS_INDE_P_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   emit_addu(psp_a2,psp_a0,psp_a1);

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a2,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}
static OP_RESULT ARM_OP_LDRH_POS_INDE_M_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(16));
   emit_lw(psp_a1, RCPU, _reg_pos(0));

   emit_subu(psp_a2,psp_a0,psp_a1);

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);
   emit_sw(psp_a2,RCPU,_reg_pos(16));

   emit_sw(psp_v0,RCPU,_reg_pos(12));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

#define ARM_OP_B  0
#define ARM_OP_BL 0

//-----------------------------------------------------------------------------
//   MRS / MSR
//-----------------------------------------------------------------------------
static OP_RESULT ARM_OP_MRS_CPSR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);
   emit_lw(psp_at,RCPU,_flags); //CPSR ADDR
   emit_sw(psp_at,RCPU,_reg_pos(12));
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT ARM_OP_MRS_SPSR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);
	emit_lw(psp_a0,RCPU,_flags + 4); //SPSR ADDR
   emit_sw(psp_a0,RCPU,_reg_pos(12));
	return OPR_RESULT(OPR_CONTINUE, 1);
}

//static OP_RESULT ARM_OP_BKPT(uint32_t pc, const u32 i) { emit_prefetch(); return OPR_RESULT(OPR_CONTINUE, 1); }

//-----------------------------------------------------------------------------
//   SWP/SWPB
//-----------------------------------------------------------------------------
template<int PROCNUM>
static u32 FASTCALL op_swp(u32 adr, u32 Rd, u32 Rs)
{
	u32 tmp = ROR(READ32(cpu->mem_if->data, adr), (adr & 3)<<3);
	WRITE32(cpu->mem_if->data, adr, Rs);
	ARMPROC.R[Rd] = tmp; 
   //interpreted_cycles += (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr));
	return 0;//return (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr));
}
template<int PROCNUM>
static u32 FASTCALL op_swpb(u32 adr, u32 Rd, u32 Rs)
{
	u32 tmp = READ8(cpu->mem_if->data, adr);
	WRITE8(cpu->mem_if->data, adr, Rs);
	ARMPROC.R[Rd] = tmp;
	//interpreted_cycles += (MMU_memAccessCycles<PROCNUM,8,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,8,MMU_AD_WRITE>(adr));
	return (MMU_memAccessCycles<PROCNUM,8,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,8,MMU_AD_WRITE>(adr));
}

typedef u32 FASTCALL (*OP_SWP_SWPB)(u32, u32, u32);
static const OP_SWP_SWPB op_swp_tab[2][2] = {{ op_swp<0>, op_swp<1> }, { op_swpb<0>, op_swpb<1> }};

static OP_RESULT op_swp_(const u32 i, int b)
{
   emit_lw(psp_a0,RCPU,_reg_pos(16));

   emit_movi(psp_a1,REG_POS(i,12));

	emit_jal(op_swp_tab[b][block_procnum]);

   emit_lw(psp_a2,RCPU,_reg_pos(0));

   emit_move(RCYC, psp_v0);

   emit_MMU_aluMemCycles(4, 0);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT ARM_OP_SWP(uint32_t pc, const u32 i) { sync_r15(i, false, 0); return op_swp_(i, 0);  };
static OP_RESULT ARM_OP_SWPB(uint32_t pc, const u32 i){ sync_r15(i, false, 0); return op_swp_(i, 1);  };


static void maskPrecalc(u32 _num)
{
#define precalc(num) {  \
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is allways false (disabled) */  \
	if (BIT_N(cp15.protectBaseSize[num],0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
		mask = CP15_MASKFROMREG(cp15.protectBaseSize[num]) ;   \
		set = CP15_SETFROMREG(cp15.protectBaseSize[num]) ; \
		if (CP15_SIZEIDENTIFIER(cp15.protectBaseSize[num])==0x1F)  \
		{   /* for the 4GB region, u32 suffers wraparound */   \
			mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
		} \
	}  \
	cp15.setSingleRegionAccess(num, mask, set) ;  \
}
	switch(_num)
	{
		case 0: precalc(0); break;
		case 1: precalc(1); break;
		case 2: precalc(2); break;
		case 3: precalc(3); break;
		case 4: precalc(4); break;
		case 5: precalc(5); break;
		case 6: precalc(6); break;
		case 7: precalc(7); break;

		case 0xFF:
			precalc(0);
			precalc(1);
			precalc(2);
			precalc(3);
			precalc(4);
			precalc(5);
			precalc(6);
			precalc(7);
		break;
	}
#undef precalc
}

void emit_moveARMCP(const u32 i){

	u8 opcode1 = (i>>21)&0x7;
	u8 opcode2 = (i>>5)&0x7;
   u8 CRn     = REG_POS(i, 16);
   u8 CRm     = REG_POS(i, 0);

   emit_lw(psp_a0, RCPU, _reg_pos(12));

	switch(CRn)
	{
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
         emit_lui(psp_a1, 0xf);
         emit_ori(psp_a1, psp_a1,0xF085);

			//On the NDS bit0,2,7,12..19 are R/W, Bit3..6 are always set, all other bits are always zero.
         emit_and(psp_at,psp_a0,psp_a1);
         emit_ori(psp_at,psp_at, 0x78);

         emit_ext(psp_a2, psp_a0, 7, 7);

         emit_sw(psp_at,RCPU,_cp15(ctrl));

         emit_ext(psp_a3, psp_a0, 13, 13);

         emit_sw(psp_a2, RCPU,_MMU(ARM9_RW_MODE));

			//zero 31-jan-2010: change from 0x0FFF0000 to 0xFFFF0000 per gbatek

         emit_lui(psp_a1, 0xffff);

         emit_mult(psp_a1, psp_a3);

         emit_mflo(psp_at);

         emit_sw(psp_at, RCPU, _NDS_ARM9(intVector));

         emit_ext(psp_a1, psp_a0, 15, 15);

         emit_not(psp_a1,psp_a1);

         emit_sw(psp_a1, RCPU, _NDS_ARM9(LDTBit));
		
			//LOG("CP15: ARMtoCP ctrl %08X (val %08X)\n", ctrl, val);
		}
		return;
	case 2:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 0:
            emit_sw(psp_a0, RCPU, _cp15(DCConfig));
				return;
			case 1:
            emit_sw(psp_a0, RCPU, _cp15(ICConfig));
				return;
			default:
				return;
			}
		}
		return;
	case 3:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
         emit_sw(psp_a0, RCPU, _cp15(writeBuffCtrl));
			//LOG("CP15: ARMtoCP writeBuffer ctrl %08X\n", writeBuffCtrl);
			return;
		}
		return;
	case 5:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 2:
            emit_sw(psp_a0, RCPU, _cp15(DaccessPerm));
            emit_jal(maskPrecalc);
            emit_movi(psp_a0,0xff);      
				return;
			case 3:
            emit_sw(psp_a0, RCPU, _cp15(IaccessPerm));
            emit_jal(maskPrecalc);
            emit_movi(psp_a0,0xff);   
				return;
			default:
				return;
			}
		}
		return;
	case 6:
		if((opcode1==0) && (opcode2==0))
		{
			if (CRm < 8)
			{
            emit_sw(psp_a0, RCPU, _cp15(protectBaseSize[CRm]));
            emit_jal(maskPrecalc);
            emit_movi(psp_a0,CRm); 
				return;
			}
		}
		return;
	case 7:
		if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
		{
			//CP15wait4IRQ;
         emit_movi(psp_a1, CPU_FREEZE_IRQ_IE_IF);
         emit_sw(psp_a1, RCPU, _NDS_ARM9(freeze));
			//IME set deliberately omitted: only SWI sets IME to 1
			return;
		}
		return;
	case 9:
		if((opcode1==0))
		{
			switch(CRm)
			{
			case 0:
				switch(opcode2)
				{
				case 0:
               emit_sw(psp_a0, RCPU, _cp15(DcacheLock));
					return;
				case 1:
               emit_sw(psp_a0, RCPU, _cp15(IcacheLock));
					return;
				default:
					return;
				}
			case 1:
				switch(opcode2)
				{
				case 0:
               emit_lui(psp_a1,0x0FFF);
               emit_ori(psp_a1, psp_a1, 0xF000);
               emit_and(psp_a1, psp_a0, psp_a1);

               emit_sw(psp_a1, RCPU, _cp15(DTCMRegion));
               emit_sw(psp_a1, RCPU, _MMU(DTCMRegion));
					return;
				case 1:
               emit_sw(psp_a0, RCPU, _cp15(ITCMRegion));

					//ITCM base is not writeable!
               emit_sw(psp_zero, RCPU, _MMU(ITCMRegion));
					return;
				default:
					return;
				}
			}
		}
		return;
	default:
		return;
	}
}

static OP_RESULT ARM_OP_MCR(uint32_t pc, const u32 i){
   
   sync_r15(i, false, 0);

   if (block_procnum == ARMCPU_ARM7) return OPR_RESULT(OPR_CONTINUE, 1);

	if (REG_POS(i, 8)  != 15) return OPR_RESULT(OPR_CONTINUE, 1);
   if (REG_POS(i, 12) == 15) return OPR_RESULT(OPR_CONTINUE, 1);

   emit_moveARMCP(i);

   return OPR_RESULT(OPR_CONTINUE, 1);
}



static OP_RESULT ARM_OP_SWI(uint32_t pc, const u32 i){
   
   sync_r15(i, false, 0);

   u32 last_op = emit_SlideDelay();

   emit_jal(_ARMPROC.swi_tab[i & 0x1F]);
   emit_Write32(last_op);
   
   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT ARM_OP_BX(uint32_t pc, const u32 i){
   
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU, _reg_pos(0));

   emit_movi(psp_a1,-4);

   emit_ext(psp_a2,psp_a0,0,0);

   emit_and(psp_a0, psp_a0, psp_a1);

   emit_ins(psp_at, psp_a2, _flag_T, _flag_T);

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_sb(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

//-----------------------------------------------------------------------------
//   LDRD / STRD
//-----------------------------------------------------------------------------

#define cpu (&ARMPROC)

typedef u32 FASTCALL (*LDRD_STRD_REG)(u32);
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_LDRD_REG(u32 adr)
{
	cpu->R[Rnum] = READ32(cpu->mem_if->data, adr);
	
	// For even-numbered registers, we'll do a double-word load. Otherwise, we'll just do a single-word load.
	if ((Rnum & 0x01) == 0)
	{
		cpu->R[Rnum+1] = READ32(cpu->mem_if->data, adr+4);
		interpreted_cycles += (MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr+4));
      return 0;
   }
	
	interpreted_cycles += MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
   return 0;
}
template<int PROCNUM, u8 Rnum>
static u32 FASTCALL OP_STRD_REG(u32 adr)
{
	WRITE32(cpu->mem_if->data, adr, cpu->R[Rnum]);
	
	// For even-numbered registers, we'll do a double-word store. Otherwise, we'll just do a single-word store.
	if ((Rnum & 0x01) == 0)
	{
		WRITE32(cpu->mem_if->data, adr+4, cpu->R[Rnum + 1]);
		interpreted_cycles += (MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr) + MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr+4));
      return 0;
   }
	
	interpreted_cycles += MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
   return 0;
}
#define T(op, proc) op<proc,0>, op<proc,1>, op<proc,2>, op<proc,3>, op<proc,4>, op<proc,5>, op<proc,6>, op<proc,7>, op<proc,8>, op<proc,9>, op<proc,10>, op<proc,11>, op<proc,12>, op<proc,13>, op<proc,14>, op<proc,15>
static const LDRD_STRD_REG op_ldrd_tab[2][16] = { {T(OP_LDRD_REG, 0)}, {T(OP_LDRD_REG, 1)} };
static const LDRD_STRD_REG op_strd_tab[2][16] = { {T(OP_STRD_REG, 0)}, {T(OP_STRD_REG, 1)} };
#undef T

static OP_RESULT ARM_OP_LDRD_STRD_POST_INDEX(uint32_t pc, const u32 i){
   
   sync_r15(i, false, 0);

   u8 Rd_num = REG_POS(i, 12);

   if (Rd_num == 14)
	{
		printf("OP_LDRD_STRD_POST_INDEX: use R14!!!!\n");
		return OPR_RESULT(OPR_CONTINUE, 1);
	}
	if (Rd_num & 0x1)
	{
		printf("OP_LDRD_STRD_POST_INDEX: ERROR!!!!\n");
		return OPR_RESULT(OPR_CONTINUE, 1);
	}
   
   emit_lw(psp_a0, RCPU, _reg_pos(16));

   #define IMM_OFF (((i>>4)&0xF0)+(i&0xF))

   // I bit - immediate or register
	if (BIT22(i))
	{
		if (BIT23(i)) emit_addiu(psp_at, psp_a0, IMM_OFF); else emit_subiu(psp_at, psp_a0, IMM_OFF);
	}
	else
	{
      emit_lw(psp_a1, RCPU, _reg_pos(0));
		if (BIT23(i)) emit_addu(psp_at, psp_a0, psp_a1); else emit_subu(psp_at, psp_a0, psp_a1);
	}

	emit_jal((void*)(BIT5(i) ? op_strd_tab[block_procnum][Rd_num] : op_ldrd_tab[block_procnum][Rd_num]));
   
   emit_sw(psp_at, RCPU, _reg_pos(16));

	emit_MMU_aluMemCycles(3, 0);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

#define ARM_OP_LDRD_STRD_OFFSET_PRE_INDEX 0
#define ARM_OP_MSR_CPSR 0
#define ARM_OP_BX 0
#define ARM_OP_BLX_REG 0
#define ARM_OP_BKPT 0
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
#define ARM_OP_MRC 0
#define ARM_OP_UND 0
static const ArmOpCompiler arm_instruction_compilers[4096] = {
#define TABDECL(x) ARM_##x
#include "instruction_tabdef.inc"
#undef TABDECL
};

////////
// THUMB
////////

static OP_RESULT THUMB_OP_ASR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 v = (i>>6) & 0x1F;

   emit_lw(psp_at,RCPU,_flags); //load flag reg

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_ext(psp_a1,psp_a0,v-1,v-1);
   emit_ins(psp_at,psp_a1,_flag_C, _flag_C); //C 

   emit_srl(psp_a0,psp_a0,v);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N); //N 

   emit_movi(psp_a1,1);
   emit_movn(psp_a1,psp_zero,psp_a0);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z); //Z 

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LSL_0(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   emit_ext(psp_a1,psp_a0,31,31);

   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

   emit_lw(psp_at,RCPU,_flags);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LSL(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 v = (i>>6) & 0x1F;

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_ext(psp_a1,psp_a0,32-v,32-v);

   emit_lw(psp_at,RCPU,_flags); //load flag reg
   emit_ins(psp_at,psp_a1,_flag_C, _flag_C); //C 

   emit_sll(psp_a0,psp_a0,v);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N); //N 

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z); //Z 

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LSR_0(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   emit_lw(psp_at,RCPU,_flags); //load flag reg

   emit_ext(psp_a1,psp_a0,31,31);
   
   emit_ins(psp_at,psp_a1,_flag_C, _flag_C); //C

   emit_sw(psp_zero,RCPU,_thumb_reg_pos(0));

   emit_ins(psp_at,psp_zero,_flag_N, _flag_N); //N 

   emit_ori(psp_at,psp_at,1 << _flag_Z);   //Z 

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LSR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 v = (i>>6) & 0x1F;

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   
   emit_ext(psp_a1,psp_a0,v-1,v-1);

   emit_lw(psp_at,RCPU,_flags); //load flag reg

   emit_ins(psp_at,psp_a1,_flag_C, _flag_C); //C 

   if (v != 0) emit_srl(psp_a0,psp_a0,v);

   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N); //N 

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z,_flag_Z); //Z 

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_MOV_IMM8(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   if ((i&0xFF) != 0) {

      emit_movi(psp_a0, (i&0xFF));

      emit_lbu(psp_at,RCPU,_flags + 3);

      emit_ori(psp_at, psp_at, ((i&0xFF)>>31) << _flag_N8);

      emit_sw(psp_a0,RCPU,_thumb_reg_pos(8));
      
      emit_ori(psp_at, psp_at, ((u32)(i&0xFF) != 0) << _flag_Z8);

      emit_sb(psp_at,RCPU,_flags + 3);

      return OPR_RESULT(OPR_CONTINUE, 1);
   }

   emit_lw(psp_at,RCPU,_flags);

   emit_ins(psp_at,psp_zero,_flag_Z, _flag_N);

   emit_sw(psp_zero,RCPU,_thumb_reg_pos(8));

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_ADD_IMM8(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags+3);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(8));

   u8 imm = (i&0xFF);

   emit_addiu(psp_a2,psp_a0,imm);

   emit_sw(psp_a2,RCPU,_thumb_reg_pos(8));
   
   emit_ext(psp_t0,psp_a2,31,31);
   emit_ins(psp_at,psp_t0,_flag_N8, _flag_N8);

   emit_slt(psp_t0, psp_a2, psp_a0);
   emit_xori(psp_t0, psp_t0, imm);
   emit_ins(psp_at,psp_t0,_flag_V8, _flag_V8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_ori(psp_at,psp_at,1<<_flag_C8);

   emit_sb(psp_at,RCPU,_flags + 3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_IMM3(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags + 3);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   u8 imm = (i>>6)&0x07;

   if (imm == 0)	// mov 2
	{
      emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

      emit_ext(psp_a2,psp_a0,31,31);
      emit_ins(psp_at,psp_a2,_flag_N8, _flag_N8);

      emit_sltiu(psp_a2, psp_a0, 1);
      emit_ins(psp_at,psp_a2,_flag_Z8, _flag_Z8);

      emit_ins(psp_at,psp_zero,_flag_C8, _flag_V8);

      emit_sb(psp_at,RCPU,_flags + 3);

		return OPR_RESULT(OPR_CONTINUE, 1);
	}

   emit_addiu(psp_a2,psp_a0,imm);

   emit_sw(psp_a2,RCPU,_thumb_reg_pos(0));

   emit_slt(psp_t0, psp_a2, psp_a0);
   emit_xori(psp_t0, psp_t0, imm);
   emit_ins(psp_at,psp_t0,_flag_V8, _flag_V8);
   
   emit_ext(psp_a1,psp_a2,31,31);
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_ori(psp_at,psp_at,1<<_flag_C8);

   emit_sb(psp_at,RCPU,_flags + 3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_CMP_IMM8(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags + 3);

   u8 imm = (i&0xFF); emit_movi(psp_a1,-imm);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(8));

   emit_addu(psp_a2,psp_a0,psp_a1);

   emit_xor(psp_a3,psp_a1,psp_a2);
   emit_xor(psp_a1,psp_a0,psp_a2);
   emit_and(psp_a3,psp_a3,psp_a1);

   emit_ext(psp_a3,psp_a3,31,31);
   emit_ins(psp_at,psp_a3,_flag_V8, _flag_V8);
   
   emit_ext(psp_a1,psp_a2,31,31);
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_sltiu(psp_a1, psp_a0, imm);
   emit_not(psp_a1,psp_a1);
   emit_ins(psp_at,psp_a1,_flag_C8, _flag_C8);

   emit_sb(psp_at,RCPU,_flags+ 3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_SUB_IMM8(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags + 3);

   u8 imm = (i&0xFF);  emit_movi(psp_a1,-imm);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(8));

   emit_addu(psp_a2,psp_a0,psp_a1);

   emit_sw(psp_a2,RCPU,_thumb_reg_pos(8));

   emit_xor(psp_a3,psp_a1,psp_a2);
   emit_xor(psp_a1,psp_a0,psp_a2);
   emit_and(psp_a3,psp_a3,psp_a1);

   emit_ext(psp_a3,psp_a3,31,31);
   emit_ins(psp_at,psp_a3,_flag_V8, _flag_V8);
   
   emit_ext(psp_a1,psp_a2,31,31);
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_sltiu(psp_a1, psp_a0, imm);
   emit_not(psp_a1,psp_a1);
   emit_ins(psp_at,psp_a1,_flag_C8, _flag_C8);

   emit_sb(psp_at,RCPU,_flags + 3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_SUB_IMM3(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags + 3);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   
   u8 imm = (i>>6)&0x07;

   emit_movi(psp_a1,-imm);

   emit_add(psp_a2,psp_a0,psp_a1);

   emit_sw(psp_a2,RCPU,_thumb_reg_pos(0));

   emit_xor(psp_a3,psp_a1,psp_a2);
   emit_xor(psp_a1,psp_a0,psp_a2);
   emit_and(psp_a3,psp_a3,psp_a1);

   emit_ext(psp_a3,psp_a3,31,31);
   emit_ins(psp_at,psp_a3,_flag_V8, _flag_V8);
   
   emit_ext(psp_a1,psp_a2,31,31);
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_sltiu(psp_a1, psp_a0, imm);
   emit_not(psp_a1,psp_a1);
   emit_ins(psp_at,psp_a1,_flag_C8, _flag_C8);

   emit_sb(psp_at,RCPU,_flags+3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

//-----------------------------------------------------------------------------
//   AND
//-----------------------------------------------------------------------------

static OP_RESULT THUMB_OP_AND(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));
   emit_and(psp_a0,psp_a0,psp_a1);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));
   
   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_BIC(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));

   emit_not(psp_a1,psp_a1);
   emit_and(psp_a0,psp_a0,psp_a1);

   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));
   
   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_CMN(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lbu(psp_at,RCPU,_flags+3);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(8));

   emit_add(psp_a2,psp_a0,psp_a1);

   emit_xor(psp_a3,psp_a1,psp_a2);
   emit_xor(psp_a1,psp_a0,psp_a2);
   emit_and(psp_a3,psp_a3,psp_a1);

   emit_ext(psp_a3,psp_a3,31,31);
   emit_ins(psp_at,psp_a3,_flag_V8, _flag_V8);
   
   emit_ext(psp_a1,psp_a2,31,31);
   emit_ins(psp_at,psp_a1,_flag_N8, _flag_N8);

   emit_sltiu(psp_a1, psp_a2, 1);
   emit_ins(psp_at,psp_a1,_flag_Z8, _flag_Z8);

   emit_ori(psp_at,psp_at,1<<_flag_C8);

   emit_sb(psp_at,RCPU,_flags+3);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


//-----------------------------------------------------------------------------
//   EOR
//-----------------------------------------------------------------------------

static OP_RESULT THUMB_OP_EOR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));
   emit_xor(psp_a0,psp_a0,psp_a1);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));
   
   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------

static OP_RESULT THUMB_OP_MVN(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);
   
	emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_not(psp_a0,psp_a0);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));
   
   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ORR(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));
   emit_or(psp_a0,psp_a0,psp_a1);
   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));
   
   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------
static OP_RESULT THUMB_OP_TST(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));

   emit_and(psp_a0,psp_a0,psp_a1);

	emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ROR_REG(uint32_t pc, const u32 i)
{
   return OPR_INTERPRET;

   sync_r15(i, false, 0);

   emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(0));
   emit_lbu(psp_a1,RCPU,_thumb_reg_pos(3) + 3);

   //emit_movz

   emit_and(psp_a0,psp_a0,psp_a1);

	emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_MOV_SPE(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);

	//cpu->R[Rd] = cpu->R[REG_POS(i, 3)];
   emit_lw(psp_a0,RCPU,_reg_pos(3));
	emit_sw(psp_a0,RCPU,_reg(Rd));

	if(Rd == 15)
	{
      interpreted_cycles += 4;

		emit_sw(psp_a0,RCPU,_next_instr);
	}
	
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_SPE(uint32_t pc, const u32 i){
   sync_r15(i, false, 0);

   u32 Rd = _REG_NUM(i, 0) | ((i>>4)&8);

   emit_lw(psp_a0,RCPU,_reg(Rd));
   emit_lw(psp_a1,RCPU,_reg_pos(3));
   emit_addu(psp_a0,psp_a0,psp_a1);
   emit_sw(psp_a0,RCPU,_reg(Rd));

   if(Rd==15)
		emit_sw(psp_a0,RCPU,_next_instr);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_MUL_REG(uint32_t pc, const u32 i){
   
   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(0));

   emit_mult(psp_a1,psp_a0);

   emit_mflo(psp_a0);

   emit_sw(psp_a0,RCPU,_thumb_reg_pos(0));

   emit_lw(psp_at,RCPU,_flags);

   emit_ext(psp_a1,psp_a0,31,31);
   emit_ins(psp_at,psp_a1,_flag_N, _flag_N);

   emit_sltiu(psp_a1, psp_a0, 1);
   emit_ins(psp_at,psp_a1,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

   if (block_procnum == ARMCPU_ARM7)
         emit_movi(RCYC, 4);
   else
         MUL_Mxx_END(0, 1);
   
   return OPR_RESULT(OPR_CONTINUE, 5);
}


static OP_RESULT THUMB_OP_CMP_SPE(uint32_t pc, const u32 i){
   
   u32 Rn = (i&7) | ((i>>4)&8);

   return OPR_INTERPRET;

   sync_r15(i, false, 0);

   emit_lw(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU,_reg(Rn));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(3));

   emit_subu(psp_a2,psp_a0,psp_a1);

   emit_sltu(psp_t0, psp_a0, psp_a1);                                    
   emit_ins(psp_at,psp_t0,_flag_C, _flag_C);

   emit_srl(psp_t0, psp_a2, 31);
   emit_ins(psp_at, psp_t0, _flag_N, _flag_N);

   emit_slt(psp_t1, psp_a0, psp_a1);
   emit_xor(psp_t1, psp_t1, psp_t0);
   emit_ins(psp_at,psp_t1,_flag_V, _flag_V);

   emit_sltiu(psp_t0, psp_a2, 1);
   emit_ins(psp_at, psp_t0,_flag_Z, _flag_Z);

   emit_sw(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_B_COND(uint32_t pc, const u32 i)
{

   u32 adr =  4 + ((u32)((s8)(i&0xFF))<<1);

   sync_r15(i, false, 0);

   emit_addiu(psp_at, psp_gp, 2);
   emit_sw(psp_at,RCPU,_instr_adr);

   emit_branch((i>>8)&0xF, i, 8, 0);

   emit_addiu(psp_a0, psp_gp, adr);
   emit_sw(psp_a0,RCPU,_instr_adr);


   return OPR_RESULT(OPR_CONTINUE, 1);
}

#define SIGNEEXT_IMM11(i)	(((i)&0x7FF) | (BIT10(i) * 0xFFFFF800))
#define SIGNEXTEND_11(i) (((s32)i<<21)>>21)

static OP_RESULT THUMB_OP_B_UNCOND(uint32_t pc, const u32 i)
{
   emit_addiu(psp_a0, psp_gp, (4 + (SIGNEXTEND_11(i)<<1)));
   emit_sw(psp_a0,RCPU,_instr_adr);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADJUST_P_SP(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg(13));
   emit_addiu(psp_a0,psp_a0,((i&0x7F)<<2));
   emit_sw(psp_a0,RCPU,_reg(13));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADJUST_M_SP(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg(13));
   emit_subiu(psp_a0,psp_a0,((i&0x7F)<<2));
   emit_sw(psp_a0,RCPU,_reg(13));
   
   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_2PC(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 imm = ((i&0xFF)<<2);

   emit_lw(psp_a1, RCPU, _R15);

   emit_addiu(psp_a1, psp_a1, imm);

   emit_sw(psp_a1, RCPU, _thumb_reg_pos(8));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_ADD_2SP(uint32_t pc, const u32 i)
{
   u32 imm = ((i&0xFF)<<2);

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_reg(13));
   
   if (imm) emit_addiu(psp_a0,psp_a0,imm);

   emit_sw(psp_a0,RCPU,_thumb_reg_pos(8));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_BL_10(uint32_t pc, const u32 i)
{   
   sync_r15(i, false, 0);

   u32 adr = (SIGNEXTEND_11(i)<<12) + 4;

   emit_lui(psp_a0,adr>>16);
   emit_ori(psp_a0,psp_a0,adr&0xFFFF);

   emit_addu(psp_a0, psp_gp, psp_a0);

   emit_sw(psp_a0, RCPU, _reg(14));

	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_POP(uint32_t pc, const u32 i)
{
   u8 poped = 0;
   
   emit_lw(psp_k1, RCPU, _reg(13));

	for(int j = 0; j<8; j++)
		if(BIT_N(i, j))
		{

         if (block_procnum) emit_jal(_MMU_read32<1>);
         else               emit_jal(_MMU_read32<0>);  
        
         emit_move(psp_a0,psp_k1);

         emit_sw(psp_v0,RCPU,_reg(j));

         emit_addiu(psp_k1,psp_k1,4);

         poped++;
		}

	emit_sw(psp_k1, RCPU, _reg(13));

   emit_MMU_aluMemCycles(2, poped);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_PUSH(uint32_t pc, const u32 i)
{
   u8 pushed = 0;

   emit_lw(psp_k1, RCPU, _reg(13));

   emit_subiu(psp_k1,psp_k1,4);

	for(int j = 0; j<8; j++)
		if(BIT_N(i, 7-j))
		{
         
         emit_lw(psp_a1,RCPU,_reg(7 - j));

         if (block_procnum) emit_jal(_MMU_write32<1>);
         else               emit_jal(_MMU_write32<0>);

         emit_move(psp_a0,psp_k1);
   
         emit_subiu(psp_k1,psp_k1,4);

         pushed++;
		}

   emit_addiu(psp_k1,psp_k1,4);

	emit_sw(psp_k1, RCPU, _reg(13));

   emit_MMU_aluMemCycles(3, pushed);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_SWI_THUMB(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 last_op = emit_SlideDelay();

   emit_jal(_ARMPROC.swi_tab[i & 0x1F]);
   emit_Write32(last_op);
   
   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_STRB_REG_OFF(uint32_t pc, const u32 i)
{
   //u32 adr_first = _ARMPROC.R[_REG_NUM(i, 3)] + _ARMPROC.R[_REG_NUM(i, 6)];

   sync_r15(i, false, 0);

	emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a2,RCPU,_thumb_reg_pos(6));

   emit_lb(psp_a1,RCPU,_thumb_reg_pos(0));

   //EXPERIMENTAL OPTIMIZATION

   //if (classify_adr(adr_first, 0) != MEMTYPE_MAIN)
   {

      if (block_procnum) emit_jal(_MMU_write08<1>);
      else               emit_jal(_MMU_write08<0>);

      emit_addu(psp_a0,psp_a2,psp_a0);

   }/*else{

      const u32 ram_loc = ((u32)&MMU.MAIN_MEM);

      printf("FAST PATH HIT\n");

      emit_addu(psp_a0,psp_a2,psp_a0);

      emit_lui(psp_at, ram_loc>>16);
      emit_ori(psp_at, psp_at, ram_loc&0xffff);
      emit_sb(psp_a1, psp_at, psp_a0);
   }*/
   
			
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDRB_REG_OFF(uint32_t pc, const u32 i){

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(6));

   if (block_procnum) emit_jal(_MMU_read08<1>);
   else               emit_jal(_MMU_read08<0>);

   emit_addu(psp_a0,psp_a1,psp_a0);

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_STRH_REG_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a2,RCPU,_thumb_reg_pos(6));

   emit_lhu(psp_a1,RCPU,_thumb_reg_pos(0));

   if (block_procnum) emit_jal(_MMU_write16<1>);
   else               emit_jal(_MMU_write16<0>);

   emit_addu(psp_a0,psp_a2,psp_a0);
			
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDRH_REG_OFF(uint32_t pc, const u32 i){

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(6));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   emit_addu(psp_a0,psp_a1,psp_a0);

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_STRB_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

	emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   emit_lb(psp_a1, RCPU,_thumb_reg_pos(0));

   if (block_procnum) emit_jal(_MMU_write08<1>);
   else               emit_jal(_MMU_write08<0>);

   emit_addiu(psp_a0,psp_a0,((i>>6)&0x1F));
			
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDRB_IMM_OFF(uint32_t pc, const u32 i){

   sync_r15(i, false, 0);

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   if (block_procnum) emit_jal(_MMU_read08<1>);
   else               emit_jal(_MMU_read08<0>);

   emit_addiu(psp_a0,psp_a0,((i>>6)&0x1F));

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_STRH_IMM_OFF(uint32_t pc, const u32 i)
{
   sync_r15(i, false, 0);

   u32 offset = ((i>>5)&0x3E);

	emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   if ((offset) != 0) emit_lhu(psp_a1,RCPU,_thumb_reg_pos(0));

   if (block_procnum) emit_jal(_MMU_write16<1>);
   else               emit_jal(_MMU_write16<0>);

   if ((offset) != 0) emit_addiu(psp_a0,psp_a0, offset);
   else               emit_lw(psp_a1,RCPU,_thumb_reg_pos(0));
			
	return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDRH_IMM_OFF(uint32_t pc, const u32 i){

   sync_r15(i, false, 0);

   u32 offset = ((i>>5)&0x3E);

   if ((offset) != 0) emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   if (block_procnum) emit_jal(_MMU_read16<1>);
   else               emit_jal(_MMU_read16<0>);

   if ((offset) != 0) emit_addiu(psp_a0,psp_a0, offset);
   else               emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_STR_IMM_OFF(uint32_t pc, const u32 i){

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   sync_r15(i, false, 0);

   u32 offset = ((i>>4)&0x7C);

   if ((offset) != 0) 
	{ 
		emit_addiu(psp_a0,psp_a0,offset);
	}

   if (block_procnum) emit_jal(_MMU_write32<1>);
   else               emit_jal(_MMU_write32<0>);

   emit_lw(psp_a1,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_STR_REG_OFF(uint32_t pc, const u32 i){

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   sync_r15(i, false, 0);

   emit_lw(psp_at,RCPU,_thumb_reg_pos(6));

   emit_addu(psp_a0,psp_at,psp_a0);

   if (block_procnum) emit_jal(_MMU_write32<1>);
   else               emit_jal(_MMU_write32<0>);

   emit_lw(psp_a1,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDR_REG_OFF(uint32_t pc, const u32 i){

   emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));
   emit_lw(psp_a1,RCPU,_thumb_reg_pos(6));

   if (block_procnum) emit_jal(_MMU_read32<1>);
   else               emit_jal(_MMU_read32<0>);

   emit_addu(psp_a0,psp_a1,psp_a0);

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDR_IMM_OFF(uint32_t pc, const u32 i){

   sync_r15(i, false, 0);

   u32 offset = ((i>>4)&0x7C);

   if ((offset) != 0) emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   if (block_procnum) emit_jal(_MMU_read32<1>);
   else               emit_jal(_MMU_read32<0>);

   if ((offset) != 0) emit_addiu(psp_a0,psp_a0, offset);
   else               emit_lw(psp_a0,RCPU,_thumb_reg_pos(3));

   emit_sw(psp_v0,RCPU,_thumb_reg_pos(0));

   return OPR_RESULT(OPR_CONTINUE, 1);
}



static OP_RESULT THUMB_OP_BL_11(uint32_t pc, const u32 i){

   emit_lw(psp_a0,RCPU, _reg(14));

   emit_addiu(psp_a0, psp_a0, ((i&0x7FF)<<1));

   emit_addiu(psp_a1,psp_gp, 2);

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_ori(psp_a1,psp_a1, 1);

   emit_sw(psp_a1, RCPU, _reg(14));

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_BLX(uint32_t pc, const u32 i){

   if (!block_procnum) return OPR_INTERPRET;

   if(n_ops < 6) interpreted_cycles += 50; //Speed up branches

   emit_lw(psp_a0,RCPU, _reg(14));

   emit_movi(psp_a2, -4);

   emit_addiu(psp_a0, psp_a0, ((i&0x7FF)<<1));

   emit_and(psp_a0,psp_a0, psp_a2);

   emit_lw(psp_at, RCPU, _flags);
   
   emit_addiu(psp_a1,psp_gp, 2);

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_ori(psp_a1,psp_a1, 1);

   emit_sw(psp_a1, RCPU, _reg(14));

   emit_ins(psp_at, psp_zero, 5, 5);
   
   emit_sw(psp_at, RCPU, _flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_BLX_THUMB(uint32_t pc, const u32 i){

   if (!block_procnum) return OPR_INTERPRET;

   if(n_ops < 6) interpreted_cycles += 50; //Speed up branches

   emit_lbu(psp_at,RCPU,_flags);

   emit_lw(psp_a0,RCPU, _reg_pos(3));

   emit_addiu(psp_a3,psp_gp, 2);

   emit_movi(psp_a1,-4);

   emit_ori(psp_a3, psp_a3, 1);

   emit_ext(psp_a2,psp_a0,0,0);

   emit_sw(psp_a3, RCPU, _reg(14));

   emit_and(psp_a0, psp_a0, psp_a1);

   emit_ins(psp_at, psp_a2, _flag_T, _flag_T);

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_sb(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


static OP_RESULT THUMB_OP_BX_THUMB(uint32_t pc, const u32 i){

   return OPR_INTERPRET;

   emit_lbu(psp_at,RCPU,_flags);

   if (REG_POS(i, 3) == 15){

      emit_movi(psp_a1,-4);
      
      emit_and(psp_a0,psp_gp,psp_a1);

      emit_sw(psp_a0, RCPU, _instr_adr);

      emit_ins(psp_at,psp_zero,_flag_T, _flag_T);

      emit_sb(psp_at,RCPU,_flags);

      return OPR_RESULT(OPR_CONTINUE, 1);
   }

   emit_lw(psp_a0,RCPU, _reg_pos(3));

   emit_movi(psp_a1,-2);

   emit_ext(psp_a2,psp_a0,0,0);

   emit_and(psp_a0, psp_a0, psp_a1);

   emit_ins(psp_at, psp_a2, _flag_T, _flag_T);

   if(n_ops < 3) emit_addu(RCYC, RCYC, psp_a0); //Speed up branches

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_sb(psp_at,RCPU,_flags);

   return OPR_RESULT(OPR_CONTINUE, 1);
}



#define THUMB_OP_INTERPRET       0

#define THUMB_OP_ASR             THUMB_OP_INTERPRET

#define THUMB_OP_UND_THUMB       THUMB_OP_INTERPRET

#define THUMB_OP_ASR_0           THUMB_OP_INTERPRET

#define THUMB_OP_ADD_REG         THUMB_OP_INTERPRET
#define THUMB_OP_SUB_REG         THUMB_OP_INTERPRET

#define THUMB_OP_LSL_REG         THUMB_OP_INTERPRET
#define THUMB_OP_LSR_REG         THUMB_OP_INTERPRET
#define THUMB_OP_ASR_REG         THUMB_OP_INTERPRET
#define THUMB_OP_ADC_REG         THUMB_OP_INTERPRET
#define THUMB_OP_SBC_REG         THUMB_OP_INTERPRET
#define THUMB_OP_ROR_REG         THUMB_OP_INTERPRET
#define THUMB_OP_NEG             THUMB_OP_INTERPRET
#define THUMB_OP_CMP             THUMB_OP_INTERPRET

#define THUMB_OP_LDR_SPREL       THUMB_OP_INTERPRET
#define THUMB_OP_STR_SPREL       THUMB_OP_INTERPRET
#define THUMB_OP_LDR_PCREL       THUMB_OP_INTERPRET


#define THUMB_OP_LDRSB_REG_OFF   THUMB_OP_INTERPRET
#define THUMB_OP_LDRSH_REG_OFF   THUMB_OP_INTERPRET


// UNDEFINED OPS
#define THUMB_OP_PUSH_LR         THUMB_OP_INTERPRET
#define THUMB_OP_BKPT_THUMB      THUMB_OP_INTERPRET

static OP_RESULT THUMB_OP_POP_PC(uint32_t pc, const u32 i){

   u8 poped = 0;

   sync_r15(i, false, 0);
   
   emit_lw(psp_k1, RCPU, _reg(13));

	for(int j = 0; j<8; j++)
		if(BIT_N(i, j))
		{
         if (block_procnum) emit_jal(_MMU_read32<1>);
         else               emit_jal(_MMU_read32<0>);

         emit_move(psp_a0,psp_k1);

         emit_sw(psp_v0, RCPU, _reg(j));
   
         emit_addiu(psp_k1,psp_k1,4);

         poped++;
		}

   if (block_procnum) emit_jal(_MMU_read32<1>);
   else               emit_jal(_MMU_read32<0>);
   emit_move(psp_a0,psp_k1);

   if (!block_procnum){
      emit_lbu(psp_at,RCPU,_flags);
      emit_ext(psp_a1, psp_v0, 0, 0);
      emit_ins(psp_at, psp_a1, _flag_T, _flag_T);
      emit_sb(psp_at,RCPU,_flags);
   }

   emit_movi(psp_a1, -2);

   emit_and(psp_a0, psp_v0, psp_a1);

   emit_sw(psp_a0, RCPU, _instr_adr);

   emit_addiu(psp_k1,psp_k1,4);

   emit_sw(psp_k1, RCPU, _reg(13));

   emit_MMU_aluMemCycles(5, poped);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_STMIA_THUMB(uint32_t pc, const u32 i){

   u8 poped = 0;

   sync_r15(i, false, 0);
   
   emit_lw(psp_k1, RCPU, _thumb_reg_pos(8));

	for(int j = 0; j<8; j++)

		if(BIT_N(i, j))
		{
          emit_lw(psp_a1, RCPU, _reg(j));

         if (block_procnum) emit_jal(_MMU_write32<1>);
         else               emit_jal(_MMU_write32<0>);

         emit_move(psp_a0,psp_k1);

   
         emit_addiu(psp_k1,psp_k1,4);

         poped++;
		}

	emit_sw(psp_k1, RCPU, _thumb_reg_pos(8));

   emit_MMU_aluMemCycles(2, poped);

   return OPR_RESULT(OPR_CONTINUE, 1);
}

static OP_RESULT THUMB_OP_LDMIA_THUMB(uint32_t pc, const u32 i){

   u8 poped = 0;

   u32 regIndex = _REG_NUM(i, 8);

   sync_r15(i, false, 0);
   
   emit_lw(psp_k1, RCPU, _reg(regIndex));

	for(int j = 0; j<8; j++)
		if(BIT_N(i, j))
		{
         if (block_procnum) emit_jal(_MMU_read32<1>);
         else               emit_jal(_MMU_read32<0>);

         emit_move(psp_a0,psp_k1);

         emit_sw(psp_v0, RCPU, _reg(j));
   
         emit_addiu(psp_k1,psp_k1,4);

         poped++;
		}

   if (!BIT_N(i, regIndex))
	   emit_sw(psp_k1, RCPU, _reg(regIndex));

   
   emit_MMU_aluMemCycles(3, poped);

   return OPR_RESULT(OPR_CONTINUE, 1);
}


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

static const ArmOpCompiled op_decode[2][2] = { OP_DECODE<0,0>, OP_DECODE<0,1>, OP_DECODE<1,0>, OP_DECODE<1,1> };



//-----------------------------------------------------------------------------
//   Compiler
//-----------------------------------------------------------------------------

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

static void emit_prefetch(){
   const u8 isize = thumb ? 2 : 4;

   if (skip_prefeth) {
      skip_prefeth = false;
      return;
   }

   emit_addiu(psp_at, psp_gp, isize);
   emit_sw(psp_at, psp_k0, _next_instr);

   emit_addiu(psp_at, psp_at, isize);
   emit_sw(psp_at, psp_k0, _R15);
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
   if (skip_prefeth) {
      skip_prefeth = false;
      return;
   }

   if(instr_does_prefetch(opcode))
	{
		if(force)
		{
         const u8 isize = thumb ? 2 : 4;
         emit_addiu(psp_at, psp_gp,isize);
         emit_sw(psp_at, RCPU, _instr_adr);  //pc + isize, psp_k0, nextinstr offset
			///c.mov(cpu_ptr(instruct_adr), bb_next_instruction);
		}
	}
	else
	{

      if(instr_attributes(opcode) & JIT_BYPASS)
      {
         emit_sw(psp_gp, RCPU, _instr_adr);   //pc, psp_k0, nextinstr offset
         //c.mov(cpu_ptr(instruct_adr), bb_adr);
      }
      if(instr_uses_r15(opcode, thumb))
      {
         const u8 isize = thumb ? 4 : 8;
         emit_addiu(psp_at, psp_gp,isize);
         emit_sw(psp_at, RCPU, _R15);
         //c.mov(reg_ptr(15), bb_r15);
      }
		if(force || (instr_attributes(opcode) & JIT_BYPASS) || (instr_attributes(opcode) & BRANCH_SWI) || (is_last && !instr_is_branch(opcode)))
		{
         const u8 isize = thumb ? 2 : 4;
         emit_addiu(psp_at, psp_gp,isize);
         emit_sw(psp_at, RCPU, _next_instr);
			//c.mov(cpu_ptr(next_instruction), bb_next_instruction);
		}

	}
}

static u32 emit_Halfbranch(int cond,u32 opcode, bool EndBlock)
{
	static const u8 cond_bit[] = {0x40, 0x40, 0x20, 0x20, 0x80, 0x80, 0x10, 0x10};

   if (EndBlock) sync_r15(opcode, 1, 1);

	if(cond < 8)
	{
      emit_lbu(psp_at,psp_k0,_flags+3);
      emit_andi(psp_k1,psp_at, cond_bit[cond]);

      emit_nop();
      emit_nop();
      return emit_getPointAdr() - 8;
	}

   u32 label = 0;

   switch (cond){

      case 8:  
      case 9:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,6,5);
         emit_xori(psp_k1,psp_a1,0b01);

         emit_nop();
         emit_nop();
      break;

      case 10:
      case 11:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,7,7);
         emit_ext(psp_at,psp_at,4,4);

         emit_xor(psp_k1,psp_a1,psp_at);

         emit_nop();
         emit_nop();

      break;

      case 12:
      case 13:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,7,6);
         emit_ext(psp_at,psp_at,4,3);

         emit_andi(psp_at,psp_at,0b10);
         emit_xor(psp_k1,psp_a1,psp_at);

         emit_nop();
         emit_nop();
      break;

      default:
      break;
   }

   return emit_getPointAdr() - 8;
}

static void CompleteCondition(u32 cond, u32 _addr, u32 label){
   // printf("CurrAddr: %x, Pointer: %d\n",label,_addr);

   if(cond < 8)
	{
      if (cond&1)
         emit_bneC(psp_k1,psp_zero,label,_addr);
      else
         emit_beqC(psp_k1,psp_zero,label,_addr);
      return;
   }

   if (cond&1)
      emit_beqC(psp_k1,psp_zero,label,_addr);
   else
      emit_bneC(psp_k1,psp_zero,label,_addr);
}

static void emit_branch(int cond,u32 opcode, u32 sz,bool EndBlock)
{
	static const u8 cond_bit[] = {0x40, 0x40, 0x20, 0x20, 0x80, 0x80, 0x10, 0x10};

   if (EndBlock) sync_r15(opcode, 1, 1);

	if(cond < 8)
	{
      emit_lbu(psp_at,psp_k0,_flags+3);
      emit_andi(psp_a1,psp_at, cond_bit[cond]);

      u32 label = emit_getCurrAdr() + sz + 8;

      if(cond & 1)
         emit_bne(psp_a1,psp_zero,label);
      else
         emit_beq(psp_a1,psp_zero,label);

      emit_nop();

      return;
	}

   u32 label = 0;

   switch (cond){

      case 8:  
      case 9:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,6,5);
         emit_xori(psp_a1,psp_a1,0b01);

         label = emit_getCurrAdr() + sz + 8;

         if (cond == 8) emit_bne(psp_a1,psp_zero,label); //1000 HI C set and Z clear unsigned higher
         else           emit_beq(psp_a1,psp_zero,label); //1001 LS C clear or Z set unsigned lower or same
         
         emit_nop();
      break;

      case 10:
      case 11:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,7,7);
         emit_ext(psp_at,psp_at,4,4);

         emit_xor(psp_a1,psp_a1,psp_at);

         label = emit_getCurrAdr() + sz + 8;

         if (cond == 10) emit_bne(psp_a1,psp_zero,label); //GE N equals V greater or equal
         else            emit_beq(psp_a1,psp_zero,label); //LT N not equal to V less than

         emit_nop();
      break;

      case 12:
      case 13:

         emit_lbu(psp_at,psp_k0,_flags + 3);

         emit_ext(psp_a1,psp_at,7,6);
         emit_ext(psp_at,psp_at,4,3);

         emit_andi(psp_at,psp_at,0b10);
         emit_xor(psp_a1,psp_a1,psp_at);

         label = emit_getCurrAdr() + sz + 8;

         if (cond == 12) emit_bne(psp_a1,psp_zero,label); //GT Z clear AND (N equals V) greater than
         else            emit_beq(psp_a1,psp_zero,label); //LE Z set OR (N not equal to V) less than or equal

         emit_nop();
      break;

      default:
      break;
   }
}


template<int PROCNUM>
bool IsIdleLoop(bool thumb, u32 addr_instrs, int instrsCount)
{
    // see https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/PPCAnalyst.cpp#L678
    // it basically checks if one iteration of a loop depends on another
    // the rules are quite simple

   const u32 isize = thumb ? 2 : 4;

   const uint32_t imask = thumb ? 0xFFFFFFFE : 0xFFFFFFFC;

   char dasmbuf[1024] = {0};

   if (thumb) return false;

    //printf("checking potential idle loop\n");
    u16 regsWrittenTo = 0;
    u16 regsDisallowedToWrite = 0;
    for (int i = 0; i < instrsCount; i++)
    {
        u32 opcode = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc&imask) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc&imask);

        //JIT_DEBUGPRINT("instr %d %08x regs(%x %x) %x %x\n", i, instrs[i].Instr, instrs[i].Info.DstRegs, instrs[i].Info.SrcRegs, regsWrittenTo, regsDisallowedToWrite);
        if (!thumb && BIT13(opcode)){
           //printf("%s \n",des_thumb_instructions_set[opcode>>6](pc, opcode, dasmbuf)); 
            return false;
        }
        if (i < instrsCount - 1 && REG_POS(opcode,15))
            return false;

        u16 srcRegs = opcode & ~(1 << 16);
        u16 dstRegs = opcode & ~(1 << 12);

        regsDisallowedToWrite |= srcRegs & ~regsWrittenTo;
        
        if (dstRegs & regsDisallowedToWrite)
            return false;
        regsWrittenTo |= dstRegs;
    }
    //printf("match\n");
    return true;
}

template<int PROCNUM>
static u32 compile_basicblock()
{
   block_procnum = PROCNUM;

   //while (((u32)emit_GetPtr()) & 63) emit_Skip(1); //align the instruction cache

   void* code_ptr = emit_GetPtr();

   thumb = ARMPROC.CPSR.bits.T == 1;
   const u32 base = ARMPROC.instruct_adr;
   const u32 isize = thumb ? 2 : 4;

   const uint32_t imask = thumb ? 0xFFFFFFFE : 0xFFFFFFFC;
   char dasmbuf[1024] = {0};

   uint32_t opcode = 0;

   interpreted_cycles = 0;
   int interpreted_op = 0;

   bool interpreted = true;

   pc = base;

   //printf("%x THUMB: %d  %x\n",(u32)code_ptr, thumb, base);

   emit_mpush(4,
				reg_gpr+psp_gp,
				reg_gpr+psp_k0,
				reg_gpr+psp_s0,
				reg_gpr+psp_ra);

   emit_lui(psp_k0,((u32)&ARMPROC)>>16);
	emit_ori(psp_k0,psp_k0,((u32)&ARMPROC)&0xFFFF);

   emit_move(RCYC, psp_zero);

   n_ops = 0;

   //StartCodeDump();

   u32 old_opcode = 0;

   //printf("\n\nStart\n\n");
   for (uint32_t i = 0, has_ended = 0; has_ended == 0; i ++, pc += isize, n_ops++){

      opcode = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc&imask) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc&imask);
      has_ended = instr_is_branch(opcode) || (i >= (my_config.DynarecBlockSize - 1));

      if (interpreted){
         emit_lui(psp_gp, pc >> 16);
         emit_ori(psp_gp, psp_gp, pc &0xffff);
      }else
         emit_addiu(psp_gp, psp_gp, isize);
      
      if (thumb){

         ArmOpCompiler fc = thumb_instruction_compilers[opcode>>6];
               

         if (fc){ 
            int result = fc(pc,opcode);

            if (result != OPR_INTERPRET){
               interpreted = false;
               interpreted_cycles += op_decode[PROCNUM][true]();
               continue;
            }
         }
         
         interpreted = true;

         emit_prefetch(); 

        // printf("%s \n",des_thumb_instructions_set[opcode>>6](pc, opcode, dasmbuf)); 

         emit_jal(thumb_instructions_set[PROCNUM][opcode>>6]);

         if (opcode == 0) emit_move(psp_a0,psp_zero); 
         else if (opcode != 0) emit_movi(psp_a0,opcode&0xFFFF); 
      }
      else{ 

         const bool conditional = instr_is_conditional(opcode);
         u32 addr_cond = 0;

         /*skip_load = bblock_optmizer[i].skipL;
         skip_save = bblock_optmizer[i].skipS;

         if (skip_load || skip_save){
            //printf("HJhjhjhjjhjhjhj\n");
         }*/

         if (conditional)
            addr_cond = emit_Halfbranch(CONDITION(opcode),opcode,has_ended);

            u32 Istart_adr = emit_getPointAdr();

            ArmOpCompiler fc = arm_instruction_compilers[INSTRUCTION_INDEX(opcode)];

            if (fc){ 
               int result = fc(pc,opcode);

               if (result != OPR_INTERPRET){
                  u32 skip_sz = (emit_getPointAdr() - Istart_adr) + 8;
                  
                  if (conditional) CompleteCondition(CONDITION(opcode),addr_cond,emit_getCurrAdr() + skip_sz);

                  interpreted = false;
                  interpreted_cycles += op_decode[PROCNUM][false]();
                  continue;
               }
            }

         if (conditional){

            u32 skip_sz     =  28;                        //Jump + Prefetch + bne/beq
                skip_sz    +=  ((opcode == 0) ? 4 : 8);   // Opcode

            CompleteCondition(CONDITION(opcode),addr_cond,emit_getCurrAdr() + skip_sz);
         }

         emit_prefetch();

         if (opcode != 0) emit_lui(psp_at,opcode>>16);

         interpreted_op++;

            /* printf(des_arm_instructions_set[INSTRUCTION_INDEX(opcode)](pc, opcode, dasmbuf));*/

         emit_jal(arm_instructions_set[PROCNUM][INSTRUCTION_INDEX(opcode)]);
         if (opcode != 0) { emit_ori(psp_a0,psp_at,opcode&0xFFFF); } else emit_move(psp_a0,psp_zero);

         interpreted = true;
      }

      interpreted_cycles += op_decode[PROCNUM][thumb]();
      old_opcode = opcode;
   }

  
   /*   if (!thumb && BRANCH_POS12 & opcode && REG_POS(opcode,12) == 15) {interpreted_cycles += interpreted_cycles*1.5;}
   else if (!thumb && BRANCH_LDM & opcode && BIT15(opcode))   {interpreted_cycles += interpreted_cycles*2.2;}
   else if (thumb && (opcode & BRANCH_POS0) && ((opcode&7) | ((opcode>>4)&8)) == 15) {interpreted_cycles += interpreted_cycles*1.5;}
   else if (JIT_BYPASS & opcode) {interpreted_cycles += interpreted_cycles*4;}
   else if (BRANCH_SWI & opcode) {interpreted_cycles += interpreted_cycles*2;}
   else if (BRANCH_ALWAYS & opcode) {interpreted_cycles += interpreted_cycles*2;}*/

   //if (PROCNUM && IsIdleLoop<PROCNUM>(thumb,base,n_ops) ) interpreted_cycles += interpreted_cycles*10; //Underclock arm7

   if(interpreted || !instr_does_prefetch(opcode))
	{
      emit_lw(psp_at, RCPU, _next_instr);
      emit_sw(psp_at, RCPU, _instr_adr);
   }

   //while (((u32)emit_GetPtr()) & 15) emit_nop();

   emit_addiu(psp_v0, RCYC, interpreted_cycles);

   emit_mpop(4,
				reg_gpr+psp_gp,
				reg_gpr+psp_k0,
				reg_gpr+psp_s0,
				reg_gpr+psp_ra);
   
   emit_jra();
   emit_nop();
   //emit_movi(psp_v0,interpreted_cycles);

   //if (n_ops > 25) CodeDump("dump.bin");

   //printf("THUMB %d,  %d\n",thumb,interpreted_op);
   
   make_address_range_executable((u32)code_ptr, (u32)emit_GetPtr());
   JIT_COMPILED_FUNC(base, PROCNUM) = (uintptr_t)code_ptr;

   return interpreted_cycles;
}

u8 OpcodesOptimized(u16 op){
   switch(op){
      case 0x080 ... 0x088: return true;
      case 0x090 ... 0x098: return true;
   }

   return false;
}

template<int PROCNUM>
void build_basicblock(){

   const u32 isize = thumb ? 2 : 4;

   char dasmbuf[1024] = {0};

   const uint32_t imask = thumb ? 0xFFFFFFFE : 0xFFFFFFFC;
   uint32_t op1 = 0, op2 = 0;

   thumb = ARMPROC.CPSR.bits.T == 1;
   const u32 base = ARMPROC.instruct_adr;

   if (thumb) return;

   bblock_optmizer.clear();

   pc = base;

   bblock_optmizer.push_back({false, false});

   u32 old_op = 0;
   
   for (uint32_t i = 0, has_ended = 0; has_ended == 0; i ++, pc += isize){
      op1 = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc&imask) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc&imask);
      
      has_ended = instr_is_branch(op1) || (i >= (my_config.DynarecBlockSize - 1));

      if (!OpcodesOptimized(INSTRUCTION_INDEX(op1))){
         bblock_optmizer.push_back({false, false});
         continue;
      }

      if (has_ended) break;

      op2 = thumb ? _MMU_read16<PROCNUM, MMU_AT_CODE>(pc+isize&imask) : _MMU_read32<PROCNUM, MMU_AT_CODE>(pc+isize&imask);

      if (!OpcodesOptimized(INSTRUCTION_INDEX(op2))) continue;

      /*printf("%s  %s\n",des_arm_instructions_set[INSTRUCTION_INDEX(op1)](pc, op1, dasmbuf), des_arm_instructions_set[INSTRUCTION_INDEX(op2)](pc, op2, dasmbuf));
      printf("%x\n",op1);*/
      bblock_optmizer.push_back({false/*REG_POS(old_op,12) == REG_POS(op1,16)*/, REG_POS(op2,16) == REG_POS(op1,12)});

      old_op = op1;
      //if (op2 == op1) printf("%s  %s\n",des_arm_instructions_set[INSTRUCTION_INDEX(op1)](pc, op1, dasmbuf), des_arm_instructions_set[INSTRUCTION_INDEX(op2)](pc, op2, dasmbuf));
   }

   bblock_optmizer.push_back({false, false});
}

template<int PROCNUM> u32 arm_jit_compile()
{
   if (GetFreeSpace() < 16 * 1024){
      //printf("Dynarec code reset\n");
      arm_jit_reset(true,true);
   }

   //build_basicblock<PROCNUM>();

   return compile_basicblock<PROCNUM>();
}

template u32 arm_jit_compile<0>();
template u32 arm_jit_compile<1>();

void arm_jit_reset(bool enable, bool suppress_msg)
{
   if (!suppress_msg)
	   printf("CPU mode: %s\n", enable?"JIT":"Interpreter");

   saveBlockSizeJIT = my_config.DynarecBlockSize; //CommonSettings.jit_max_block_size;

   if (enable)
   {
      //printf("JIT: max block size %d instruction(s)\n", CommonSettings.jit_max_block_size);

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

      //memset(recompile_counts, 0, sizeof(recompile_counts));
      init_jit_mem();

     resetCodeCache();
   }
}

void arm_jit_close()
{
   resetCodeCache();
}