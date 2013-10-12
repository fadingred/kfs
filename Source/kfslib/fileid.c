//
//  kfsfileid.c
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

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>

#include "fileid.h"

#pragma mark -
#pragma mark core foundation setup
// ----------------------------------------------------------------------------------------------------
// core foundation setup
// ----------------------------------------------------------------------------------------------------

#define FNV_PRIME_32 16777619U
#define FNV_PRIME_64 1099511628211U
#define FNV_BASIS_32 2166136261U
#define FNV_BASIS_64 14695981039346656037U

static CFMutableDictionaryRef filesystemMap = nil;

#if __LP64__
static const CFHashCode kHashPrime = FNV_PRIME_64;
static const CFHashCode kHashBasis = FNV_BASIS_64;
#else
static const CFHashCode kHashPrime = FNV_PRIME_32;
static const CFHashCode kHashBasis = FNV_BASIS_32;
#endif

static CFHashCode CStringHash(const char *string);
static CFHashCode CStringHash(const char *string) {
	CFHashCode hash = kHashBasis;
	while (*string) {
		hash = hash * kHashPrime;
		hash = hash ^ *string;
		string++;
	}
	return hash;	
}

static Boolean CStringEqual(const char *str1, const char *str2);
static Boolean CStringEqual(const char *str1, const char *str2) {
	return (str1 == str2) || (strcmp(str1, str2) == 0);
}

static void Initiailize(void) __attribute__((constructor));
static void Initiailize(void) {
	filesystemMap = CFDictionaryCreateMutable(NULL, 0, &(CFDictionaryKeyCallBacks){ }, &kCFTypeDictionaryValueCallBacks);
}

static void GetDictionariesForFilesystemWithID(kfsid_t identifier, CFMutableDictionaryRef *idMapOut, CFMutableDictionaryRef *pathMapOut);
static void GetDictionariesForFilesystemWithID(kfsid_t identifier, CFMutableDictionaryRef *idMapOut, CFMutableDictionaryRef *pathMapOut) {
	CFArrayRef dictionaries = CFDictionaryGetValue(filesystemMap, (void *)(uintptr_t)identifier);
	if (!dictionaries) {
		CFMutableDictionaryRef idMap = CFDictionaryCreateMutable(NULL, 0, &(CFDictionaryKeyCallBacks){ }, &(CFDictionaryValueCallBacks){ });
		CFMutableDictionaryRef pathMap = CFDictionaryCreateMutable(NULL, 0, &(CFDictionaryKeyCallBacks){
			.hash = (void *)CStringHash,
			.equal = (void *)CStringEqual
		}, &(CFDictionaryValueCallBacks){ });
		
		CFMutableDictionaryRef values[] = { idMap, pathMap };
		dictionaries = CFArrayCreate(NULL, (void const **)values, sizeof(values) / sizeof(CFMutableDictionaryRef), &kCFTypeArrayCallBacks);
		CFDictionarySetValue(filesystemMap, (void *)(uintptr_t)identifier, dictionaries);
		CFRelease(dictionaries);
	}
	
	if (dictionaries && CFArrayGetCount(dictionaries) == 2) {
		*idMapOut = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(dictionaries, 0);
		*pathMapOut = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(dictionaries, 1);
	}
}

#pragma mark -
#pragma mark function implementation
// ----------------------------------------------------------------------------------------------------
// function implementation
// ----------------------------------------------------------------------------------------------------

static pthread_mutex_t lock;

uint64_t kfs_fileid(kfsid_t fs, const char *path) {
	static uint64_t id = 1;
	uint64_t result = 0;
	
	pthread_mutex_lock(&lock);
	CFMutableDictionaryRef idMap = NULL;
	CFMutableDictionaryRef pathMap = NULL;
	GetDictionariesForFilesystemWithID(fs, &idMap, &pathMap);
	if (!(result = (uint64_t)(uintptr_t)CFDictionaryGetValue(pathMap, path))) {
		const char *save = strdup(path);
		CFDictionarySetValue(pathMap, save, (void *)(uintptr_t)id);
		CFDictionarySetValue(idMap, (void *)(uintptr_t)id, save);
		result = id++;
	}
	pthread_mutex_unlock(&lock);
	
	return result;
}

const char *path_fromid(kfsid_t fs, uint64_t fileid) {
	pthread_mutex_lock(&lock);
	CFMutableDictionaryRef idMap = NULL;
	CFMutableDictionaryRef pathMap = NULL;
	GetDictionariesForFilesystemWithID(fs, &idMap, &pathMap);
	const char *result = CFDictionaryGetValue(idMap, (void *)(uintptr_t)fileid);
	pthread_mutex_unlock(&lock);
	return result;
}

void kfs_idswap(kfsid_t fs, uint64_t id_one, uint64_t id_two) {
	const char *path_one = path_fromid(fs, id_one);
	const char *path_two = path_fromid(fs, id_two);
	
	pthread_mutex_lock(&lock);
	CFMutableDictionaryRef idMap = NULL;
	CFMutableDictionaryRef pathMap = NULL;
	GetDictionariesForFilesystemWithID(fs, &idMap, &pathMap);
	CFDictionarySetValue(pathMap, path_one, (void *)(uintptr_t)id_two);
	CFDictionarySetValue(idMap, (void *)(uintptr_t)id_two, path_one);
	CFDictionarySetValue(pathMap, path_two, (void *)(uintptr_t)id_one);
	CFDictionarySetValue(idMap, (void *)(uintptr_t)id_one, path_two);
	pthread_mutex_unlock(&lock);
}

void kfs_idclear(kfsid_t fs) {
	pthread_mutex_lock(&lock);
	CFMutableDictionaryRef idMap = NULL;
	CFMutableDictionaryRef pathMap = NULL;
	GetDictionariesForFilesystemWithID(fs, &idMap, &pathMap);
	
	// free strings associated with id and path map
	CFIndex count = CFDictionaryGetCount(pathMap);
	char **keys = malloc(sizeof(char *) * count);
	CFDictionaryGetKeysAndValues(pathMap, (void const **)keys, NULL);
	for (int i = 0; i < count; i++) {
		free(keys[i]);
	}
	free(keys);

	CFDictionaryRemoveValue(filesystemMap, (void *)(uintptr_t)fs);
	pthread_mutex_unlock(&lock);
}
