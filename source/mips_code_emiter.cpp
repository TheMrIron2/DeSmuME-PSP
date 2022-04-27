#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "mips_code_emiter.h"

//THE EMITER IS TAKEN FROM NULLDC PSP
//CODE FROM Hlide AND Skmpt

#define CODE_SIZE   (4*1024*1024)

u32 LastAddr = 0;

u8 __attribute__((aligned(64)))CodeCache[CODE_SIZE];

void* emit_GetPtr()      { return (void*)&CodeCache[LastAddr];                     }
u32   emit_getCurrAdr()  { return (u32)&CodeCache[LastAddr];   					   }
u32   emit_getPointAdr() { return LastAddr;                                        }
u32   GetFreeSpace()     { return CODE_SIZE - LastAddr;                            }
void  emit_Skip(u32 sz)  { LastAddr+=sz;                                           }
u32   emit_SlideDelay()  { emit_Skip(-4); u32 rv=*(u32*)emit_GetPtr();  return rv; }

//1+n opcodes
void emit_mpush(u32 n, ...)
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
void emit_mpop(u32 n, ...)
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

void emit_Write32(u32 data)
{
	*(u32*)&CodeCache[LastAddr]=data;
	LastAddr+=4;
}

void insert_instruction(psp_insn_t insn)
{
	emit_Write32(insn.word);
}

void emit_Write32Pos(u32 data, u32 pos)
{
	*(u32*)&CodeCache[pos]=data;
}

void insert_instruction2(psp_insn_t insn,u32 pos)
{
	emit_Write32Pos(insn.word,pos);
}

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

void resetCodeCache(){
    memset(CodeCache, 0, CODE_SIZE);
    LastAddr = 0;
}

uint32_t startAddr = 0;

void CodeDump(const char * filename){
	FILE*f=fopen(filename,"wb");
	fwrite(&CodeCache[startAddr],LastAddr-startAddr,1,f);
	fclose(f);
}

void StartCodeDump(){
	startAddr = LastAddr;
}