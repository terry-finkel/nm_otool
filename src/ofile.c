#include "ofilep.h"
#include <ar.h>
#include <fcntl.h>
#include <mach-o/fat.h>
#include <mach-o/ranlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ERR_XTEND "extends past the end of the file)\n"
#define SAR_EFMT1 3
#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)

static int              dispatch(t_ofile * ofile, t_object *object, t_meta *meta);

static const char       *errors[] = {
        [E_RRNO] = "truncated or malformed",
        [E_GARBAGE] = "is not an object file",
        [E_MAGIC] = "invalid magic",
        [E_INV4L] = "cmdsize not a multiple of",
        [E_ARFMAG] = "terminator characters in archive member not the correct",
        [E_AROFFSET] = "offset to next archive member past the end of the archive after member",
        [E_AROVERLAP] = "cputype (0) cpusubtype (0) offset 0 overlaps universal headers",
        [E_LOADOFF] = "extends past the end all load commands in the file",
        [E_SEGOFF] = "fileoff field plus filesize field",
        [E_FATOFF] = "offset plus size of",
        [E_SYMSTRX] = "bad string table index"
};

static const size_t     header_size[] = {
        [false] = sizeof(struct mach_header),
        [true] = sizeof(struct mach_header_64)
};

static const char       *filecodes[] = {
        [E_MACHO] = "object",
        [E_FAT] = "fat file",
        [E_AR] = "archive"
};

static const char       *segcodes[] = {
        [LC_SEGMENT] = "LC_SEGMENT",
        [LC_SEGMENT_64] = "LC_SEGMENT_64"
};


int
printerr (const t_meta *meta) {

    if (meta->errcode == E_RRNO) {

        ft_fprintf(stderr, "%s: \'%s\': %s\n", meta->bin, meta->path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (meta->errcode < E_INV4L) {

        ft_fprintf(stderr, "%s: %s\n", meta->path, errors[meta->errcode]);
        return EXIT_FAILURE;
    }

    ft_fprintf(stderr, "%s: \'%s\': %s %s ", meta->bin, meta->path, errors[0], filecodes[meta->type]);
    switch (meta->errcode) {
        case E_INV4L:
            ft_fprintf(stderr, "(load command %u %s %d)\n", meta->k_command, errors[meta->errcode], meta->arch ? 8 : 4);
            break;
        case E_ARFMAG:
            ft_fprintf(stderr, "(%s %s values for the archive member header for %s)\n", errors[meta->errcode],
                    STR(ARFMAG), meta->ar_member);
            break;
        case E_AROFFSET:
            ft_fprintf(stderr, "(%s %s)\n", errors[meta->errcode], meta->ar_member);
            break;
        case E_AROVERLAP:
            ft_fprintf(stderr, "(%s)\n", errors[meta->errcode]);
            break;
        case E_LOADOFF:
            ft_fprintf(stderr, "(load command %u %s)\n", meta->k_command + 1, errors[meta->errcode]);
            break;
        case E_SEGOFF:
            ft_fprintf(stderr, "(load command %u %s in %s %s", meta->k_command, errors[meta->errcode],
                    segcodes[meta->command], ERR_XTEND);
            break;
        case E_FATOFF:
            ft_fprintf(stderr, "(%s cputype (%d) cpusubtype (%d) %s", errors[meta->errcode], meta->u_n.n_cpu,
                    meta->u_k.k_cpu, ERR_XTEND);
            break;
        case E_SYMSTRX:
        default:
            ft_fprintf(stderr, "(%s: %d past the end of string table, for symbol at index %u)\n",
                    errors[meta->errcode], meta->u_n.n_strindex, meta->u_k.k_strindex);
    }

    /* Because nm is so fancy, it prints an addional newline for some reason. */
    if (meta->obin == FT_NM) ft_fprintf(stderr, "\n");
    
    return EXIT_FAILURE;
}

static void
header_dump (t_ofile *ofile, t_object *object) {

    const struct mach_header    *header = (struct mach_header *)object->object;
    const uint32_t              magic = oswap_32(object, header->magic);
    const uint32_t              cputype = oswap_32(object, (uint32_t)header->cputype);
    const uint32_t              cpusubtype = oswap_32(object, (uint32_t)header->cpusubtype) & ~CPU_SUBTYPE_MASK;
    const bool                  caps = (oswap_32(object, (uint32_t)header->cpusubtype) & CPU_SUBTYPE_MASK) != 0;
    const uint32_t              filetype = oswap_32(object, header->filetype);
    const uint32_t              ncmds = oswap_32(object, header->ncmds);
    const uint32_t              sizeofcmds = oswap_32(object, header->sizeofcmds);
    const uint32_t              flags = oswap_32(object, header->flags);

    ft_dstrfpush(ofile->buffer, "Mach header\n");
    ft_dstrfpush(ofile->buffer, "      magic cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags\n");
    ft_dstrfpush(ofile->buffer, "%11#x %7d %10d %5.2#p %11u %5u %10u %#.8x\n", magic, cputype, cpusubtype,
            (caps ? 128 : 0), filetype, ncmds, sizeofcmds, flags);
}

static int
read_macho_file (t_ofile *ofile, t_object *object, t_meta *meta) {

    struct mach_header *header = (struct mach_header *)opeek(object, 0, sizeof *header);
    if (header == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

    uint32_t ncmds = oswap_32(object, header->ncmds);
    size_t offset = header_size[object->is_64];

    /* If the object isn't an archive or fat, retrieve the architecture. */
    if (object->nxArchInfo == NULL) object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object,
            (uint32_t)header->cputype), (cpu_subtype_t)oswap_32(object, (uint32_t)header->cpusubtype));

    /* Output (or not) the name of the file or of the archive / fat. */
    if (meta->obin == FT_NM || ofile->opt & OTOOL_d || ofile->opt & OTOOL_t) {

        if (meta->obin == FT_NM && (ofile->opt & NAME_OUTPUT || ofile->opt & ARCH_OUTPUT || meta->type == E_AR))
            ft_dstrfpush(ofile->buffer, "\n");

        if (meta->type == E_AR) {

            ft_dstrfpush(ofile->buffer, "%s(%s):\n", meta->path, object->name);
        } else {

            /* Weird conditions to match the outputs of both nm and otool. */
            if (ofile->opt & NAME_OUTPUT || ofile->opt & ARCH_OUTPUT) ft_dstrfpush(ofile->buffer, "%s", object->name);
            if (ofile->opt & ARCH_OUTPUT) ft_dstrfpush(ofile->buffer, " (%sarchitecture %s)",
                    (meta->obin == FT_NM) ? "for " : "", ofile->arch);
            if (ofile->opt & NAME_OUTPUT || ofile->opt & ARCH_OUTPUT) ft_dstrfpush(ofile->buffer, ":\n");
        }
    }

    /* Initialize our section iterator for nm. Starts at 1 as we take into account LC_SYMTAB. */
    object->k_sect = 1;

    /*
       Let's go though the commands. We will save the LC_SYMTAB for later as we first need to save the section numbers
       for nm, and the error checks in LC_SYMTAB are performed after the ones in sections and segments.
    */

    size_t symtab_offset = 0;
    for (meta->k_command = 0; meta->k_command < ncmds; meta->k_command++) {

        const struct load_command *loader = (struct load_command *)opeek(object, offset, sizeof *loader);
        if (loader == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

        meta->command = oswap_32(object, loader->cmd);
        const uint32_t cmdsize = oswap_32(object, loader->cmdsize);

        /* Various command loader checks. */
        if (offset + cmdsize > object->size) return (meta->errcode = E_LOADOFF), EXIT_FAILURE;
        if (cmdsize % (object->is_64 ? 8 : 4)) {

            meta->arch = object->is_64;
            meta->errcode = E_INV4L;
            return EXIT_FAILURE;
        }
        if (cmdsize == 0) {

            meta->k_command -= 1;
            meta->errcode = E_LOADOFF;
            return EXIT_FAILURE;
        }

        /* Save the offset of LC_SYMTAB to use it later. */
        if (meta->command == LC_SYMTAB) symtab_offset = offset;

        /* We run through the segments (and only the segments) first. */
        else if (meta->command <= LC_SEGMENT_64 && meta->reader[meta->command]
        && meta->reader[meta->command](ofile, object, meta, offset) != EXIT_SUCCESS) return EXIT_FAILURE;

        offset += oswap_32(object, loader->cmdsize);
    }

    /* Go through LC_SYMTAB. For otool, and only if -t or -d is specified, this will prevent dumping a corrupted file. */
    if (symtab_offset && (meta->obin == FT_NM || ofile->opt & OTOOL_d || ofile->opt & OTOOL_t)
    && meta->reader[LC_SYMTAB](ofile, object, meta, symtab_offset) != EXIT_SUCCESS) return EXIT_FAILURE;

    if (ofile->opt & OTOOL_h) header_dump(ofile, object);

    /* In some cases NXArchInfo will be malloc (arch (3)), free it to prevent leaks. */
    NXFreeArchInfo(object->nxArchInfo);

    /* Output object and clear buffer. */
    ft_fprintf(stdout, ofile->buffer->buff);
    ft_dstrclr(ofile->buffer);
    return EXIT_SUCCESS;
}

static int
process_archive (t_ofile *ofile, t_object *object, t_meta *meta, size_t *offset) {

    int retcode = EXIT_SUCCESS;
    const struct ar_hdr *ar_hdr = (struct ar_hdr *)opeek(object, *offset, sizeof *ar_hdr);
    if (ar_hdr == NULL) return (meta->errcode = E_AROFFSET), EXIT_FAILURE;

    /* Consistency check. If it fails, don't return immediately so we can save the archive name to display it later. */
    if (ft_strnequ(ARFMAG, ar_hdr->ar_fmag, 2) == 0) {

        meta->errcode = E_ARFMAG;
        retcode = EXIT_FAILURE;
    }

    const int size = ft_atoi(ar_hdr->ar_size);
    if (size < 0) return EXIT_FAILURE; /* E_RRNO */

    /* Populate our object. */
    *offset += sizeof *ar_hdr;
    object->size = (size_t)size;
    int name_size = 0;
    if (ft_strnequ(ar_hdr->ar_name, AR_EFMT1, SAR_EFMT1)) {

        name_size = ft_atoi(ar_hdr->ar_name + SAR_EFMT1);
        if (name_size < 0) return EXIT_FAILURE; /* E_RRNO */

        object->object = ofile->file + *offset + name_size;
        object->name = ofile->file + *offset;
    } else {

        object->object = ofile->file + *offset;
        object->name = ar_hdr->ar_name;
    }

    /* Adujst size if name is EFMT1 */
    if (*offset + object->size > ofile->size) {

        meta->errcode = E_AROFFSET;
        retcode = EXIT_FAILURE;
    }

    *offset += object->size;
    object->size -= (size_t)name_size;
    meta->ar_member = ft_strdup(object->name);
    return retcode;
}

static int
read_archive (t_ofile *ofile, t_object *object, t_meta *meta) {

    const void *restore = ofile->file;
    ofile->file = object->object;
    size_t offset = SARMAG;

    if (meta->type != E_FAT) meta->type = E_AR;
    if (process_archive(ofile, object, meta, &offset) != EXIT_SUCCESS) return EXIT_FAILURE;

    /* Check SYMDEF validity. */
    const char *symdef = ofile->file + sizeof(struct ar_hdr) + SARMAG;
    if (ft_strequ(symdef, SYMDEF) == 0 && ft_strequ(symdef, SYMDEF_SORTED) == 0 && ft_strequ(symdef, SYMDEF_64) == 0
        && ft_strequ(symdef, SYMDEF_64_SORTED) == 0) return EXIT_FAILURE; /* E_RRNO */

    /* Archive looks valid so far, let's loop through each of it's member. */

    if (meta->obin == FT_OTOOL) ft_dstrfpush(ofile->buffer, "Archive : %s\n", meta->path);

    while (offset != ofile->size) {

        /* Restore full object. */
        object->object = ofile->file;
        object->size = ofile->size;

        if (process_archive(ofile, object, meta, &offset) != EXIT_SUCCESS) return EXIT_FAILURE;
        if (dispatch(ofile, object, meta) != EXIT_SUCCESS) return EXIT_FAILURE;
    }

    ofile->file = restore;
    return EXIT_SUCCESS;
}

static int
test_offset_fat_arch (t_ofile *ofile, const t_object *object, t_meta *meta, const void *ptr) {

    const uint64_t offset = (object->is_64)
            ? oswap_64(object, ((struct fat_arch_64 *)ptr)->offset)
            : oswap_32(object, ((struct fat_arch *)ptr)->offset);

    const uint64_t size = (object->is_64)
            ? oswap_64(object, ((struct fat_arch_64 *)ptr)->size)
            : oswap_32(object, ((struct fat_arch *)ptr)->size);

    if (offset + size > ofile->size) {

        meta->u_n.n_cpu = object->nxArchInfo->cputype;
        meta->u_k.k_cpu = object->nxArchInfo->cpusubtype;
        meta->errcode = E_FATOFF;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
dispatch_fat (t_ofile *ofile, t_object *object, t_meta *meta, const void *ptr) {

    if (object->is_64 == false) {

        const struct fat_arch *arch = (struct fat_arch *)ptr;
        object->object = ofile->file + oswap_32(object, arch->offset);
        object->size = oswap_32(object, arch->size);
    } else {

        const struct fat_arch_64 *arch_64 = (struct fat_arch_64 *)ptr;
        object->object = ofile->file + oswap_64(object, arch_64->offset);
        object->size = oswap_64(object, arch_64->size);
    }

    /*
       Endianness and 64 will be overriden by dispatch as the object will be treated anew, we save them to restore
       them after the object has been processed.
    */

    const bool fat_is_64 = object->is_64;
    const bool fat_is_cigam = object->is_cigam;
    const int retcode = dispatch(ofile, object, meta);
    object->is_64 = fat_is_64;
    object->is_cigam = fat_is_cigam;
    object->object = ofile->file;

    return retcode;
}

static int
read_fat_file (t_ofile *ofile, t_object *object, t_meta *meta) {

    meta->type = E_FAT;
    struct fat_header *fat_header = (struct fat_header *)opeek(object, 0, sizeof *fat_header);
    if (fat_header == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

    size_t offset = sizeof *fat_header;
    const uint32_t nfat_arch = oswap_32(object, fat_header->nfat_arch);

    /*
       If the "all" architecture flag hasn't been specified, we first iterate through every available architecture in
       the fat object in order to find one that matches the specified one if given, or the host one if not.
    */

    /* If the arch hasn't been specified, we look for a x86_64. */
    bool no_specific = (ofile->arch == NULL && (ofile->arch = "x86_64"));
    if (no_specific || ft_strequ(ofile->arch, "all") == 0) {

        for (uint32_t k = 0; k < nfat_arch; k++) {

            const struct fat_arch *fat_arch = (struct fat_arch *)opeek(object, offset, sizeof *fat_arch);
            if (fat_arch == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

            object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
                    (cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

            if (object->nxArchInfo == NULL) return (meta->errcode = E_AROVERLAP), EXIT_FAILURE;
            if (test_offset_fat_arch(ofile, object, meta, fat_arch) != EXIT_SUCCESS) return EXIT_FAILURE;
            if (ft_strequ(ofile->arch, object->nxArchInfo->name)) return dispatch_fat(ofile, object, meta, fat_arch);

            offset += object->is_64 ? sizeof(struct fat_arch_64) : sizeof(struct fat_arch);
        }

        /* The architecture hasn't been found. If no specific arch was mentioned, dump everything. */
        if (no_specific == false) {

            ft_printf("%s: file: %s does not contain architecture: %s.\n", meta->bin, meta->path, ofile->arch);
            return EXIT_SUCCESS;
        }

        offset = sizeof *fat_header;
    }

    /*
       If this code is reached, either "all" was specified, or the host architecture wasn't found in the fat file.
       Either way, we can dump everything.
    */

    ofile->opt |= ARCH_OUTPUT;
    for (uint32_t k = 0; k < oswap_32(object, fat_header->nfat_arch); k++) {

        /* Mutate object architecture specifications to treat it as an independent Mach-O file. */
        const struct fat_arch *fat_arch = (struct fat_arch *)opeek(object, offset, sizeof *fat_arch);
        if (fat_arch == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

        object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
                (cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

        if (object->nxArchInfo == NULL) return (meta->errcode = E_AROVERLAP), EXIT_FAILURE;
        if (test_offset_fat_arch(ofile, object, meta, fat_arch) != EXIT_SUCCESS) return EXIT_FAILURE;

        ofile->arch = object->nxArchInfo->name;

        /* In case of an error, nm displays the error but keeps dumping. otool terminates immediately. */
        if (dispatch_fat(ofile, object, meta, fat_arch) != EXIT_SUCCESS) {

            if (meta->obin == FT_OTOOL) return EXIT_FAILURE;

            printerr(meta);
            ft_dstrclr(ofile->buffer);
        }

        offset += sizeof *fat_arch;
    }

    return EXIT_SUCCESS;
}

static int
dispatch (t_ofile *ofile, t_object *object, t_meta *meta) {

    struct s_magic {
        const char    *arch_magic;
        uint32_t     magic;
        bool        is_64;
        bool        is_cigam;
        int         (*read_file)(t_ofile *, t_object *, t_meta *);
    } magic[] = {
            {0, MH_MAGIC, false, false, read_macho_file},
            {0, MH_CIGAM, false, true, read_macho_file},
            {0, MH_MAGIC_64, true, false, read_macho_file},
            {0, MH_CIGAM_64, true, true, read_macho_file},
            {"!<arch>\n", 0, 0, 0, read_archive},
            {0, FAT_MAGIC, false, false, read_fat_file},
            {0, FAT_CIGAM, false, true, read_fat_file},
            {0, FAT_MAGIC_64, true, false, read_fat_file},
            {0, FAT_CIGAM_64, true, true, read_fat_file},
    };

    /* We add 2 to the magic_len as we also need to check for archives. */
    const int magic_len = (int)(sizeof magic / sizeof *magic);

    for (int k = 0; k < magic_len; k++) {

        if (meta->type == E_AR && k == 4) return (meta->errcode = E_MAGIC), EXIT_FAILURE;
        if (meta->type == E_FAT && k == 5) return (meta->errcode = E_MAGIC), EXIT_FAILURE;

        /* Find the magic number of the object. Then dispatch it to the correct reader. */
        if ((magic[k].arch_magic != NULL && ft_strnequ(magic[k].arch_magic, object->object, SARMAG))
        || *(uint32_t *)object->object == magic[k].magic) {

            object->is_64 = magic[k].is_64;
            object->is_cigam = magic[k].is_cigam;
            return magic[k].read_file(ofile, object, meta);
        }
    }

    /* If nothing matched, the file isn't valid. */
    meta->errcode = E_GARBAGE;
    return EXIT_FAILURE;
}

int
open_file (t_ofile *ofile, t_meta *meta) {

    /* Open the file, perform various checks and map it into memory. */
    const int     fd = open(meta->path, O_RDONLY);
    struct stat    stat;

    if (fd == -1 || fstat(fd, &stat) == -1) return EXIT_FAILURE;

    ofile->size = (size_t)stat.st_size;
    if (ofile->size < sizeof(uint32_t)) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;
    if (stat.st_mode & S_IFDIR) {

        meta->errcode = E_RRNO;
        errno = EISDIR;
        return EXIT_FAILURE;
    }

    ofile->file = mmap(NULL, ofile->size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (close(fd) == -1 || ofile->file == MAP_FAILED) return EXIT_FAILURE; /* E_RRNO */

    /*
       We duplicate the file and the file size into the object structure as well, it will allow us to process FAT
       objects independently.
    */

    t_object object = {
            .object = ofile->file,
            .name = meta->path,
            .size = ofile->size,
    };

    /* Send the file to the generic dispatcher. */
    int retcode = dispatch(ofile, &object, meta);

    munmap((void *)ofile->file, ofile->size);
    return retcode;
}
