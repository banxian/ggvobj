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
        // TODO: may have multiple global/init_data located at same 0x0 address?!
        msg("found section [%s], banknumber:%d, baseaddress:%04X, length:%04X, segment count:%d\n", secname, secptr->banknumber, secptr->baseaddress, secptr->length, secptr->segmentscount);
        bool isram = secptr->baseaddress < 0x4000;
        unsigned recend = isram?0x4000:0xC000;
        int btn = askbuttons_c("~R~ecommend", "~S~mallest", "~O~riginal", 1, 
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
        segpos = 0;
        segptr -= secptr->segmentscount;
        for (int segindex = 0; segindex < secptr->segmentscount; segindex++, segptr++) {
            const char* segname = strtable + segptr->namedelta;
            //if (!segcreated) {
            //    // have data? 4040 + 8000 = C040?!
            //    segcreated = add_segm(0, secptr->baseaddress, secptr->baseaddress + secptr->length, segname, "CODE");
            //    if (secptr->baseaddress + secptr->length > extaddr) {
            //        extaddr = secptr->baseaddress + secptr->length;
            //    }
            //}
            add_segm(0, secptr->baseaddress + segpos, secptr->baseaddress + segpos + segptr->datasize, segname, isram?"RAM":"CODE");
            // mem2base
            //file2base(li, segrec->dataoffset, secres->baseaddress + segpos, secres->baseaddress + segpos + segrec->datasize, FILEREG_PATCHABLE);
            mem2base(objbuf + segptr->dataoffset, secptr->baseaddress + segpos, secptr->baseaddress + segpos + segptr->datasize, FILEREG_PATCHABLE);
            segpos += segptr->datasize; // append
            extcount += segptr->reloccount;
        }
        // if recend have extra space from segpos, 
        if (recend > secptr->baseaddress + segdatasize) {
            add_segm(0, secptr->baseaddress + segdatasize, recend, secname, isram?"RAM":"CODE");
        }
    }
    secptr -= header->sectioncount38;
    const labelrecord_s* label10ptr = (const labelrecord_s*)(objbuf + header->labeloffset10);
    const symbolrecord_s* lpsymtable20 = (const symbolrecord_s*)(objbuf + header->symboloffset20);
    for (int symindex = 0; symindex < header->symbolcount3A; symindex++, label10ptr++) {
        const symbolrecord_s* symrecex = &lpsymtable20[label10ptr->symbolindex];
        const char* symname = strtable + symrecex->namedelta;
        unsigned symea = secptr[symrecex->secindex].baseaddress + symrecex->delta;
        set_name(symea, symname, SN_CHECK | (symrecex->flag8 == 0x40?SN_PUBLIC:SN_NON_PUBLIC)); // TODO: local
        //rev20ptr[rev10ptr->quickoffset];//
        if (symrecex->flag8 == 0x40) {
            add_entry(symea, symea, symname, false);
        }
    }
    // Create undef?
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
                    fixup_data_t reloc;
                    reloc.type = FIXUP_OFF16;
                    reloc.off = target;
                    reloc.sel = 0; //
                    reloc.displacement = target - (segbegin + relocptr->reloc);
                    //set_fixup(segbegin + extptr->reloc, &reloc); // useless
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
