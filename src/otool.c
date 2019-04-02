#include "ofilep.h"


static void
hexdump (t_ofile *ofile, const t_object *object, const uint64_t offset, const uint64_t addr, const uint64_t size) {

    uint32_t    *ptr = (uint32_t *)(object->object + offset);
    const bool  dbyte = (object->nxArchInfo == NULL
            || (object->nxArchInfo->cputype != CPU_TYPE_I386 && object->nxArchInfo->cputype != CPU_TYPE_X86_64));

    for (uint64_t k = 0; k < size; k++) {

        if (k % 16 == 0) ft_dstrfpush(ofile->buffer, "%0*llx\t", (object->is_64 ? 16 : 8), addr + k);
        if (dbyte == true) {

            ft_dstrfpush(ofile->buffer, "%08x ", oswap_32(object, *(ptr + k / 4)));
            k += 3;
        } else {

            ft_dstrfpush(ofile->buffer, "%02x ", ((unsigned char *)ptr)[k]);
        }

        if (k % 16 == 15 || k + 1 == size) ft_dstrfpush(ofile->buffer, "\n");
    }
}

static int
segment (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    struct segment_command *segment = (struct segment_command *)opeek(object, offset, sizeof *segment);
    if (segment == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

    if (oswap_32(object, segment->fileoff) + oswap_32(object, segment->filesize) > object->size) {

        meta->errcode = E_SEGOFF;
        meta->command = oswap_32(object, segment->cmd);
        return EXIT_FAILURE;
    }

    offset += sizeof *segment;
    const uint32_t nsects = oswap_32(object, segment->nsects);
    for (uint32_t k = 0; k < nsects; k++) {

        const struct section *section = (struct section *)opeek(object, offset, sizeof *section);
        if (section == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

        if ((ft_strequ(section->segname, SEG_TEXT) && ft_strequ(section->sectname, SECT_TEXT) && ofile->opt & OTOOL_t)
        || (ft_strequ(section->segname, SEG_DATA) && ft_strequ(section->sectname, SECT_DATA) && ofile->opt & OTOOL_d)) {

            ft_dstrfpush(ofile->buffer, "Contents of (%s,%s) section\n", section->segname, section->sectname);
            hexdump(ofile, object, oswap_32(object, section->offset), oswap_32(object, section->addr),
                    oswap_32(object, section->size));
        }

        offset += sizeof *section;
    }

    return EXIT_SUCCESS;
}

static int
segment_64 (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    struct segment_command_64 *segment = (struct segment_command_64 *)(object->object + offset);
    if (segment == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

    if (oswap_64(object, segment->fileoff) + oswap_64(object, segment->filesize) > object->size) {

        meta->errcode = E_SEGOFF;
        meta->command = oswap_32(object, segment->cmd);
        return EXIT_FAILURE;
    }

    offset += sizeof *segment;
    const uint32_t nsects = oswap_32(object, segment->nsects);
    for (uint32_t k = 0; k < nsects; k++) {

        const struct section_64 *section = (struct section_64 *)opeek(object, offset, sizeof *section);
        if (section == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

        if ((ft_strequ(section->segname, SEG_TEXT) && ft_strequ(section->sectname, SECT_TEXT) && ofile->opt & OTOOL_t)
        || (ft_strequ(section->segname, SEG_DATA) && ft_strequ(section->sectname, SECT_DATA) && ofile->opt & OTOOL_d)) {

            ft_dstrfpush(ofile->buffer, "Contents of (%s,%s) section\n", section->segname, section->sectname);
            hexdump(ofile, object, oswap_32(object, section->offset), oswap_64(object, section->addr),
                    oswap_64(object, section->size));
        }

        offset += sizeof *section;
    }

    return EXIT_SUCCESS;
}

static int
symtab_check (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    /* This function merely serves to check corrupted files. We won't output or need anything from it. */

    (void)ofile, (void)meta;
    const struct symtab_command *symtab = (struct symtab_command *)opeek(object, offset, sizeof *symtab);
    if (symtab == NULL) return EXIT_FAILURE; /* E_RRNO */

    const uint32_t stroff = oswap_32(object, symtab->stroff);
    const uint32_t strsize = oswap_32(object, symtab->strsize);
    offset = oswap_32(object, symtab->symoff);

    for (meta->u_k.k_strindex = 0; meta->u_k.k_strindex < oswap_32(object, symtab->nsyms); meta->u_k.k_strindex++) {

        const struct nlist *nlist = (struct nlist *)opeek(object, offset, sizeof *nlist);
        if (nlist == NULL) return EXIT_FAILURE; /* E_RRNO */

        const uint32_t n_strx = oswap_32(object, nlist->n_un.n_strx);
        if (stroff + n_strx > object->size) {

            meta->errcode = E_SYMSTRX;
            meta->u_n.n_strindex = (int)(stroff + strsize + n_strx - ofile->size);
            return EXIT_FAILURE;
        }

        offset += (object->is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist));
    }

    return EXIT_SUCCESS;
}

int
main (int argc, const char *argv[]) {

    int             index = 1, retcode = EXIT_SUCCESS;
    static t_dstr   buffer;
    static t_meta   meta = {
            .obin = FT_OTOOL,
            .reader = {
                    [LC_SEGMENT] = segment,
                    [LC_SEGMENT_64] = segment_64,
                    [LC_SYMTAB] = symtab_check
            }
    };
    t_ofile         ofile = {
            .arch = NULL,
            .buffer = &buffer,
            .opt = NAME_OUTPUT
    };
    const t_opt     opts[] = {
            {FT_OPT_BOOLEAN, 'd', "data", &ofile.opt, "Display the contents of the (__DATA, __data) section.", OTOOL_d},
            {FT_OPT_BOOLEAN, 'h', "header", &ofile.opt, "Display the Mach header.", OTOOL_h},
            {FT_OPT_BOOLEAN, 't', "text", &ofile.opt, "Display the contents of the (__TEXT,__text) section.", OTOOL_t},
            {FT_OPT_STRING, 0, "arch", &ofile.arch, "Specifies the architecture of the file to display when the file "
                "is a fat binary. \"all\" can be specified to display all architectures in the file. The default is to "
                "display only the host architecture.", 0},
            {FT_OPT_END, 0, 0, 0, 0, 0}
    };

    if (ft_optparse(opts, &index, argc, (char **)argv)) {

        ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
        return EXIT_FAILURE;
    };

    if (ofile.arch && ft_strequ(ofile.arch, "all") == 0 && NXGetArchInfoFromName(ofile.arch) == NULL) {

        ft_fprintf(stderr, "%1$s: unknown architecture specification flag: --arch %2$s\n%1$s: known architecture flags"
                " are:", argv[0], ofile.arch);
        const NXArchInfo *nxArchInfo = NXGetAllArchInfos();

        for (int k = 0; nxArchInfo[k].name != NULL; k++) ft_fprintf(stderr, " %s", nxArchInfo[k].name);

        ft_fprintf(stderr, "\n");
        ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
        return EXIT_FAILURE;
    }
    if (ofile.opt < OTOOL_d) return ft_fprintf(stderr, "%s: one of -dht must be specified.\n", argv[0]), EXIT_FAILURE;
    if (argc == index) argv[argc++] = "a.out";

    meta.bin = argv[0];
    for ( ; index < argc; index++) {

        meta.path = argv[index];
        meta.errcode = E_RRNO;
        meta.type = E_MACHO;
        if (open_file(&ofile, &meta) != EXIT_SUCCESS) {

            printerr(&meta);
            retcode = EXIT_FAILURE;

            if (meta.errcode == E_RRNO) break;
        }
    }

    return retcode;
}
