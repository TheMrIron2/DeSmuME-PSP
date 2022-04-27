#ifndef MIPS_CODE_EMITER
#define MIPS_CODE_EMITER

#include "types.h"
#include "PSP/emit/psp_emit.h"

//THE EMITER IS TAKEN FROM NULLDC PSP
//CODE FROM Hlide AND Skmpt

static void __debugbreak() { fflush(stdout); *(int*)0=1;}
#define dbgbreak {__debugbreak(); for(;;);}
#define _T(x) x
#define die(reason) { printf("Fatal error : %x %d\n in %s -> %s : %d \n",_T(reason),_T(__FUNCTION__),_T(__FILE__),__LINE__); dbgbreak;}

inline bool is_s8(u32 v) { return (s8)v==(s32)v; }
inline bool is_u8(u32 v) { return (u8)v==(s32)v; }
inline bool is_s16(u32 v) { return (s16)v==(s32)v; }
inline bool is_u16(u32 v) { return (u16)v==(u32)v; }

void emit_mpush(u32 n, ...);
void emit_mpop(u32 n, ...);

void CodeDump(const char * filename);
void StartCodeDump();

void  emit_Skip(u32 sz);

void emit_Write32(u32 data);
void insert_instruction(psp_insn_t insn);
void insert_instruction2(psp_insn_t insn,u32 pos);
u32  emit_SlideDelay();

void make_address_range_executable(u32 address_start, u32 address_end);

void resetCodeCache();

void* emit_GetPtr();
u32   emit_getPointAdr();
u32   emit_getCurrAdr();
u32   GetFreeSpace();

#endif