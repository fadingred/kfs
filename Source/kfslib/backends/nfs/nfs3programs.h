#ifndef _NFS_PROGRAMS_
#define _NFS_PROGRAMS_

#include "nfs3.h"

void nfs_program_3(struct svc_req *rqstp, SVCXPRT *transp);
void mount_program_3(struct svc_req *rqstp, SVCXPRT *transp);

#endif
