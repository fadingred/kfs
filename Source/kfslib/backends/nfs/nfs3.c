//
//  nfs.c
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

#include "nfs3.h"
#include "kfslib.h"
#include "internal.h"
#include "fileid.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

//#define KFS_DEBUG_LOG
#ifdef KFS_DEBUG_LOG
#define dlog(format, ...) do { fprintf(stderr, format "\n", ##__VA_ARGS__); } while (0)
#define dlog_begin(format, ...) do { \
	dlog("begin %s", __FUNCTION__); \
	if (strlen(format)) { dlog(format, ##__VA_ARGS__); } \
} while (0)
#define dlog_end() do { \
	dlog("\t%s %i", result.status == NFS3_OK ? "ok" : "error", result.status); \
} while (0)
#else
#define dlog(format, ...)
#define dlog_begin(format, ...)
#define dlog_end()
#endif

#define NFS_IRUSR 0x00100
#define NFS_IWUSR 0x00080
#define NFS_IXUSR 0x00040
#define NFS_IRGRP 0x00020
#define NFS_IWGRP 0x00010
#define NFS_IXGRP 0x00008
#define NFS_IROTH 0x00004
#define NFS_IWOTH 0x00002
#define NFS_IXOTH 0x00001


/* Helper Methods
 * ------------------------------------------------------------------------- */

static const kfsfilesystem_t *get_filesystem_from_path(const char *path, const char **outPath, uint64_t *outIdentifier) {
	char *seperate = strdup(path);
	char *duplicated = seperate;
	char *fsid_str = strsep(&seperate, ":");
	char *fileid_str = seperate;
	
	uint64_t fsid = fsid_str ? (uint64_t)atoll(fsid_str) : 0ULL;
	uint64_t fileid = fileid_str ? (uint64_t)atoll(fileid_str) : kfs_fileid(fsid, "/");
	const kfsfilesystem_t *filesystem = kfstable_get(fsid);
	if (outPath) { *outPath = path_fromid(fsid, fileid); }
	if (outIdentifier) { *outIdentifier = fsid; }
	free(duplicated);
	return filesystem;
}

const kfsfilesystem_t *get_filesystem(nfs_fh3 object, const char **outPath, uint64_t *outIdentifier);
const kfsfilesystem_t *get_filesystem(nfs_fh3 object, const char **outPath, uint64_t *outIdentifier) {
	return get_filesystem_from_path(object.data.data_val, outPath, outIdentifier);
}

nfsstat3 convert_status(int err, nfsstat3 default_status);
nfsstat3 convert_status(int err, nfsstat3 default_status) {
	switch (err) {
		case EPERM: return NFS3ERR_PERM; break;
		case ENOENT: return NFS3ERR_NOENT; break;
		case EIO: return NFS3ERR_IO; break;
		case ENXIO: return NFS3ERR_NXIO; break;
		case EACCES: return NFS3ERR_ACCES; break;
		case EEXIST: return NFS3ERR_EXIST; break;
		case EXDEV: return NFS3ERR_XDEV; break;
		case ENODEV: return NFS3ERR_NODEV; break;
		case ENOTDIR: return NFS3ERR_NOTDIR; break;
		case EISDIR: return NFS3ERR_ISDIR; break;
		case EINVAL: return NFS3ERR_INVAL; break;
		case EFBIG: return NFS3ERR_FBIG; break;
		case ENOSPC: return NFS3ERR_NOSPC; break;
		case EROFS: return NFS3ERR_ROFS; break;
		case EMLINK: return NFS3ERR_MLINK; break;
		case ENAMETOOLONG: return NFS3ERR_NAMETOOLONG; break;
		case ENOTEMPTY: return NFS3ERR_NOTEMPTY; break;
		case EDQUOT: return NFS3ERR_DQUOT; break;
		default: return default_status; break;
	}
}

nfsstat3 get_fattr(nfs_fh3 object, fattr3 *result);
nfsstat3 get_fattr(nfs_fh3 object, fattr3 *result) {
	nfsstat3 status = NFS3_OK;
	uint64_t identifier = 0;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(object, &path, &identifier);
	if (filesystem) {
		dlog("\t%s (path, getattr)", path);
		
		kfsstat_t sbuf = {};
		if (filesystem->stat(path, &sbuf, &error, filesystem->context)) {
			*result = (fattr3){};
			
			if (0) {}
			else if (sbuf.type == KFS_REG) { result->type = NF3REG; }
			else if (sbuf.type == KFS_DIR) { result->type = NF3DIR; }
			else if (sbuf.type == KFS_BLK) { result->type = NF3BLK; }
			else if (sbuf.type == KFS_CHR) { result->type = NF3CHR; }
			else if (sbuf.type == KFS_LNK) { result->type = NF3LNK; }
			else if (sbuf.type == KFS_SOCK) { result->type = NF3SOCK; }
			else if (sbuf.type == KFS_FIFO) { result->type = NF3FIFO; }
			
			if (sbuf.mode & KFS_IRUSR) { result->mode |= NFS_IRUSR; }
			if (sbuf.mode & KFS_IWUSR) { result->mode |= NFS_IWUSR; }
			if (sbuf.mode & KFS_IXUSR) { result->mode |= NFS_IXUSR; }
			if (sbuf.mode & KFS_IRGRP) { result->mode |= NFS_IRGRP; }
			if (sbuf.mode & KFS_IWGRP) { result->mode |= NFS_IWGRP; }
			if (sbuf.mode & KFS_IXGRP) { result->mode |= NFS_IXGRP; }
			if (sbuf.mode & KFS_IROTH) { result->mode |= NFS_IROTH; }
			if (sbuf.mode & KFS_IWOTH) { result->mode |= NFS_IWOTH; }
			if (sbuf.mode & KFS_IXOTH) { result->mode |= NFS_IXOTH; }
			
			result->nlink = 1;
			result->uid = getuid();
			result->gid = getgid();
			result->size = sbuf.size;
			result->used = sbuf.used;
			result->rdev = (specdata3){ 0, 0 };
			result->fsid = 0;
			result->fileid = kfs_fileid(identifier, path);
			result->atime = (nfstime3){ sbuf.atime.sec, sbuf.atime.nsec };
			result->mtime = (nfstime3){ sbuf.mtime.sec, sbuf.mtime.nsec };
			result->ctime = (nfstime3){ sbuf.ctime.sec, sbuf.ctime.nsec };
		} else { // stat failed
			status = convert_status(error, NFS3ERR_NOENT);
		}
	} else { // no filesystem
		status = NFS3ERR_BADHANDLE;
	}

	return status;
}


nfsstat3 set_fattr(nfs_fh3 object, const sattr3 *attrs);
nfsstat3 set_fattr(nfs_fh3 object, const sattr3 *attrs) {
	nfsstat3 status = NFS3_OK;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(object, &path, NULL);
	if (filesystem) {
		dlog("\t%s (path, setattr)", path);

		// check for resize
		if (status == NFS3_OK && attrs->size.set_it) {
			if (!filesystem->truncate(path, attrs->size.set_size3_u.size, &error, filesystem->context)) {
				status = convert_status(error, NFS3ERR_NOENT); // truncate failed
			}
		}

		// check for mode change
		if (status == NFS3_OK && attrs->mode.set_it) {
			kfsmode_t mode = 0;
			if (attrs->mode.set_mode3_u.mode & NFS_IRUSR) { mode |= KFS_IRUSR; }
			if (attrs->mode.set_mode3_u.mode & NFS_IWUSR) { mode |= KFS_IWUSR; }
			if (attrs->mode.set_mode3_u.mode & NFS_IXUSR) { mode |= KFS_IXUSR; }
			if (attrs->mode.set_mode3_u.mode & NFS_IRGRP) { mode |= KFS_IRGRP; }
			if (attrs->mode.set_mode3_u.mode & NFS_IWGRP) { mode |= KFS_IWGRP; }
			if (attrs->mode.set_mode3_u.mode & NFS_IXGRP) { mode |= KFS_IXGRP; }
			if (attrs->mode.set_mode3_u.mode & NFS_IROTH) { mode |= KFS_IROTH; }
			if (attrs->mode.set_mode3_u.mode & NFS_IWOTH) { mode |= KFS_IWOTH; }
			if (attrs->mode.set_mode3_u.mode & NFS_IXOTH) { mode |= KFS_IXOTH; }
			
			if (!filesystem->chmod(path, mode, &error, filesystem->context)) {
				status = convert_status(error, NFS3ERR_NOENT); // chmod failed
			}
		}
		
		// set times
		if (status == NFS3_OK && (attrs->atime.set_it || attrs->mtime.set_it)) {
			kfstime_t *atime = attrs->atime.set_it ? &(kfstime_t){
				.sec = attrs->atime.set_atime_u.atime.seconds,
				.nsec = attrs->atime.set_atime_u.atime.nseconds
			} : NULL;
			kfstime_t *mtime = attrs->mtime.set_it ? &(kfstime_t){
				.sec = attrs->mtime.set_mtime_u.mtime.seconds,
				.nsec = attrs->mtime.set_mtime_u.mtime.nseconds
			} : NULL;
			
			if (!filesystem->utimes(path, atime, mtime, &error, filesystem->context)) {
				status = convert_status(error, NFS3ERR_NOENT); // utimes failed
			}
		}
		
		// set uid
		if (status == NFS3_OK && attrs->uid.set_it) {
			// currently not supported, allow sets to current uid
			if (attrs->uid.set_uid3_u.uid != getuid()) {
				status = NFS3ERR_NOTSUPP;
			}
		}
		
		// set gid
		if (status == NFS3_OK && attrs->gid.set_it) {
			// currently not supported, allow sets to current gid or to 0
			if (attrs->gid.set_gid3_u.gid != getgid() &&
				attrs->gid.set_gid3_u.gid != 0) {
				status = NFS3ERR_NOTSUPP;
			}
		}
	} else { // no filesystem
		status = NFS3ERR_BADHANDLE;
	}
	
	return status;
}

nfsstat3 get_required_post_op(post_op_attr *result, nfs_fh3 object);
nfsstat3 get_required_post_op(post_op_attr *result, nfs_fh3 object) {
	result->attributes_follow = true;
	return get_fattr(object, &result->post_op_attr_u.attributes);
}

nfsstat3 get_pre_op(pre_op_attr *result, nfs_fh3 object);
nfsstat3 get_pre_op(pre_op_attr *result, nfs_fh3 object) {
	result->attributes_follow = false;
	return NFS3_OK;
}

nfsstat3 get_post_op(post_op_attr *result, nfs_fh3 object);
nfsstat3 get_post_op(post_op_attr *result, nfs_fh3 object) {
	// we could get post op attributes here, but there's little
	// reason to. we're not really trying to handle the same
	// complexities of a real NFS server. if there's a reason
	// to, though, it's easy with:
	// return get_required_post_op(result, object);
	result->attributes_follow = false;
	return NFS3_OK;
}


/* RPC Methods (stubs generated by rpcgen)
 * ------------------------------------------------------------------------- */

void *
nfsproc3_null_3_svc(struct svc_req *rqstp) {
	dlog_begin("");
	static char* result;
	return((void*) &result);
}

GETATTR3res *
nfsproc3_getattr_3_svc(GETATTR3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle)", args.object.data.data_val);
	static GETATTR3res result;
	result.status = get_fattr(args.object, &result.GETATTR3res_u.resok.obj_attributes);
	dlog_end();
	return(&result);
}

SETATTR3res *
nfsproc3_setattr_3_svc(SETATTR3args args,  struct svc_req *rqstp) {
	dlog_begin("");
	static SETATTR3res result;

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.SETATTR3res_u.resok.obj_wcc.before :
		&result.SETATTR3res_u.resfail.obj_wcc.before;
	get_pre_op(pre_op, args.object);
	
	// assume we're okay to start
	result.status = NFS3_OK;
	
	// guard check
	if (args.guard.check) {
		fattr3 attrs;
		get_fattr(args.object, &attrs);
		if (attrs.ctime.seconds != args.guard.sattrguard3_u.obj_ctime.seconds ||
			attrs.ctime.nseconds != args.guard.sattrguard3_u.obj_ctime.nseconds) {
			result.status = NFS3ERR_NOT_SYNC;
		}
	}
	
	// after guard check
	if (result.status == NFS3_OK) {
		result.status = set_fattr(args.object, &args.new_attributes);
	}

	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.SETATTR3res_u.resok.obj_wcc.after :
		&result.SETATTR3res_u.resfail.obj_wcc.after;
	get_post_op(post_op, args.object);
	dlog_end();
	return(&result);
}

LOOKUP3res *
nfsproc3_lookup_3_svc(LOOKUP3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle), %s", args.what.dir.data.data_val, args.what.name);

	static LOOKUP3res result;
	const char *path = NULL;
	uint64_t identifier = 0;
	const kfsfilesystem_t *filesystem = get_filesystem(args.what.dir, &path, &identifier);
	if (filesystem) {
		dlog("\t%s (path)", path);
		static char filehandle[PATH_MAX];
		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.what.name);
		snprintf(filehandle, PATH_MAX, "%llu:%llu", identifier, kfs_fileid(identifier, fspath));
		
		result.status = NFS3_OK;
		result.LOOKUP3res_u.resok.object.data.data_val = filehandle;
		result.LOOKUP3res_u.resok.object.data.data_len = strlen(filehandle) + 1;
		
		nfsstat3 objstatus = get_required_post_op(&result.LOOKUP3res_u.resok.obj_attributes,
												   result.LOOKUP3res_u.resok.object);
		switch (objstatus) {
			case NFS3_OK:
			case NFS3ERR_IO:
			case NFS3ERR_NOENT:
			case NFS3ERR_ACCES:
			case NFS3ERR_NAMETOOLONG:
			case NFS3ERR_STALE:
			case NFS3ERR_BADHANDLE:
			case NFS3ERR_SERVERFAULT:
				result.status = objstatus;
				break;
			default:
				result.status = NFS3ERR_SERVERFAULT;
				break;
		}
		
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.LOOKUP3res_u.resok.dir_attributes :
		&result.LOOKUP3res_u.resfail.dir_attributes;
	get_post_op(post_op, args.what.dir);
	dlog_end();
	return(&result);
}

ACCESS3res *
nfsproc3_access_3_svc(ACCESS3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle), %i", args.object.data.data_val, args.access);
	static ACCESS3res result;
	
	fattr3 attr = {};
	get_fattr(args.object, &attr);

	uint32 flags = 0;
	uint32 flags_read = ACCESS3_READ;
	uint32 flags_write = ACCESS3_MODIFY | ACCESS3_EXTEND | ACCESS3_DELETE;
	uint32 flags_execute = ACCESS3_EXECUTE | ACCESS3_LOOKUP;
	
	if ((attr.mode & NFS_IRUSR) && (attr.uid == getuid())) { flags |= flags_read; }
	else if ((attr.mode & NFS_IRGRP) && (attr.gid == getgid())) { flags |= flags_read; }
	else if ((attr.mode & NFS_IROTH)) { flags |= flags_read; }

	if ((attr.mode & NFS_IWUSR) && (attr.uid == getuid())) { flags |= flags_write; }
	else if ((attr.mode & NFS_IWGRP) && (attr.gid == getgid())) { flags |= flags_write; }
	else if ((attr.mode & NFS_IWOTH)) { flags |= flags_write; }

	if ((attr.mode & NFS_IXUSR) && (attr.uid == getuid())) { flags |= flags_execute; }
	else if ((attr.mode & NFS_IXGRP) && (attr.gid == getgid())) { flags |= flags_execute; }
	else if ((attr.mode & NFS_IXOTH)) { flags |= flags_execute; }
	
	result.status = NFS3_OK;
	result.ACCESS3res_u.resok.access = flags;
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.ACCESS3res_u.resok.obj_attributes :
		&result.ACCESS3res_u.resfail.obj_attributes;
	get_post_op(post_op, args.object);
	dlog_end();
	return(&result);
}

READLINK3res *
nfsproc3_readlink_3_svc(READLINK3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle)", args.symlink.data.data_val);
	static READLINK3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.symlink, &path, NULL);
	if (filesystem) {
		dlog("\t%s (path)", path);
		char *data = NULL;
		if (filesystem->readlink(path, &data, &error, filesystem->context)) {
			static char buffer[PATH_MAX];
			strncpy(buffer, data, PATH_MAX);
			result.status = NFS3_OK;
			result.READLINK3res_u.resok.data = buffer;
		} else { // readlink failed
			result.status = convert_status(error, NFS3ERR_INVAL);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_INVAL:
				case NFS3ERR_ACCES:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
		free(data);
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.READLINK3res_u.resok.symlink_attributes :
		&result.READLINK3res_u.resfail.symlink_attributes;
	get_post_op(post_op, args.symlink);
	dlog_end();
	return(&result);
}

READ3res *
nfsproc3_read_3_svc(READ3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s %lli %i", args.file.data.data_val, args.offset, args.count);
	static READ3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.file, &path, NULL);
	if (filesystem) {
		dlog("\t%s (path)", path);
		static char buffer[READ_MAX_LEN];
		int count = 0;
		int rsize = args.count;
		if (rsize > READ_MAX_LEN) { rsize = READ_MAX_LEN; }
		if ((count = filesystem->read(path, buffer, args.offset, rsize, &error, filesystem->context)) != -1) {
			result.status = NFS3_OK;
			result.READ3res_u.resok.data.data_val = buffer;
			result.READ3res_u.resok.data.data_len = READ_MAX_LEN;
			result.READ3res_u.resok.count = count;
			result.READ3res_u.resok.eof = (count == 0);

		} else { // read failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_NXIO:
				case NFS3ERR_ACCES:
				case NFS3ERR_INVAL:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.READ3res_u.resok.file_attributes :
		&result.READ3res_u.resfail.file_attributes;
	get_post_op(post_op, args.file);
	dlog_end();
	return(&result);
}

WRITE3res *
nfsproc3_write_3_svc(WRITE3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %lli %i", args.file.data.data_val, args.offset, args.count);
	static WRITE3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.file, &path, NULL);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.WRITE3res_u.resok.file_wcc.before :
		&result.WRITE3res_u.resfail.file_wcc.before;
	get_pre_op(pre_op, args.file);
	
	if (filesystem) {
		dlog("\t%s (path)", path);
		int count = 0;
		int wsize = args.count;
		if (wsize > WRITE_MAX_LEN) { wsize = WRITE_MAX_LEN; }
		if ((count = filesystem->write(path, args.data.data_val, args.offset, wsize, &error, filesystem->context)) != -1) {
			result.status = NFS3_OK;
			result.WRITE3res_u.resok.count = count;
			result.WRITE3res_u.resok.committed = FILE_SYNC;
		} else { // write failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_FBIG:
				case NFS3ERR_DQUOT:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_INVAL:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.WRITE3res_u.resok.file_wcc.after :
		&result.WRITE3res_u.resfail.file_wcc.after;
	get_post_op(post_op, args.file);
	dlog_end();
	return(&result);
}

CREATE3res *
nfsproc3_create_3_svc(CREATE3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.where.dir.data.data_val, args.where.name);
	static CREATE3res result;
	uint64_t identifier = 0;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.where.dir, &path, &identifier);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.CREATE3res_u.resok.dir_wcc.before :
		&result.CREATE3res_u.resfail.dir_wcc.before;
	get_pre_op(pre_op, args.where.dir);
	
	if (filesystem) {
		dlog("\t%s (path)", path);

		static char filehandle[PATH_MAX];
		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.where.name);
		snprintf(filehandle, PATH_MAX, "%llu:%llu", identifier, kfs_fileid(identifier, fspath));
		nfs_fh3 fh = { .data = { .data_val = filehandle, .data_len = strlen(filehandle) + 1, } };
		
		// assume we're okay to start
		result.status = NFS3_OK;
		
		// mode check
		if (args.how.mode == UNCHECKED) { } // no checks needed
		else if (args.how.mode == GUARDED) {
			fattr3 attrs;
			if (get_fattr(fh, &attrs) != NFS3_OK) {
				result.status = NFS3ERR_EXIST;
			}
		}
		else if (args.how.mode == EXCLUSIVE) {
			result.status = NFS3ERR_NOTSUPP;
		}

		// after mode check
		if (result.status == NFS3_OK) {
			if (filesystem->create(fspath, &error, filesystem->context)) {
				result.status = NFS3_OK;
				result.CREATE3res_u.resok.obj.handle_follows = true;
				result.CREATE3res_u.resok.obj.post_op_fh3_u.handle = fh;
				
				// set attributes now
				nfsstat3 setstatus = set_fattr(fh, &args.how.createhow3_u.obj_attributes);
				switch (setstatus) {
					case NFS3_OK:
					case NFS3ERR_IO:
					case NFS3ERR_ACCES:
					case NFS3ERR_EXIST:
					case NFS3ERR_NOTDIR:
					case NFS3ERR_NOSPC:
					case NFS3ERR_ROFS:
					case NFS3ERR_NAMETOOLONG:
					case NFS3ERR_DQUOT:
					case NFS3ERR_STALE:
					case NFS3ERR_BADHANDLE:
					case NFS3ERR_NOTSUPP:
					case NFS3ERR_SERVERFAULT:
						result.status = setstatus;
						break;
					default:
						result.status = NFS3ERR_SERVERFAULT;
						break;
				}
				
				if (setstatus != NFS3_OK) {
					// remove the file if there was an error (and don't worry about
					// whether this is successful or not)
					filesystem->remove(fspath, &(int){0}, filesystem->context);
				}
				
				get_required_post_op(&result.CREATE3res_u.resok.obj_attributes, fh);
				
			} else { // create failed
				result.status = convert_status(error, NFS3ERR_IO);
				switch (result.status) {
					case NFS3_OK:
					case NFS3ERR_IO:
					case NFS3ERR_ACCES:
					case NFS3ERR_EXIST:
					case NFS3ERR_NOTDIR:
					case NFS3ERR_NOSPC:
					case NFS3ERR_ROFS:
					case NFS3ERR_NAMETOOLONG:
					case NFS3ERR_DQUOT:
					case NFS3ERR_STALE:
					case NFS3ERR_BADHANDLE:
					case NFS3ERR_NOTSUPP:
					case NFS3ERR_SERVERFAULT:
						break;
					default:
						result.status = NFS3ERR_SERVERFAULT;
						break;
				}
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.CREATE3res_u.resok.dir_wcc.after :
		&result.CREATE3res_u.resfail.dir_wcc.after;
	get_post_op(post_op, args.where.dir);
	dlog_end();
	return(&result);
}

MKDIR3res *
nfsproc3_mkdir_3_svc(MKDIR3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.where.dir.data.data_val, args.where.name);
	static MKDIR3res result;
	uint64_t identifier = 0;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.where.dir, &path, &identifier);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.MKDIR3res_u.resok.dir_wcc.before :
		&result.MKDIR3res_u.resfail.dir_wcc.before;
	get_pre_op(pre_op, args.where.dir);
	
	if (filesystem) {
		dlog("\t%s (path)", path);

		static char filehandle[PATH_MAX];
		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.where.name);
		snprintf(filehandle, PATH_MAX, "%llu:%llu", identifier, kfs_fileid(identifier, fspath));
		nfs_fh3 fh = { .data = { .data_val = filehandle, .data_len = strlen(filehandle) + 1, } };
		
		if (filesystem->mkdir(fspath, &error, filesystem->context)) {
			result.status = NFS3_OK;
			result.MKDIR3res_u.resok.obj.handle_follows = true;
			result.MKDIR3res_u.resok.obj.post_op_fh3_u.handle = fh;
			
			// set attributes
			nfsstat3 setstatus = set_fattr(fh, &args.attributes);
			switch (setstatus) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_EXIST:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_DQUOT:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					result.status = setstatus;
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
			
			if (setstatus != NFS3_OK) {
				// remove the directory if there was an error (and don't worry about
				// whether this is successful or not)
				filesystem->rmdir(fspath, &(int){0}, filesystem->context);
			}

			get_required_post_op(&result.MKDIR3res_u.resok.obj_attributes, fh);
			
		} else { // mkdir failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_EXIST:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_DQUOT:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.MKDIR3res_u.resok.dir_wcc.after :
		&result.MKDIR3res_u.resfail.dir_wcc.after;
	get_post_op(post_op, args.where.dir);
	dlog_end();
	return(&result);
}

SYMLINK3res *
nfsproc3_symlink_3_svc(SYMLINK3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.where.dir.data.data_val, args.where.name);
	static SYMLINK3res result;
	uint64_t identifier = 0;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.where.dir, &path, &identifier);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.SYMLINK3res_u.resok.dir_wcc.before :
		&result.SYMLINK3res_u.resfail.dir_wcc.before;
	get_pre_op(pre_op, args.where.dir);
	
	if (filesystem) {
		dlog("\t%s (path)", path);

		static char filehandle[PATH_MAX];
		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.where.name);
		snprintf(filehandle, PATH_MAX, "%llu:%llu", identifier, kfs_fileid(identifier, fspath));
		nfs_fh3 fh = { .data = { .data_val = filehandle, .data_len = strlen(filehandle) + 1, } };
		
		if (filesystem->symlink(fspath, args.symlink.symlink_data, &error, filesystem->context)) {
			result.status = NFS3_OK;
			result.SYMLINK3res_u.resok.obj.handle_follows = true;
			result.SYMLINK3res_u.resok.obj.post_op_fh3_u.handle = fh;
			
			// set attributes
			nfsstat3 setstatus = set_fattr(fh, &args.symlink.symlink_attributes);
			switch (setstatus) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_EXIST:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_DQUOT:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					result.status = setstatus;
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
			
			get_required_post_op(&result.SYMLINK3res_u.resok.obj_attributes, fh);
			
		} else { // symlink failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_EXIST:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_DQUOT:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.SYMLINK3res_u.resok.dir_wcc.after :
		&result.SYMLINK3res_u.resfail.dir_wcc.after;
	get_post_op(post_op, args.where.dir);
	dlog_end();
	return(&result);
}

MKNOD3res *
nfsproc3_mknod_3_svc(MKNOD3args args,  struct svc_req *rqstp) {
	dlog_begin("");
	static MKNOD3res result;
	result.status = NFS3ERR_NOTSUPP;
	dlog_end();
	return(&result);
}

REMOVE3res *
nfsproc3_remove_3_svc(REMOVE3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.object.dir.data.data_val, args.object.name);
	static REMOVE3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.object.dir, &path, NULL);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.REMOVE3res_u.resok.dir_wcc.before :
		&result.REMOVE3res_u.resfail.dir_wcc.before;
	get_pre_op(pre_op, args.object.dir);
	
	if (filesystem) {
		dlog("\t%s (path)", path);

		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.object.name);
		
		if (filesystem->remove(fspath, &error, filesystem->context)) {
			result.status = NFS3_OK;
		} else { // remove failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_NOENT:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_ROFS:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.REMOVE3res_u.resok.dir_wcc.after :
		&result.REMOVE3res_u.resfail.dir_wcc.after;
	get_post_op(post_op, args.object.dir);
	dlog_end();
	return(&result);
}

RMDIR3res *
nfsproc3_rmdir_3_svc(RMDIR3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.object.dir.data.data_val, args.object.name);
	static RMDIR3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.object.dir, &path, NULL);

	pre_op_attr *pre_op = (result.status == NFS3_OK) ?
		&result.RMDIR3res_u.resok.dir_wcc.before :
		&result.RMDIR3res_u.resfail.dir_wcc.before;
	get_pre_op(pre_op, args.object.dir);
	
	if (filesystem) {
		dlog("\t%s (path)", path);

		static char fspath[PATH_MAX];
		bool root = (strcmp(path, "/") == 0);
		snprintf(fspath, PATH_MAX, root ? "%s%s" : "%s/%s", path, args.object.name);
		
		if (filesystem->rmdir(fspath, &error, filesystem->context)) {
			result.status = NFS3_OK;
		} else { // rmdir failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_NOENT:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_INVAL:
				case NFS3ERR_EXIST:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_ROFS:
				case NFS3ERR_NOTEMPTY:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.RMDIR3res_u.resok.dir_wcc.after :
		&result.RMDIR3res_u.resfail.dir_wcc.after;
	get_post_op(post_op, args.object.dir);
	dlog_end();
	return(&result);
}

RENAME3res *
nfsproc3_rename_3_svc(RENAME3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %s", args.from.dir.data.data_val, args.to.dir.data.data_val);
	static RENAME3res result;
	uint64_t from_identifier = 0;
	uint64_t to_identifier = 0;
	int error = 0;
	const char *from_path = NULL;
	const char *to_path = NULL;
	const kfsfilesystem_t *from_filesystem = get_filesystem(args.from.dir, &from_path, &from_identifier);
	const kfsfilesystem_t *to_filesystem = get_filesystem(args.to.dir, &to_path, &to_identifier);

	pre_op_attr *from_pre_op = (result.status == NFS3_OK) ?
		&result.RENAME3res_u.resok.fromdir_wcc.before :
		&result.RENAME3res_u.resfail.fromdir_wcc.before;
	pre_op_attr *to_pre_op = (result.status == NFS3_OK) ?
		&result.RENAME3res_u.resok.todir_wcc.before :
		&result.RENAME3res_u.resfail.todir_wcc.before;
	get_pre_op(from_pre_op, args.from.dir);
	get_pre_op(to_pre_op, args.to.dir);
	
	if ((from_filesystem && to_filesystem) &&
		(from_filesystem == to_filesystem) &&
		(from_identifier == to_identifier)) {
		dlog("\t%s (path) %s (path)", from_path, to_path);

		static char from_fspath[PATH_MAX];
		bool from_root = (strcmp(from_path, "/") == 0);
		snprintf(from_fspath, PATH_MAX, from_root ? "%s%s" : "%s/%s", from_path, args.from.name);

		static char to_fspath[PATH_MAX];
		bool to_root = (strcmp(to_path, "/") == 0);
		snprintf(to_fspath, PATH_MAX, to_root ? "%s%s" : "%s/%s", to_path, args.to.name);
		
		if (from_filesystem->rename(from_fspath, to_fspath, &error, from_filesystem->context)) {
			// swap ids so our file handle isn't stale
			// the destination has been removed, so swapping (rather than overwriting
			// and generating a new id for the destination path) should be just fine. the
			// nfs client really shouldn't use the destination's file handle any more.
			kfs_idswap(from_identifier,
					   kfs_fileid(from_identifier, from_fspath),
					   kfs_fileid(to_identifier, to_fspath));
			result.status = NFS3_OK;
		} else { // rename failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_NOENT:
				case NFS3ERR_IO:
				case NFS3ERR_ACCES:
				case NFS3ERR_EXIST:
				case NFS3ERR_XDEV:
				case NFS3ERR_NOTDIR:
				case NFS3ERR_ISDIR:
				case NFS3ERR_INVAL:
				case NFS3ERR_NOSPC:
				case NFS3ERR_ROFS:
				case NFS3ERR_MLINK:
				case NFS3ERR_NAMETOOLONG:
				case NFS3ERR_NOTEMPTY:
				case NFS3ERR_DQUOT:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_NOTSUPP:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *from_post_op = (result.status == NFS3_OK) ?
		&result.RENAME3res_u.resok.fromdir_wcc.after :
		&result.RENAME3res_u.resfail.fromdir_wcc.after;
	post_op_attr *to_post_op = (result.status == NFS3_OK) ?
		&result.RENAME3res_u.resok.todir_wcc.after :
		&result.RENAME3res_u.resfail.todir_wcc.after;
	get_post_op(from_post_op, args.from.dir);
	get_post_op(to_post_op, args.to.dir);
	dlog_end();
	return(&result);
}

LINK3res *
nfsproc3_link_3_svc(LINK3args args,  struct svc_req *rqstp) {
	dlog_begin("");
	static LINK3res result;
	result.status = NFS3ERR_NOTSUPP;
	dlog_end();
	return(&result);
}

READDIR3res *
nfsproc3_readdir_3_svc(READDIR3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle) %i %s", args.dir.data.data_val, (int)args.cookie, args.cookieverf);

	typedef char pathname[NAME_MAX];
	static uint32 timemask = (~(~0LL << (NFS3_COOKIEVERFSIZE << 2)));
	static pathname names[DIR_MAX_LEN];
	static entry3 entries[DIR_MAX_LEN];
	static READDIR3res result;
	
	// doing this first to use the attributes to verify cookie
	fattr3 dirattr = {};
	get_fattr(args.dir, &dirattr);

	bool newrequest = (args.cookie == 0) && (args.cookieverf == NULL || (strlen(args.cookieverf) == 0));
	bool cookie_valid = true;
	snprintf(result.READDIR3res_u.resok.cookieverf, NFS3_COOKIEVERFSIZE, "%x",
		dirattr.mtime.seconds & timemask);
	
	if ((newrequest == false) && (strcmp(args.cookieverf, result.READDIR3res_u.resok.cookieverf) != 0)) {
		cookie_valid = false;
	}
	
	if (cookie_valid) {
		uint64_t identifier = 0;
		int error = 0;
		const char *path = NULL;
		const kfsfilesystem_t *filesystem = get_filesystem(args.dir, &path, &identifier);
		if (filesystem) {
			dlog("\t%s (path)", path);
			kfscontents_t *contents = kfscontents_create();
			if (filesystem->readdir(path, contents, &error, filesystem->context)) {
				uint64_t cnt_i = 0;
				uint64_t ent_i = 0;
				uint64_t cnt_count = kfscontents_count(contents);
				uint64_t ent_count = (args.count > DIR_MAX_LEN) ? DIR_MAX_LEN : args.count;
				// start searching through contents at args.cookie (the requested index), and
				// iterate through until we've filled the entries or we've reached the end of the
				// directory listing.
				for (cnt_i = args.cookie; cnt_i < cnt_count && ent_i < ent_count; cnt_i++, ent_i++) {
					const char *entry = kfscontents_at(contents, cnt_i);
					strncpy(names[ent_i], entry, sizeof(pathname));
					
					uint64_t fileid = 0;
					char *fullpath = NULL;
					bool root = (strcmp(path, "/") == 0);
					asprintf(&fullpath, root ? "%s%s" : "%s/%s", path, entry);
					fileid = kfs_fileid(identifier, fullpath);
					free(fullpath);
					
					entries[ent_i].fileid = fileid;
					entries[ent_i].name = names[ent_i];
					entries[ent_i].cookie = cnt_i;
					entries[ent_i].nextentry = NULL;
					if (ent_i > 0) {
						entries[ent_i-1].nextentry = &entries[ent_i]; // set up linked list
					}
				}
				result.status = NFS3_OK;
				result.READDIR3res_u.resok.reply.entries = entries;
				result.READDIR3res_u.resok.reply.eof = (cnt_i == cnt_count);
			} else { // lsdir failed
				result.status = convert_status(error, NFS3ERR_NOTDIR);
				switch (result.status) {
					case NFS3_OK:
					case NFS3ERR_IO:
					case NFS3ERR_ACCES:
					case NFS3ERR_NOTDIR:
					case NFS3ERR_BAD_COOKIE:
					case NFS3ERR_TOOSMALL:
					case NFS3ERR_STALE:
					case NFS3ERR_BADHANDLE:
					case NFS3ERR_NOTSUPP:
					case NFS3ERR_SERVERFAULT:
						break;
					default:
						result.status = NFS3ERR_SERVERFAULT;
						break;
				}
			}
			kfscontents_destroy(contents);
		} else { // no filesystem
			result.status = NFS3ERR_BADHANDLE;
		}
	} else { // cookie invalid
		result.status = NFS3ERR_BAD_COOKIE;
	}

	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.READDIR3res_u.resok.dir_attributes :
		&result.READDIR3res_u.resfail.dir_attributes;
	get_post_op(post_op, args.dir);
	dlog_end();
	return(&result);
}

READDIRPLUS3res *
nfsproc3_readdirplus_3_svc(READDIRPLUS3args args,  struct svc_req *rqstp) {
	dlog_begin("");
	static READDIRPLUS3res result;
	result.status = NFS3ERR_NOTSUPP;
	dlog_end();
	return(&result);
}

FSSTAT3res *
nfsproc3_fsstat_3_svc(FSSTAT3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle)", args.fsroot.data.data_val);
	static FSSTAT3res result;
	int error = 0;
	const char *path = NULL;
	const kfsfilesystem_t *filesystem = get_filesystem(args.fsroot, &path, NULL);
	if (filesystem) {
		dlog("\t%s (path)", path);
		kfsstatfs_t sbuf = {};
		if (filesystem->statfs(path, &sbuf, &error, filesystem->context)) {
			result.status = NFS3_OK;
			result.FSSTAT3res_u.resok.tbytes = sbuf.size;
			result.FSSTAT3res_u.resok.fbytes = sbuf.free;
			result.FSSTAT3res_u.resok.abytes = sbuf.free;
			result.FSSTAT3res_u.resok.tfiles = 0;
			result.FSSTAT3res_u.resok.ffiles = 0;
			result.FSSTAT3res_u.resok.afiles = 0;
			result.FSSTAT3res_u.resok.invarsec = 0;
		} else { // statfs failed
			result.status = convert_status(error, NFS3ERR_IO);
			switch (result.status) {
				case NFS3_OK:
				case NFS3ERR_IO:
				case NFS3ERR_STALE:
				case NFS3ERR_BADHANDLE:
				case NFS3ERR_SERVERFAULT:
					break;
				default:
					result.status = NFS3ERR_SERVERFAULT;
					break;
			}
		}
	} else { // no filesystem
		result.status = NFS3ERR_BADHANDLE;
	}
	
	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.FSSTAT3res_u.resok.obj_attributes :
		&result.FSSTAT3res_u.resfail.obj_attributes;
	get_post_op(post_op, args.fsroot);
	dlog_end();
	return(&result);
}


FSINFO3res *
nfsproc3_fsinfo_3_svc(FSINFO3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle)", args.fsroot.data.data_val);
	static FSINFO3res result;
	result.status = NFS3_OK;
	result.FSINFO3res_u.resok.rtmax = READ_MAX_LEN;
	result.FSINFO3res_u.resok.rtpref = READ_MAX_LEN;
	result.FSINFO3res_u.resok.rtmult = 1;
	result.FSINFO3res_u.resok.wtmax = WRITE_MAX_LEN;
	result.FSINFO3res_u.resok.wtpref = WRITE_MAX_LEN;
	result.FSINFO3res_u.resok.wtmult = 1;
	result.FSINFO3res_u.resok.dtpref = DIR_MAX_LEN;
	result.FSINFO3res_u.resok.maxfilesize = UINT_MAX;
	result.FSINFO3res_u.resok.time_delta = (nfstime3){ 1, 0 };
	result.FSINFO3res_u.resok.properties = FSF3_HOMOGENEOUS | FSF3_SYMLINK | FSF3_CANSETTIME; /* FSF3_LINK */

	post_op_attr *post_op = (result.status == NFS3_OK) ?
		&result.FSINFO3res_u.resok.obj_attributes :
		&result.FSINFO3res_u.resfail.obj_attributes;
	get_post_op(post_op, args.fsroot);
	dlog_end();
	return(&result);
}

PATHCONF3res *
nfsproc3_pathconf_3_svc(PATHCONF3args args,  struct svc_req *rqstp) {
	dlog_begin("\t%s (handle)", args.object.data.data_val);
	static PATHCONF3res result;
	result.status = NFS3_OK;
	result.PATHCONF3res_u.resok.linkmax = LINK_MAX;
	result.PATHCONF3res_u.resok.name_max = NAME_MAX;
	result.PATHCONF3res_u.resok.no_trunc = true;
	result.PATHCONF3res_u.resok.chown_restricted = false;
	result.PATHCONF3res_u.resok.case_insensitive = true;
	result.PATHCONF3res_u.resok.case_preserving = true;
	dlog_end();
	return(&result);
}

COMMIT3res *
nfsproc3_commit_3_svc(COMMIT3args args,  struct svc_req *rqstp) {
	dlog_begin("");
	static COMMIT3res result;
	result.status = NFS3ERR_NOTSUPP;
	dlog_end();
	return(&result);
}

void *
mountproc3_null_3_svc(struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static char* result;
	return((void*) &result);
}

mountres3 *
mountproc3_mnt_3_svc(dirpath args,  struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static mountres3 result;
	result.fhs_status = MNT3ERR_NOTSUPP;
	return(&result);
}

mountlist *
mountproc3_dump_3_svc(struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static mountlist result;
	return(&result);
}

void *
mountproc3_umnt_3_svc(dirpath args,  struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static char* result;
	return((void*) &result);
}

void *
mountproc3_umntall_3_svc(struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static char* result;
	return((void*) &result);
}

exports *
mountproc3_export_3_svc(struct svc_req *rqstp) {
	// todo: it's possible that this needs to be implemented
	fprintf(stderr, "unexpected request: %s\n", __FUNCTION__);
	static exports result;
	return(&result);
}
