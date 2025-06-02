#ifndef HWDEFS_H_INCLUDED
#define HWDEFS_H_INCLUDED

//Address of parameter block in flash
#define FLASH_PAGE_SIZE 1024
#define PARAM_BLKSIZE FLASH_PAGE_SIZE
#define PARAM_BLKNUM  1   //last block of 1k
#define CAN1_BLKNUM   2

enum HwRev { HW_UNKNOWN, HW_1X, HW_20, HW_21, HW_22, HW_23, HW_24 };

extern HwRev hwRev;

#endif // HWDEFS_H_INCLUDED
