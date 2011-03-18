//
//  internal.h
//  KFS
//
//  Copyright (c) 2011, FadingRed LLC
//  All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
//  following conditions are met:
//  
//    - Redistributions of source code must retain the above copyright notice, this list of conditions and the
//      following disclaimer.
//    - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
//    - Neither the name of the FadingRed LLC nor the names of its contributors may be used to endorse or promote
//      products derived from this software without specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef _KFSINTERNAL_H_
#define _KFSINTERNAL_H_

#include "kfslib.h"

struct kfscontents {
	const char **entries;
	uint64_t capacity;
	uint64_t count;
};

/*!
 \brief		Puts a filesystem
 \details	Puts a filesystem in the table. Returns the identifier
			used for the filesystem.
 */
kfsid_t kfstable_put(const kfsfilesystem_t *filesystem);

/*!
 \brief		Deletes a filesystem
 \details	Deletes a filesystem from the table.
 */
void kfstable_remove(kfsid_t identifier);

/*!
 \brief		Get a filesystem
 \details	Gets a filesystem from the table. It is guarenteed to have
			each of the filesystem functions set.
 */
const kfsfilesystem_t *kfstable_get(kfsid_t identifier);

/*!
 \brief		Iterates identifiers
 \details	Start by passing in a pointer to an identifier (initialized to 0).
			Continue to pass the same pointer, and this will iterate the
			identifiers in the table. While there are still entries in the table
			this will continue to iterate, so if you only want to iterate once,
			you'll need to check for when you get back the same identifier twice.
			This will return true if an identifier was returned, and false if
			there were no more identifiers in the table.
 */
bool kfstable_iterate(kfsid_t *identifier);

#define READ_MAX_LEN	0x10000		/* 64K */
#define WRITE_MAX_LEN	0x10000		/* 64K */
#define DIR_MAX_LEN		0x1000		/* 4096 */

#endif
