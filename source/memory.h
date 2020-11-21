/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MEMORY_H
#define MEMORY_H


typedef enum
{
  REG_DISPCNT  = 0x000,
  REG_DISPSTAT = 0x004 / 2,
  REG_VCOUNT   = 0x006 / 2,
  REG_BG0CNT   = 0x008 / 2,
  REG_BG1CNT   = 0x00A / 2,
  REG_BG2CNT   = 0x00C / 2,
  REG_BG3CNT   = 0x00E / 2,
  REG_BG0HOFS  = 0x010 / 2,
  REG_BG0VOFS  = 0x012 / 2,
  REG_BG1HOFS  = 0x014 / 2,
  REG_BG1VOFS  = 0x016 / 2,
  REG_BG2HOFS  = 0x018 / 2,
  REG_BG2VOFS  = 0x01A / 2,
  REG_BG3HOFS  = 0x01C / 2,
  REG_BG3VOFS  = 0x01E / 2,
  REG_BG2PA    = 0x020 / 2,
  REG_BG2PB    = 0x022 / 2,
  REG_BG2PC    = 0x024 / 2,
  REG_BG2PD    = 0x026 / 2,
  REG_BG2X_L   = 0x028 / 2,
  REG_BG2X_H   = 0x02A / 2,
  REG_BG2Y_L   = 0x02C / 2,
  REG_BG2Y_H   = 0x02E / 2,
  REG_BG3PA    = 0x030 / 2,
  REG_BG3PB    = 0x032 / 2,
  REG_BG3PC    = 0x034 / 2,
  REG_BG3PD    = 0x036 / 2,
  REG_BG3X_L   = 0x038 / 2,
  REG_BG3X_H   = 0x03A / 2,
  REG_BG3Y_L   = 0x03C / 2,
  REG_BG3Y_H   = 0x03E / 2,
  REG_WIN0H    = 0x040 / 2,
  REG_WIN1H    = 0x042 / 2,
  REG_WIN0V    = 0x044 / 2,
  REG_WIN1V    = 0x046 / 2,
  REG_WININ    = 0x048 / 2,
  REG_WINOUT   = 0x04A / 2,
  REG_BLDCNT   = 0x050 / 2,
  REG_BLDALPHA = 0x052 / 2,
  REG_BLDY     = 0x054 / 2,
  REG_TM0D     = 0x100 / 2,
  REG_TM0CNT   = 0x102 / 2,
  REG_TM1D     = 0x104 / 2,
  REG_TM1CNT   = 0x106 / 2,
  REG_TM2D     = 0x108 / 2,
  REG_TM2CNT   = 0x10A / 2,
  REG_TM3D     = 0x10C / 2,
  REG_TM3CNT   = 0x10E / 2,
  REG_P1       = 0x130 / 2,
  REG_P1CNT    = 0x132 / 2,
  REG_RCNT     = 0x134 / 2,
  REG_IE       = 0x200 / 2,
  REG_IF       = 0x202 / 2,
  REG_IME      = 0x208 / 2,
  REG_HALTCNT  = 0x300 / 2
} HARDWARE_REGISTER;


//extern u16 io_registers[1024 / 2];

extern u32 reg[64];


#endif /* MEMORY_H */
