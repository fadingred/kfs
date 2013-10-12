//
//  kfsfileid.h
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

#import "kfslib.h"

/*!
 \brief		Get a fileid from a path
 \details	Gets a unique file id from the given path.
 */
uint64_t kfs_fileid(kfsid_t filesystem, const char *path);

/*!
 \brief		Swap file ids
 \details	Swap the paths that the ids map to.
 */
void kfs_idswap(kfsid_t filesystem, uint64_t id_one, uint64_t id_two);

/*!
 \brief		Gets a path from a file id
 \details	Gets a path given a file id. The path needs to have
			been registered with the system via the kfs_fileid
			call before this will return anything. Returns NULL
			when a path can't be found.
 */
const char *path_fromid(kfsid_t filesystem, uint64_t fileid);

/*!
 \brief		Clear all ids for the filesystem
 \details	Remove all ids for the filesystem (useful to reclaim
			memory on unmount of a filesystem)
 */
void kfs_idclear(kfsid_t filesystem);
