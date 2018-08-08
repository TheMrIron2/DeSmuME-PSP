/*  Copyright (C) 2006 Guillaume Duhamel

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

#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char FS_SEPARATOR = '/';

static SceIoDirent dir;

void * FsReadFirst(const char * p, FsEntry * entry) {

	void** ret;
	SceUID fd = sceIoDopen( p );
	if(fd < 0) {
		return NULL;
	}

	memset( &dir, 0, sizeof(SceIoDirent) );
	if(sceIoDread( fd, &dir ) <= 0) {
		sceIoDclose( fd );
		return NULL;
	}

	strncpy(entry->cFileName, dir.d_name,256);
	entry->cFileName[255] = 0 ;
	strncpy(entry->cAlternateFileName, dir.d_name,14);
	entry->cAlternateFileName[14] = 0 ;
	entry->flags = 0;

	if(FIO_S_ISDIR( dir.d_stat.st_mode )) {
		entry->flags = FS_IS_DIR;
		entry->fileSize = 0;
	} else {
        entry->fileSize = dir.d_stat.st_size;
	}

	ret = (void**)malloc(sizeof(SceUID));
	*ret = (void*)fd;
	return ret;
}

int FsReadNext(void * search, FsEntry * entry) {

	SceUID fd = *(SceUID*)search;

	memset( &dir, 0, sizeof(SceIoDirent) );
	if(sceIoDread( fd, &dir ) <= 0) {
		return 0;	/* No more files or error */
	}

	if(FIO_S_ISDIR( dir.d_stat.st_mode )) {
		entry->flags = FS_IS_DIR;
		entry->fileSize = 0;
	} else {
        entry->fileSize = dir.d_stat.st_size;
	}

	return 1;
}

void FsClose(void * search) {
	SceUID fd = *(SceUID*)search;

	sceIoDclose( fd );
}

int FsError(void) {
	return FS_ERR_NO_MORE_FILES;
	//return FS_ERR_UNKNOWN;
}
