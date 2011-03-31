//
//  kfslib.c
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

#include "kfslib.h"
#include "internal.h"
#include "fileid.h"
#include "mountargs.h"
#include "nfs3programs.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <arpa/inet.h>
#include <rpc/pmap_clnt.h>
#include <pthread.h>

int _rpcpmstart;
int _rpcsvcdirty;

const char *kfs_devprefix = "kfs";

static unsigned short g_nfs_port = 0;
static void (*g_thread_begin)(void) = NULL;
static void (*g_thread_end)(void) = NULL;

static
int kfsrun(void);

#define _msgout(format, ...) do { fprintf(stderr, format "\n", ##__VA_ARGS__); } while (0)
#define _errout(format, ...) do { fprintf(stderr, format " %i: %s\n", ##__VA_ARGS__, errno, strerror(errno)); } while (0)
#define NFS_VUNREAL	999

static void finalize(void) __attribute__((destructor));
static void finalize(void) {
	kfsid_t identifier = 0;
	while ((kfstable_iterate(&identifier))) {
		kfs_unmount(identifier);
	}
}

kfsid_t kfs_mount(const kfsfilesystem_t *filesystem) {
	// start the nfs server one time
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, (void (*)(void))kfsrun);
	
	// get a unique identifier
	kfsid_t identifier = kfstable_put(filesystem);

	// setup arguments
	char *fshandle = NULL;
	asprintf(&fshandle, "%llu", identifier);

	char *hostname = NULL;
	asprintf(&hostname, "%s%llu", kfs_devprefix, identifier);

	struct sockaddr_in nfsaddr = (struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_port = g_nfs_port,
		.sin_addr.s_addr = inet_addr("127.0.0.1"),
	};

	struct nfs_args3 args = (struct nfs_args3){
		.version = NFS_V3,
		.addr = (struct sockaddr *)&nfsaddr,
		.addrlen = sizeof(struct sockaddr_in),
		.sotype = SOCK_STREAM,
		.proto = IPPROTO_TCP,
		.fh = (u_char *)fshandle,
		.fhsize = strlen(fshandle),
		.flags = NFSMNT_NFSV3 | NFSMNT_WSIZE | NFSMNT_RSIZE | NFSMNT_READDIRSIZE |
				 NFSMNT_TIMEO | NFSMNT_RETRANS | NFSMNT_NOLOCKS | NFSMNT_DEADTIMEOUT,
		.wsize = WRITE_MAX_LEN,
		.rsize = READ_MAX_LEN,
		.readdirsize = DIR_MAX_LEN,
		.timeo = 1,
		.retrans = 4,
		.maxgrouplist = 0,
		.readahead = 0,
		.hostname = hostname,
	};

	if (identifier >= 0) {
		if (mkdir(filesystem->options.mountpoint, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
			kfstable_remove(identifier);
			identifier = -1;
		}
	}

	if (identifier >= 0) {
		int flags = MNT_SYNCHRONOUS;
		if (!filesystem->write || !filesystem->create || !filesystem->remove ||
			!filesystem->rename || !filesystem->truncate ||
			!filesystem->mkdir || !filesystem->rmdir) { flags |= MNT_RDONLY; }
		if (mount("nfs", filesystem->options.mountpoint, flags, &args) != 0) {
			kfstable_remove(identifier);
			identifier = -1;
		}
	}
	
	free(fshandle);
	free(hostname);
	
	return identifier;
}

void kfs_unmount(kfsid_t identifier) {
	const kfsfilesystem_t *filesystem = kfstable_get(identifier);

	// unmount the filesystem
	if (filesystem) {
		unmount(filesystem->options.mountpoint, MNT_FORCE);
		rmdir(filesystem->options.mountpoint);
	}

	// remove the entry from our table
	kfstable_remove(identifier);
	
	// free any file ids
	kfs_idclear(identifier);
}


#pragma mark -
#pragma mark running the nfs server
// ----------------------------------------------------------------------------------------------------
// running the nfs server
// ----------------------------------------------------------------------------------------------------

static int _kfsrun(void) {
	if (g_thread_begin) { g_thread_begin(); }

	svc_run();
	_msgout("svc_run returned unexpectedly");
	
	if (g_thread_end) { g_thread_end(); }
	return 1; // should never reach here
}

static int kfsrun(void) {
	// create and bind a new socket for kfs to use
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(0),
		.sin_addr.s_addr = inet_addr("127.0.0.1"),
	};
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		_errout("bind failed.");
		return 1;
	}

	// get the socket name and store off the port we're using
	struct sockaddr_in baddr = {};
	socklen_t len = sizeof(baddr);
	if (getsockname(sock, (struct sockaddr *)&baddr, &len) != 0) {
		_errout("getsockname failed.");
		return 1;
	}
	g_nfs_port = baddr.sin_port;
	
	// wake up the portmap daemon. on os x 10.7 (lion) the nfs implementation will hang if
	// it can't communicate with the portmap daemon. this call is enough to get it started
	// up, and as long as our nfs port is active, the portmap daemon will stay alive. the
	// point of using an unreal version number is to avoid causing any problems with any
	// real nfs servers that could be running.
	pmap_unset(NFS_PROGRAM, NFS_VUNREAL);
	pmap_set(NFS_PROGRAM, NFS_VUNREAL, IPPROTO_TCP, g_nfs_port);
	
	// create the service, then register the nfs and mount programs. we supply
	// a protocol of 0 here so that the programs aren't registered with portmap
	// (as per the documentation).
	SVCXPRT *transp = svctcp_create(sock, 0, 0);
	if (transp == NULL) {
		_msgout("cannot create tcp service.");
		return 1;
	}
	if (!svc_register(transp, NFS_PROGRAM, NFS_V3, nfs_program_3, 0)) {
		_msgout("unable to register (NFS_PROGRAM, NFS_V3, tcp).");
		return 1;
	}
	if (!svc_register(transp, MOUNT_PROGRAM, MOUNT_V3, mount_program_3, 0)) {
		_msgout("unable to register (MOUNT_PROGRAM, MOUNT_V3, tcp).");
		return 1;
	}
	
	// do the actual run loop on a background thread
	pthread_t thread;
	pthread_create(&thread, NULL, (void *(*)(void *))_kfsrun, NULL);
	
	return 0;
}


#pragma mark -
#pragma mark type implementations
// ----------------------------------------------------------------------------------------------------
// type implementations
// ----------------------------------------------------------------------------------------------------

kfscontents_t *kfscontents_create(void) {
	return calloc(1, sizeof(struct kfscontents));
}

void kfscontents_destroy(kfscontents_t *contents) {
	if (contents) {
		for (uint64_t i = 0; i < kfscontents_count(contents); i++) {
			free((void *)kfscontents_at(contents, i));
		}
		free(contents);
	}
}

void kfscontents_append(kfscontents_t *contents, const char *entry) {
	unsigned int cap = contents->capacity;
	unsigned int pos = contents->count;
	unsigned int len = contents->count + 1;
		
	if (cap < len) {
		if (cap == 0) { cap = 1; }
		else { cap *= 2; }

		contents->entries = realloc(contents->entries, sizeof(const char *) * cap);
	}
	
	contents->entries[pos] = strdup(entry);
	contents->capacity = cap;
	contents->count = len;
}

uint64_t kfscontents_count(kfscontents_t *contents) {
	return contents->count;
}

const char *kfscontents_at(kfscontents_t *contents, uint64_t idx) {
	const char *entry = NULL;
	if (idx < contents->count) {
		entry = contents->entries[idx];
	}
	return entry;
}


#pragma mark -
#pragma mark thread helpers
// ----------------------------------------------------------------------------------------------------
// thread helpers
// ----------------------------------------------------------------------------------------------------

void kfs_set_thread_begin_callback(void (*fn)(void))  { g_thread_begin = fn; }
void kfs_set_thread_end_callback(void (*fn)(void)) { g_thread_end = fn; }


#pragma mark -
#pragma mark error handling
// ----------------------------------------------------------------------------------------------------
// error handling
// ----------------------------------------------------------------------------------------------------

void kfs_perror(const char *user_str) {
	const char *msg = NULL;
	switch (errno) {
		case EKFS_INTR:
			msg = "KFS internal error.";
			break;
		case EKFS_EMFS:
			msg = "KFS maximum filesystems exceeded.";
			break;
		default:
			break;
	}
	if (msg) {
		if (user_str) { fprintf(stderr, "%s: %s", user_str, msg); }
		else { fprintf(stderr, "%s", msg); }
	} else {
		perror(user_str);
	}
}
