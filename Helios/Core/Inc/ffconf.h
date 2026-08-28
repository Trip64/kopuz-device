/*----------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.12c                                     /
/  Configuration file for Kopuz Embedded Audio Player                         /
/----------------------------------------------------------------------------*/

#define _FFCONF 68300   /* Revision ID - must match ff.h */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define _FS_READONLY    1   /* Read-only safe for user SD card */
#define _FS_MINIMIZE    0   /* Full directory and file functions */
#define _USE_STRFUNC    0
#define _USE_FIND       0
#define _USE_MKFS       0
#define _USE_FASTSEEK   1
#define _USE_EXPAND     0
#define _USE_CHMOD      0
#define _USE_LABEL      0
#define _USE_FORWARD    0

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define _CODE_PAGE      437     /* US English / Standard OEM */
#define _USE_LFN        1       /* Long File Names (LFN) enabled with static buffer */
#define _MAX_LFN        255
#define _LFN_UNICODE    0
#define _STRF_ENCODE    3
#define _FS_RPATH       0

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define _VOLUMES        1
#define _STR_VOLUME_ID  0
#define _MULTI_PARTITION 0

/*---------------------------------------------------------------------------/
/ Physical Drive Configurations
/---------------------------------------------------------------------------*/

#define _MIN_SS         512
#define _MAX_SS         512
#define _USE_TRIM       0

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define _FS_TINY        0       /* Dedicated sector buffer for speed and reliability */
#define _FS_EXFAT       1       /* Enable exFAT support for modern 64GB+ MicroSD cards */
#define _FS_NORTC       1
#define _NORTC_MON      1
#define _NORTC_MDAY     1
#define _NORTC_YEAR     2024
#define _FS_NOFSINFO    0
#define _FS_LOCK        0
#define _FS_REENTRANT   0
#define _FS_TIMEOUT     1000
#define _SYNC_t         void*
