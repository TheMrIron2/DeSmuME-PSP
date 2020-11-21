/*  
	Copyright (C) 2006-2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "matrix.h"
#include "MMU.h"
#include "pspmath.h"
#include "PSP/pspvfpu.h" 
 
void _NOSSE_MatrixMultVec4x4(const float* matrix, float* vecPtr)
{
/*
	float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];
	float w = vecPtr[3];
	
	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[8] + w * matrix[12];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[9] + w * matrix[13];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + w * matrix[14];
	vecPtr[3] = x * matrix[3] + y * matrix[7] + z * matrix[11] + w * matrix[15];

	*/
	
	    __asm__ volatile (
		"lv.q C100,  0 + %2\n"
		"lv.q C110, 16 + %2\n"
		"lv.q C120, 32 + %2\n"
		"lv.q C130, 48 + %2\n"
		"lv.q C200,  0 + %1\n"
		"vtfm4.q C000, E100, C200\n"
		"sv.q C000, %0\n"
		: "=m"(*vecPtr) : "m"(*vecPtr), "m"(*matrix)
		);
		

}


//-------------------------
//switched SSE functions: implementations for no SSE
#ifndef ENABLE_SSE
void MatrixMultVec4x4 (const float *matrix, float *vecPtr)
{
	_NOSSE_MatrixMultVec4x4(matrix, vecPtr);
}


void MatrixMultVec3x3 (const float *matrix, float *vecPtr)
{
	/*float x = vecPtr[0];
	float y = vecPtr[1];
	float z = vecPtr[2];

	vecPtr[0] = x * matrix[0] + y * matrix[4] + z * matrix[ 8];
	vecPtr[1] = x * matrix[1] + y * matrix[5] + z * matrix[ 9];
	vecPtr[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];*/

	__asm__ volatile (
		"lv.q C100,  0 + %2\n"
		"lv.q C110, 16 + %2\n"
		"lv.q C120, 32 + %2\n"
		"lv.q C200,  0 + %1\n"
		"vtfm4.q C000, E100, C200\n"
		"sv.q C000, %0\n"
		: "=m"(*vecPtr) : "m"(*vecPtr), "m"(*matrix)
		);

}

void MatrixMultiply (float *matrix, const float *rightMatrix)
{
	__asm__
		(
			"ulv.q C000,  0 + %1\n"
			"ulv.q C010, 16 + %1\n"
			"ulv.q C020, 32 + %1\n"
			"ulv.q C030, 48 + %1\n"

			"ulv.q C100,  0 + %2\n"
			"ulv.q C110, 16 + %2\n"
			"ulv.q C120, 32 + %2\n"
			"ulv.q C130, 48 + %2\n"

			"vmmul.q M200, M000, M100\n"

			"usv.q C200,  0 + %0\n"
			"usv.q C210, 16 + %0\n"
			"usv.q C220, 32 + %0\n"
			"usv.q C230, 48 + %0\n"
			: "=m"(*matrix) : "m"(*matrix), "m"(*rightMatrix) : "memory");
}
//Note to use it: needs (1/value) to work
void MatrixDivide4X4(float* matrix, float div)
{
	__asm__ volatile(
		".set			push\n"					// save assember option
		".set			noreorder\n"			// suppress reordering
		"mfc1			$8,   %1\n"				
		"mtv			$8,   s200\n"			
		"lv.q			c100,  0 + %0\n"		
		"lv.q			c110, 16 + %0\n"		
		"lv.q			c120, 32 + %0\n"		
		"lv.q			c130, 48 + %0\n"		
		"vmscl.q		e000, e100, s200\n"		
		"sv.q			c000,  0 + %0\n"		
		"sv.q			c010, 16 + %0\n"		
		"sv.q			c020, 32 + %0\n"		
		"sv.q			c030, 48 + %0\n"		
		".set			pop\n"					// restore assember option
		: "+m"(*matrix)
		: "f"(div)
		);
}

void MatrixDivide3X3(float* matrix, float div)
{
	__asm__ volatile(
		".set			push\n"					// save assember option
		".set			noreorder\n"			// suppress reordering
		"mfc1			$8,   %1\n"				
		"mtv			$8,   s200\n"			
		"lv.q			c100,  0 + %0\n"		
		"lv.q			c110, 16 + %0\n"		
		"lv.q			c120, 32 + %0\n"		
		"lv.q			c130, 48 + %0\n"		
		"vmscl.t		e000, e100, s200\n"		
		"sv.q			c000,  0 + %0\n"		
		"sv.q			c010, 16 + %0\n"		
		"sv.q			c020, 32 + %0\n"		
		"sv.q			c030, 48 + %0\n"		
		".set			pop\n"					// restore assember option
		: "+m"(*matrix)
		: "f"(div)
		);
}

void MatrixTranslate	(float *matrix, const float *ptr)
{
	__asm__ volatile (
		"ulv.q C030, %1\n"

		"ulv.q C100,  0 + %0\n"
		"ulv.q C110, 16 + %0\n"
		"ulv.q C120, 32 + %0\n"
		"ulv.q C130, 48 + %0\n"

		"vscl.q	C000, C100, S030\n"
		"vscl.q	C010, C110, S031\n"
		"vscl.q	C020, C120, S032\n"

		"vadd.q	C130, C130, C000\n"
		"vadd.q	C130, C130, C010\n"
		"vadd.q	C130, C130, C020\n"

		"usv.q C130, 48 + %0\n"	// only C130 has changed
		: "+m"(*matrix) : "m"(*ptr));
}



void MatrixScale (float *matrix, const float *ptr)
{
	__asm__ volatile (
		"ulv.q C100,  0 + %0\n"
		"ulv.q C110, 16 + %0\n"
		"ulv.q C120, 32 + %0\n"
		"ulv.q C130, 48 + %0\n"

		"ulv.q C000, %1\n"

		"vscl.t C100, C100, S000\n"
		"vscl.t C110, C110, S001\n"
		"vscl.t C120, C120, S002\n"

		"usv.q C100,  0 + %0\n"
		"usv.q C110, 16 + %0\n"
		"usv.q C120, 32 + %0\n"
		"usv.q C130, 48 + %0\n"
		: "+m"(*matrix) : "m"(*ptr));
}

#endif //switched c/asm functions
//-----------------------------------------

void MatrixInit  (float *matrix)
{
	memset (matrix, 0, sizeof(float)*16);
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}

void MatrixTranspose(float *matrix)
{
	float temp;
#define swap(A,B) temp = matrix[A];matrix[A] = matrix[B]; matrix[B] = temp;
	swap(1,4);
	swap(2,8);
	swap(3,0xC);
	swap(6,9);
	swap(7,0xD);
	swap(0xB,0xE);
#undef swap
}

void MatrixIdentity	(float *matrix)
{
	__asm__ volatile (
		"vmidt.q	M000\n"
		"sv.q		C000, 0  + %0\n"
		"sv.q		C010, 16 + %0\n"
		"sv.q		C020, 32 + %0\n"
		"sv.q		C030, 48 + %0\n"
		:"=m"(*matrix));
}

float MatrixGetMultipliedIndex (int index, float *matrix, float *rightMatrix)
{
	int iMod = index%4, iDiv = (index>>2)<<2;

	return	(matrix[iMod  ]*rightMatrix[iDiv  ])+(matrix[iMod+ 4]*rightMatrix[iDiv+1])+
			(matrix[iMod+8]*rightMatrix[iDiv+2])+(matrix[iMod+12]*rightMatrix[iDiv+3]);
}

void MatrixSet (float *matrix, int x, int y, float value)	// TODO
{
	matrix [x+(y<<2)] = value;
}

void MatrixCopy (float* matrixDST, const float* matrixSRC)
{
	matrixDST[0] = matrixSRC[0];
	matrixDST[1] = matrixSRC[1];
	matrixDST[2] = matrixSRC[2];
	matrixDST[3] = matrixSRC[3];
	matrixDST[4] = matrixSRC[4];
	matrixDST[5] = matrixSRC[5];
	matrixDST[6] = matrixSRC[6];
	matrixDST[7] = matrixSRC[7];
	matrixDST[8] = matrixSRC[8];
	matrixDST[9] = matrixSRC[9];
	matrixDST[10] = matrixSRC[10];
	matrixDST[11] = matrixSRC[11];
	matrixDST[12] = matrixSRC[12];
	matrixDST[13] = matrixSRC[13];
	matrixDST[14] = matrixSRC[14];
	matrixDST[15] = matrixSRC[15];

}

int MatrixCompare (const float* matrixDST, const float* matrixSRC)
{
	return memcmp((void*)matrixDST, matrixSRC, sizeof(float)*16);
}

void MatrixStackInit(MatrixStack *stack)
{
	for (int i = 0; i < stack->size; i++)
	{
		MatrixInit(&stack->matrix[i*16]);
	}
	stack->position = 0;
}

void MatrixStackSetMaxSize (MatrixStack *stack, int size)
{
	int i;

	stack->size = (size + 1);

	if (stack->matrix != NULL) {
		free (stack->matrix);
	}
	stack->matrix = (float*) malloc (stack->size*16*sizeof(float));

	for (i = 0; i < stack->size; i++)
	{
		MatrixInit (&stack->matrix[i*16]);
	}

	stack->size--;
}


MatrixStack::MatrixStack(int size, int type)
{
	MatrixStackSetMaxSize(this,size);
	this->type = type;
}

static void MatrixStackSetStackPosition (MatrixStack *stack, int pos)
{
	stack->position += pos;

	if((stack->position < 0) || (stack->position > stack->size))
		MMU_new.gxstat.se = 1;
	stack->position &= stack->size;
}

void MatrixStackPushMatrix (MatrixStack *stack, const float *ptr)
{
	//printf("Push %i pos %i\n", stack->type, stack->position);
	if ((stack->type == 0) || (stack->type == 3))
		MatrixCopy (&stack->matrix[0], ptr);
	else
		MatrixCopy (&stack->matrix[stack->position*16], ptr);
	MatrixStackSetStackPosition (stack, 1);
}

void MatrixStackPopMatrix (float *mtxCurr, MatrixStack *stack, int size)
{
	//printf("Pop %i pos %i (change %d)\n", stack->type, stack->position, -size);
	MatrixStackSetStackPosition(stack, -size);
	if ((stack->type == 0) || (stack->type == 3))
		MatrixCopy (mtxCurr, &stack->matrix[0]);
	else
		MatrixCopy (mtxCurr, &stack->matrix[stack->position*16]);
}

float * MatrixStackGetPos (MatrixStack *stack, int pos)
{
	//assert(pos<31);
	return &stack->matrix[pos*16];
}

float * MatrixStackGet (MatrixStack *stack)
{
	return &stack->matrix[stack->position*16];
}

void MatrixStackLoadMatrix (MatrixStack *stack, int pos, const float *ptr)
{
	//assert(pos<31);
	MatrixCopy (&stack->matrix[pos*16], ptr);
}

void Vector2Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}

void Vector2Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
}

void Vector2Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
}

float Vector2Dot(const float *a, const float *b)
{
	return (a[0]*b[0]) + (a[1]*b[1]);
}

/* http://www.gamedev.net/community/forums/topic.asp?topic_id=289972 */
float Vector2Cross(const float *a, const float *b)
{
	return (a[0]*b[1]) - (a[1]*b[0]);
}

float Vector3Dot(const float *a, const float *b) 
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void Vector3Cross(float* dst, const float *a, const float *b) 
{
	dst[0] = a[1]*b[2] - a[2]*b[1];
	dst[1] = a[2]*b[0] - a[0]*b[2];
	dst[2] = a[0]*b[1] - a[1]*b[0];
}


float Vector3Length(const float *a)
{
	float lengthSquared = Vector3Dot(a,a);
	float length = sqrt(lengthSquared);
	return length;
}

void Vector3Add(float *dst, const float *src)
{
	dst[0] += src[0];
	dst[1] += src[1];
	dst[2] += src[2];
}

void Vector3Subtract(float *dst, const float *src)
{
	dst[0] -= src[0];
	dst[1] -= src[1];
	dst[2] -= src[2];
}

void Vector3Scale(float *dst, const float scale)
{
	dst[0] *= scale;
	dst[1] *= scale;
	dst[2] *= scale;
}

void Vector3Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

void Vector3Normalize(float *dst)
{
	float length = Vector3Length(dst);
	Vector3Scale(dst,1.0f/length);
}

void Vector4Copy(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}