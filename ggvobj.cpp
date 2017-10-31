#include "../idaldr.h"
#include "ggvtypes.h"

int idaapi accept_file(linput_t *li, char fileformatname[MAX_FILE_FORMAT_NAME], int n)
{
    if ( n ) {
        return 0;
    }
    size_t filesize = qlsize(li);
    if (filesize > sizeof(objheader_s)) {
        objheader_s head;
        qlread(li, &head, sizeof(head));
        if (filesize == head.objsize8) {
            qstrncpy(fileformatname, "GGV WaveSource 6502", MAX_FILE_FORMAT_NAME);
            //set_processor_type("m6502", SETPROC_ALL|SETPROC_FATAL);
            return 1;
        }
    }
    return 0;
}

typedef std::map<uint32_t, uint32_t> DualIntMap;
typedef qvector<uint16_t> UShortVec;

void idaapi load_file(linput_t *li, ushort neflag, const char * /*fileformatname*/)
{
    size_t filesize = qlsize(li);

    const char* objbuf = (const char*)malloc(filesize);
    qlread(li, (void*)objbuf, filesize); // no need qlseek

    set_processor_type("m6502", SETPROC_ALL|SETPROC_FATAL);

    const objheader_s* header = (const objheader_s*)objbuf;

    const char* strtable = objbuf + header->stringtableoffset18;
    const sectionrecord_s* secptr = (const sectionrecord_s*)(objbuf + header->sectionrecoffsetC);
    int extcount = 0;
    unsigned int extaddr = 0;
    DualIntMap AddrSizeMap;
    DualIntMap AddrEndMap;
    UShortVec SecBaseVec;
    for (int sectionindex = 0; sectionindex < header->sectioncount38; sectionindex++, secptr++) {
        const char* secname = strtable + secptr->namedelta;
        bool segcreated = false;
        const segmentrecord_s* segptr = (const segmentrecord_s*)(objbuf + secptr->segmentsoffset);
        int segpos = 0;
        // first loop, calculate real section data size?
        for (int segindex = 0; segindex < secptr->segmentscount; segindex++, segptr++) {
            segpos += segptr->datasize; // append
        }
        int segdatasize = segpos;
        // keyis bank+address?
        uint32_t key = secptr->banknumber << 16 | secptr->baseaddress;
        // TODO: may have multiple global/init_data located at same 0x0 address?!
        msg("found section [%s], banknumber:%d, baseaddress:%04X, length:%04X, segment count:%d\n", secname, secptr->banknumber, secptr->baseaddress, secptr->length, secptr->segmentscount);
        DualIntMap::iterator it = AddrSizeMap.find(key);
        if (it != AddrSizeMap.end()) {
            msg("found previous section with same baseaddress and banknumber. current used space in section is %04X\n", it->second);
            segdatasize += it->second;
            SecBaseVec.push_back(secptr->baseaddress + it->second);
        } else {
            AddrSizeMap[key] = segdatasize;
            SecBaseVec.push_back(secptr->baseaddress);
        }
        bool isram = secptr->baseaddress < 0x4000;
        unsigned recend = isram?0x4000:0xC000;
        int btn = askbuttons_c("~R~ecommend", "~S~mallest", "~O~riginal", ASKBTN_YES, 
            "The section [%s] have original address range %04X to %04X, real segments filled up to %04X, Recommend end address is %04X\n", 
            secname,
            secptr->baseaddress,
            secptr->baseaddress + secptr->length,
            secptr->baseaddress + segdatasize,
            recend);
        switch (btn) {
        case ASKBTN_NO:
            recend = secptr->baseaddress + segdatasize;
            break;
        case ASKBTN_CANCEL:
            recend = secptr->baseaddress + secptr->baseaddress;
            break;
        }
        // TODO: extern have different bank to prevent conflict?
        if (recend > extaddr) {
            extaddr = recend;
        }
        AddrEndMap[key] = recend;
    }
    secptr -= header->sectioncount38;
    DualIntMap SecPosMap; // for combined sections, record previous segpos
    DualIntMap SegBaseMap; // for symbolrecord sec/seg lookup
    for (int sectionindex = 0; sectionindex < header->sectioncount38; sectionindex++, secptr++) {
        const char* secname = strtable + secptr->namedelta;
        uint32_t key = secptr->banknumber << 16 | secptr->baseaddress;
        DualIntMap::iterator posit = SecPosMap.find(key);
        bool isram = secptr->baseaddress < 0x4000;
        const segmentrecord_s* segptr = (const segmentrecord_s*)(objbuf + secptr->segmentsoffset);
        uint32_t segpos = (posit == SecPosMap.end())?0:posit->second;
        for (int segindex = 0; segindex < secptr->segmentscount; segindex++, segptr++) {
            const char* segname = strtable + segptr->namedelta;
            add_segm(0, secptr->baseaddress + segpos, secptr->baseaddress + segpos + segptr->datasize, segname, isram?"RAM":"CODE");
            //file2base(li, segrec->dataoffset, secres->baseaddress + segpos, secres->baseaddress + segpos + segrec->datasize, FILEREG_PATCHABLE);
            mem2base(objbuf + segptr->dataoffset, secptr->baseaddress + segpos, secptr->baseaddress + segpos + segptr->datasize, FILEREG_PATCHABLE);
            uint32_t secsegkey = sectionindex << 16 | segindex;
            SegBaseMap.insert(std::make_pair(secsegkey, secptr->baseaddress + segpos)) ;// 
            segpos += segptr->datasize; // append
            extcount += segptr->reloccount;
        }
        SecPosMap[key] = segpos;
        uint32_t recend = AddrEndMap[key];
        // if recend have extra space from segpos, 
        if (recend > secptr->baseaddress + segpos) {
            add_segm(0, secptr->baseaddress + segpos, recend, secname, isram?"RAM":"CODE");
        }
    }
    // Create names
    secptr -= header->sectioncount38;
    const exportrecord_s* label10ptr = (const exportrecord_s*)(objbuf + header->expoffset10);
    const symbolrecord_s* lpsymtable20 = (const symbolrecord_s*)(objbuf + header->symboloffset20);
    for (int expindex = 0; expindex < header->exportcount3A; expindex++, label10ptr++) {
        const symbolrecord_s* symrecex = &lpsymtable20[label10ptr->symbolindex];
        const char* symname = strtable + symrecex->namedelta;
        uint32_t secsegkey = symrecex->secindex << 16 | symrecex->segindex;
        unsigned symea = SegBaseMap[secsegkey] + symrecex->delta;
        set_name(symea, symname, SN_CHECK | SN_PUBLIC); // TODO: restore local label from reloc record?
        //rev20ptr[rev10ptr->quickoffset];//
        add_entry(symea, symea, symname, false);
        if (symrecex->flag8 == 0x40) {
            // try make function?
            add_func(symea, BADADDR);
        }
    }
    // Create undef and do reloc?
    int extpos = 0;
    if (extcount) {
        add_segm(0, extaddr, extaddr + extcount * 2, "UNDEF", "XTRN"); // ?? ??
        for (int sectionindex = 0; sectionindex < header->sectioncount38; sectionindex++, secptr++) {
            const segmentrecord_s* segptr = (const segmentrecord_s*)(objbuf + secptr->segmentsoffset);
            int segpos = 0;
            for (int segindex = 0; segindex < secptr->segmentscount; segindex++, segptr++) {
                unsigned int segbegin = secptr->baseaddress + segpos;
                const relocrecord_s* relocptr = (const relocrecord_s*)(objbuf + segptr->relocoffset);
                for (int relocindex = 0; relocindex < segptr->reloccount; relocindex++, relocptr++) {
                    const char* extname = strtable + lpsymtable20[relocptr->extlink].namedelta;
                    // TODO: check exists use map?
                    unsigned target = get_name_ea(BADADDR, extname);//extaddr + extpos;
                    if (target == BADADDR) {
                        if (set_name(extaddr + extpos, extname, SN_CHECK | SN_PUBLIC)) {
                            target = extaddr + extpos;
                        } else  {
                            msg("create extern name \"%s\" failed!\n", extname);
                        }
                        extpos += 2; // leave empty entry
                    }
                    put_word(segbegin + relocptr->reloc, target); // useful
                    fixup_data_t fix;
                    fix.type = target >= extaddr? FIXUP_OFF16 | FIXUP_EXTDEF:FIXUP_OFF16;
                    fix.sel = 0; //
                    fix.off = target;
                    fix.displacement = 0; // target - (segbegin + relocptr->reloc);
                    set_fixup(segbegin + relocptr->reloc, &fix); // useless
                    msg("set relocation for %04X to %04X (%s)\n", segbegin + relocptr->reloc, target, extname);
                }
                segpos += segptr->datasize; // append
            }
        }

    }

    free((void*)objbuf);
}

loader_t LDSC =
{
    IDP_INTERFACE_VERSION,
    0,                            // loader flags
    //
    //      check input file format. if recognized, then return 1
    //      and fill 'fileformatname'.
    //      otherwise return 0
    //
    accept_file,
    //
    //      load file into the database.
    //
    load_file,
    //
    //      create output file from the database.
    //      this function may be absent.
    //
    NULL,
};
