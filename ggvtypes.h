#ifndef _GGV_TYPES_H
#define _GGV_TYPES_H

#include <stdint.h>

struct objheader_s
{
    unsigned int rev0; // 00 00 CD CD
    unsigned int rev4; // 00 00 00 00
    uint32_t objsize8;
    uint32_t sectionrecoffsetC; // always48
    uint32_t expoffset10;
    unsigned int rev14;
    uint32_t stringtableoffset18;
    uint32_t offset1C; // other table
    uint32_t symboloffset20; // both ext/local
    unsigned int rev24;
    uint32_t somesize28;
    unsigned int rev2C;
    unsigned int rev30;
    uint32_t segheadoffset34; // redundancy?
    uint16_t sectioncount38;
    uint16_t exportcount3A;
    uint16_t rev3C;
    uint16_t allsrcinfosize3E;
    uint16_t symbolcount40; // contains label/data/func/segmentname
    uint16_t rev42;
    uint16_t sourcecount44; // include c/asm/h
    uint16_t rev46; // CD CD
};

struct sectionrecord_s
{
    uint16_t baseaddress; // 4040
    uint16_t length; // 8000
    uint16_t banknumber;
    uint16_t rev6; // CD CD
    uint32_t namedelta; // of section
    uint32_t segmentsoffset; // of file
    uint16_t segmentscount; // + 10
    uint16_t rev12; // CD CD
};

struct segmentrecord_s
{
    uint16_t rev0;          // CD CD
    uint16_t datasize;      // +2
    uint32_t namedelta;     // +4
    uint32_t dataoffset;    // +8, of file
    uint32_t relocoffset;  // +C, of file, may ext or local
    uint16_t reloccount;// +10, ext symbol count?
    uint16_t rev12;         // CD CD
};

struct relocrecord_s
{
    uint32_t flag0;
    uint16_t type;
    uint16_t refdelta;  // of segment
    uint16_t extlink;   // in export symbol
    uint16_t revA;
    uint32_t revC;      // TODO: lib?
};

struct symbolrecord_s
{
    uint32_t namedelta;
    uint16_t rev4;
    uint16_t rev6;
    uint16_t flag8; // function|data?
    uint16_t secindex;
    uint16_t segindex;
    uint16_t delta; // in seg
};

struct exportrecord_s {
    uint32_t namedelta;
    uint32_t symbolindex;
};

struct sourcerecord_s {
    uint32_t namedelta;
    uint32_t rev4;
};


#endif