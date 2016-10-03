#ifndef __FSLDR_H
#define __FSLDR_H

#include <3ds/types.h>

Result fsldrInit(void);
void   fsldrExit(void);

Result FSLDR_Initialize(Handle session);
Result FSLDR_SetPriority(u32 priority);
Result FSLDR_OpenFileDirectly(Handle *out, FS_ArchiveID archiveId, FS_Path archivePath, FS_Path filePath, u32 openFlags, u32 attributes);
Result FSLDR_GetNandCid(u8* out, u32 length);

#endif
