//
//  table.c
//  KFS
//
//  Copyright (c) 2012, FadingRed LLC
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

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "internal.h"

#define MAX_FIELSYSTEMS 1024

#pragma mark -
#pragma mark default implementation
// ----------------------------------------------------------------------------------------------------
// default implementation
// ----------------------------------------------------------------------------------------------------

static bool noimp(void);
static bool noimp(void) { return false; }


#pragma mark -
#pragma mark filesystem table
// ----------------------------------------------------------------------------------------------------
// filesystem table
// ----------------------------------------------------------------------------------------------------

static kfsid_t kfstable_newidentifier_nolock(void);
static kfsid_t kfstable_put_nolock(const kfsfilesystem_t *filesystem);
static void kfstable_remove_nolock(kfsid_t identifier);
static const kfsfilesystem_t *kfstable_get_nolock(kfsid_t identifier);
static bool kfstable_iterate_nolock(kfsid_t *identifier);

static kfsfilesystem_t *kfsfilesystem_duplicate(const kfsfilesystem_t *filesystem);
static void kfsfilesystem_free(kfsfilesystem_t *filesystem);

static kfsfilesystem_t *table[MAX_FIELSYSTEMS];
static pthread_mutex_t tablelock;

static kfsid_t kfstable_newidentifier_nolock(void) {
	static kfsid_t number = 0;
	kfsid_t start = number;
	kfsid_t result = -1;
	bool found = false;
	while (!found) {
		if (!table[number]) {
			result = number;
			found = true;
		}

		number = (number + 1) % MAX_FIELSYSTEMS;

		// check if we ran out of identifiers
		if (!found && (number == start)) {
			errno = EKFS_EMFS;
			break;
		}
		
		if (found) { break; }
	}
	
	return result;
}


kfsid_t kfstable_put(const kfsfilesystem_t *filesystem) {
	pthread_mutex_lock(&tablelock);
	kfsid_t result = kfstable_put_nolock(filesystem);
	pthread_mutex_unlock(&tablelock);
	return result;
}

static kfsid_t kfstable_put_nolock(const kfsfilesystem_t *filesystem) {
	kfsid_t identifier = kfstable_newidentifier_nolock();
	if (identifier >= 0) {
		if (kfstable_get_nolock(identifier) == NULL) {
			table[identifier] = kfsfilesystem_duplicate(filesystem);
			if (!table[identifier]->statfs) { table[identifier]->statfs = (void *)noimp; }
			if (!table[identifier]->stat) { table[identifier]->stat = (void *)noimp; }
			if (!table[identifier]->read) { table[identifier]->read = (void *)noimp; }
			if (!table[identifier]->write) { table[identifier]->write = (void *)noimp; }
			if (!table[identifier]->symlink) { table[identifier]->symlink = (void *)noimp; }
			if (!table[identifier]->readlink) { table[identifier]->readlink = (void *)noimp; }
			if (!table[identifier]->create) { table[identifier]->create = (void *)noimp; }
			if (!table[identifier]->remove) { table[identifier]->remove = (void *)noimp; }
			if (!table[identifier]->rename) { table[identifier]->rename = (void *)noimp; }
			if (!table[identifier]->truncate) { table[identifier]->truncate = (void *)noimp; }
			if (!table[identifier]->chmod) { table[identifier]->chmod = (void *)noimp; }
			if (!table[identifier]->utimes) { table[identifier]->utimes = (void *)noimp; }
			if (!table[identifier]->mkdir) { table[identifier]->mkdir = (void *)noimp; }
			if (!table[identifier]->rmdir) { table[identifier]->rmdir = (void *)noimp; }
			if (!table[identifier]->readdir) { table[identifier]->readdir = (void *)noimp; }
		} else {
			errno = EKFS_INTR;
			identifier = -1;
		}
	}
	
	return identifier;
}

void kfstable_remove(kfsid_t identifier) {
	pthread_mutex_lock(&tablelock);
	kfstable_remove_nolock(identifier);
	pthread_mutex_unlock(&tablelock);
}

static void kfstable_remove_nolock(kfsid_t identifier) {
	if (table[identifier]) {
		kfsfilesystem_free(table[identifier]);
		table[identifier] = NULL;
	}	
}

const kfsfilesystem_t *kfstable_get(kfsid_t identifier) {
	pthread_mutex_lock(&tablelock);
	const kfsfilesystem_t *result = kfstable_get_nolock(identifier);
	pthread_mutex_unlock(&tablelock);
	return result;
}

static const kfsfilesystem_t *kfstable_get_nolock(kfsid_t identifier) {
	return table[identifier];
}

bool kfstable_iterate(kfsid_t *identifier) {
	pthread_mutex_lock(&tablelock);
	bool result = kfstable_iterate_nolock(identifier);
	pthread_mutex_unlock(&tablelock);
	return result;
}

static bool kfstable_iterate_nolock(kfsid_t *identifier) {
	// sanity check the given identifier
	if (*identifier > MAX_FIELSYSTEMS) { *identifier = 0; }
	
	// search for the next identifier
	kfsid_t start = *identifier;
	bool success = true;
	while (table[*identifier] == NULL) {
		*identifier = (*identifier + 1) % MAX_FIELSYSTEMS;
		if (*identifier == start) {
			success = false;
			break;
		}
	}
	return success;
}

static kfsfilesystem_t *kfsfilesystem_duplicate(const kfsfilesystem_t *filesystem) {
	kfsfilesystem_t *result = malloc(sizeof(kfsfilesystem_t));
	memcpy(result, filesystem, sizeof(kfsfilesystem_t));
	result->options.mountpoint = strdup(filesystem->options.mountpoint);
	return result;
}

static void kfsfilesystem_free(kfsfilesystem_t *filesystem) {
	free((void *)filesystem->options.mountpoint);
	free(filesystem);
}
