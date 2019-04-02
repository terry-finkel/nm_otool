#include "ofilep.h"
#include <mach-o/stab.h>

typedef struct      s_entry {
    const char      *name;
    uint8_t         n_type;
    uint8_t         n_sect;
    uint64_t        n_value;
}                   t_entry;

static const char   nosect[] = {
        [N_ABS] = 'A',
        [N_INDR] = 'I',
        [N_UNDF] = 'U'
};

static const char   *stab[] = {
        [N_GSYM] = "GSYM",
        [N_FNAME] = "FNAME",
        [N_FUN] = "FUN",
        [N_STSYM] = "STSYM",
        [N_LCSYM] = "LCSYM",
        [N_BNSYM] = "BNSYM",
        [N_AST] = "AST",
        [N_OPT] = "OPT",
        [N_RSYM] = "RSYM",
        [N_SLINE] = "SLINE",
        [N_ENSYM] = "ENSYM",
        [N_SSYM] = "SSYM",
        [N_SO] = "SO",
        [N_OSO] = "OSO",
        [N_LSYM] = "LSYM",
        [N_BINCL] = "BINCL",
        [N_SOL] = "SOL",
        [N_PARAMS] = "PARAMS",
        [N_VERSION] = "VERSION",
        [N_OLEVEL] = "OLEVEL",
        [N_PSYM] = "PSYM",
        [N_EINCL] = "EINCL",
        [N_ENTRY] = "ENTRY",
        [N_LBRAC] = "LBRAC",
        [N_EXCL] = "EXCL",
        [N_RBRAC] = "RBRAC",
        [N_BCOMM] = "BCOMM",
        [N_ECOMM] = "ECOMM",
        [N_ECOML] = "ECOML",
        [N_LENG] = "LENG"
};

static char         symbols[UINT8_MAX];

static int
regular_sort (const void *restrict a, const void *restrict b) {

    /* If both entries have the same name, we sort numerically. */
    if (ft_strequ(((t_entry *)a)->name, ((t_entry *)b)->name))
        return ((t_entry *)a)->n_value >= ((t_entry *)b)->n_value;

    return ft_strcmp(((t_entry *)a)->name, ((t_entry *)b)->name) > 0;
}

static int
numerical_sort (const void *restrict a, const void *restrict b) {

    /* Lexical sort for some entries with the same values. */
    if (((t_entry *)a)->n_value == ((t_entry *)b)->n_value && (((t_entry *)a)->n_type & N_TYPE) == N_UNDF)
        return ft_strcmp(((t_entry *)a)->name, ((t_entry *)b)->name) > 0;

    return ((t_entry *)a)->n_value >= ((t_entry *)b)->n_value;
}

static bool
is_common (const uint8_t n_type, const uint64_t n_value) {

    return (n_type & N_TYPE) == N_UNDF && (n_type & N_EXT) != 0 && n_value != 0;
}

static void
output (t_ofile *ofile, const t_object *object, const t_entry *entry) {

    const uint8_t type = (uint8_t)(entry->n_type & N_TYPE);

    /* If one of these two options is specified, we need merely to display the name. */
    if ((ofile->opt & NM_j) == 0 && (ofile->opt & NM_u) == 0) {

        /* Only retrieve the type from symbols if the symbol belongs to a section. */
        int letter;
        if (type != N_UNDF && type != N_ABS && type != N_INDR && (entry->n_type & N_STAB) == 0) {
            letter = symbols[entry->n_sect];
        } else if (is_common(entry->n_type, entry->n_value)) {
            letter = 'C';
        } else if (entry->n_type & N_STAB) {
            letter = '-';
        } else {
            letter = nosect[type];
        }

        if (type != N_UNDF || letter == 'C') {
            ft_dstrfpush(ofile->buffer, "%.*lx", (object->is_64 ? 16 : 8), entry->n_value);
        } else {
            ft_dstrfpush(ofile->buffer, "%*c", (object->is_64 ? 16 : 8), ' ');
        }

        ft_dstrfpush(ofile->buffer, " %c ", ((entry->n_type & N_EXT) == 0 ? ft_tolower(letter) : letter));

        /* N_STAB debugging symbols detail. */
        if (ofile->opt & NM_a && entry->n_type & N_STAB)
            ft_dstrfpush(ofile->buffer, "%.2x %.4x %5s ", entry->n_sect, entry->n_type == N_OSO, stab[entry->n_type]);
    }

    ft_dstrfpush(ofile->buffer, "%s\n", entry->name);
}

static int
symtab (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    const struct symtab_command *symtab = (struct symtab_command *)opeek(object, offset, sizeof *symtab);
    if (symtab == NULL) return EXIT_FAILURE; /* E_RRNO */

    const uint32_t stroff = oswap_32(object, symtab->stroff);
    const uint32_t strsize = oswap_32(object, symtab->strsize);
    t_list *list = NULL;
    offset = oswap_32(object, symtab->symoff);

    for (meta->u_k.k_strindex = 0; meta->u_k.k_strindex < oswap_32(object, symtab->nsyms); meta->u_k.k_strindex++) {

        const struct nlist_64 *nlist = (struct nlist_64 *)opeek(object, offset, sizeof *nlist);
        if (nlist == NULL) return EXIT_FAILURE; /* E_RRNO */

        const uint32_t n_strx = oswap_32(object, nlist->n_un.n_strx);
        if (stroff + n_strx > object->size) {

            meta->errcode = E_SYMSTRX;
            meta->u_n.n_strindex = (int)(stroff + strsize + n_strx - ofile->size);
            return EXIT_FAILURE;
        }

        /* Increment the offset in case our symbol should not be inserted in the linked list. */
        offset += (object->is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist));

        t_entry entry = {
                .name = object->object + stroff + oswap_32(object, nlist->n_un.n_strx),
                .n_sect = nlist->n_sect,
                .n_type = nlist->n_type,
                .n_value = (object->is_64
                            ? oswap_64(object, nlist->n_value)
                            : oswap_32(object, ((struct nlist *)nlist)->n_value))
        };

        /*
           Only insert the symbol in the linked list if the command line options match.
            -a: display N_STAB debugging entries
            -h: only display external symbols
            -u: only display undefined symbols
            -U: do not display undefined symbols
        */

        if ((nlist->n_type & N_STAB) && (ofile->opt & NM_a) == 0) continue;
        if ((nlist->n_type & N_EXT) == 0 && ofile->opt & NM_g) continue;

        /* Common symbols have the undefined bit set so we need to check for them with the -u amd -U option. */
        const bool common = is_common(entry.n_type, entry.n_value);
        if (ofile->opt & NM_u && ((nlist->n_type & N_TYPE) != N_UNDF || common == true)) continue;
        if ((nlist->n_type & N_TYPE) == N_UNDF && common == false && ofile->opt & NM_U) continue;

        t_list *link = ft_lstctor(&entry, sizeof(entry));

        /* Insert the link in our linked list depending on sorting option. */
        if (ofile->opt & NM_n) ft_lstinsert(&list, link, numerical_sort, (ofile->opt & NM_r ? E_REV : E_REG));
        else if (ofile->opt & NM_p) ft_lstappend(&list, link);
        else ft_lstinsert(&list, link, regular_sort, (ofile->opt & NM_r ? E_REV : E_REG));
    }

    /* Go through out linked list to print the sorted symbols. Clean the list while we're at it. */
    while (list != NULL) {

        output(ofile, object, list->data);
        t_list *tmp = list->next;
        ft_memdtor(&list->data);
        ft_memdtor((void **)&list);
        list = tmp;
    }

    return EXIT_SUCCESS;
}

static int
segment (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    (void)ofile;
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

        if (ft_strequ(section->sectname, SECT_BSS)) symbols[object->k_sect] = 'B';
        else if (ft_strequ(section->sectname, SECT_DATA)) symbols[object->k_sect] = 'D';
        else if (ft_strequ(section->sectname, SECT_TEXT)) symbols[object->k_sect] = 'T';
        else symbols[object->k_sect] = 'S';

        object->k_sect += 1;
        offset += sizeof *section;
    }

    return EXIT_SUCCESS;
}

static int
segment_64 (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    (void)ofile;
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

        if (ft_strequ(section->sectname, SECT_BSS)) symbols[object->k_sect] = 'B';
        else if (ft_strequ(section->sectname, SECT_DATA)) symbols[object->k_sect] = 'D';
        else if (ft_strequ(section->sectname, SECT_TEXT)) symbols[object->k_sect] = 'T';
        else symbols[object->k_sect] = 'S';

        object->k_sect += 1;
        offset += sizeof *section;
    }

    return EXIT_SUCCESS;
}

int
main (int argc, const char *argv[]) {

    int             index = 1;
    static t_dstr   buffer;
    static t_meta   meta = {
            .obin = FT_NM,
            .reader = {
                    [LC_SYMTAB] = symtab,
                    [LC_SEGMENT] = segment,
                    [LC_SEGMENT_64] = segment_64
            }
    };
    t_ofile          ofile = {
            .arch = NULL,
            .buffer = &buffer,
            .opt = 0
    };
    const t_opt      opts[] = {
            {FT_OPT_BOOLEAN, 'a', "all-symbols", &ofile.opt, "Display all symbol table entries, including those "
                "inserted for use by debuggers.", NM_a},
            {FT_OPT_BOOLEAN, 'g', "only-globals", &ofile.opt, "Display only global (external) symbols.", NM_g},
            {FT_OPT_BOOLEAN, 'j', "just-symbol", &ofile.opt, "Just display the symbol names (no value or type).", NM_j},
            {FT_OPT_BOOLEAN, 'n', "numerical-sort", &ofile.opt, "Sort numerically rather than alphabetically.", NM_n},
            {FT_OPT_BOOLEAN, 'p', "no-sort", &ofile.opt, "Don't sort; display in symbol-table order.", NM_p},
            {FT_OPT_BOOLEAN, 'r', "reverse-sort", &ofile.opt, "Sort in reverse order.", NM_r},
            {FT_OPT_BOOLEAN, 'u', "only-undefined", &ofile.opt, "Display only undefined symbols.", NM_u},
            {FT_OPT_BOOLEAN, 'U', "no-undefined", &ofile.opt, "Don't display undefined symbols.", NM_U},
            {FT_OPT_STRING, 'A', "arch", &ofile.arch, "Specifies the architecture of the file to display when the file "
                "is a fat binary. \"all\" can be specified to display all architectures in the file. The default is to "
                "display only the host architecture.", 0},
            {FT_OPT_END, 0, 0, 0, 0, 0}
    };

    if (ft_optparse(opts, &index, argc, (char **)argv)) {

        ft_optusage(opts, (char *)argv[0], "[file(s)]", "Dump symbols from [file(s)] (a.out by default).");
        return EXIT_FAILURE;
    };

    if (argc == index) argv[argc++] = "a.out";
    if (ofile.arch && ft_strequ(ofile.arch, "all") == 0 && NXGetArchInfoFromName(ofile.arch) == NULL) {

        ft_fprintf(stderr, "%1$s: for the -arch option: Unknown architecture named \'%2$s\'.\n%1$s: %3$s: No "
                "architecture specified.\n", argv[0], ofile.arch, argv[index]);
        return EXIT_FAILURE;
    }

    /* Only output file name if there are multiple files. */
    if (argc - 1 > index) ofile.opt |= NAME_OUTPUT;

    int retcode = EXIT_SUCCESS;
    meta.bin = argv[0];
    for ( ; index < argc; index++) {

        meta.path = argv[index];
        meta.errcode = E_RRNO;
        meta.type = E_MACHO;
        if (open_file(&ofile, &meta) != EXIT_SUCCESS) {

            /* nm doesn't return if the binary is not a valid object. */
            printerr(&meta);
            retcode = EXIT_FAILURE;
        }
    }

    return retcode;
}
