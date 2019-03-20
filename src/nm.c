#include "ofilep.h"


typedef struct      s_entry {
    const char      *name;
    uint8_t         n_type;
    uint8_t         n_sect;
    uint64_t        n_value;
}                   t_entry;

struct              s_nosect {
    const char      *name;
    const char      letter;
}                   nosect[] = {
        [N_ABS] = {"", 'A'},
        [N_INDR] = {"", 'I'},
        [N_UNDF] = {"", 'U'}
};

struct              s_symbols {
    const char      *segname;
    const char      *sectname;
    char            letter;
};

t_vary              g_vary_stack = {NULL, 0, 0, sizeof(struct s_symbols *)};
t_vary              *g_vary = &g_vary_stack;

static void
symbol_cleanup (void *data) {

    ft_memdtor((void **)data);
}

static int
numerical_sort (const void *restrict a, const void *restrict b) {

    return ft_strcmp(((t_entry *)a)->name, ((t_entry *)b)->name) > 0;
}

static int
regular_sort (const void *restrict a, const void *restrict b) {

    return ft_strcmp(((t_entry *)a)->name, ((t_entry *)b)->name) > 0;
}

static void
output (t_ofile *ofile, const t_object *object, const t_entry *entry) {

    if ((ofile->opt & NM_j) == 0) {

        if ((entry->n_type & N_TYPE) != N_UNDF)
            ft_dstrfpush(ofile->buffer, "%.*lx", (object->is_64 ? 16 : 8), entry->n_value);

        else ft_dstrfpush(ofile->buffer, "%*c", (object->is_64 ? 16 : 8), ' ');
    }

    char letter;
    if (entry->n_sect == NO_SECT || (entry->n_type & N_TYPE) == N_ABS || (entry->n_type & N_TYPE) == N_INDR)
        letter = nosect[entry->n_type & N_TYPE].letter;

    else letter = ((struct s_symbols **)g_vary->buff)[entry->n_sect - 1]->letter;

    ft_dstrfpush(ofile->buffer, " %c ", ((entry->n_type & N_EXT) == 0 ? ft_tolower(letter) : letter));

    if (ofile->opt & NM_a && entry->n_type & N_STAB) ft_dstrfpush(ofile->buffer, "%.2u", entry->n_sect);

    ft_dstrfpush(ofile->buffer, " %s\n", entry->name);
}

static int
symtab (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    const struct symtab_command *symtab = (struct symtab_command *)opeek(object, offset, sizeof *symtab);
    if (symtab == NULL) return EXIT_FAILURE; /* E_RRNO */

    const uint32_t stroff = oswap_32(object, symtab->stroff);
    const uint32_t strsize = oswap_32(object, symtab->strsize);
    t_list *list = NULL;
    offset = oswap_32(object, symtab->symoff);

    for (meta->k_strindex = 0; meta->k_strindex < oswap_32(object, symtab->nsyms); meta->k_strindex++) {

        const struct nlist_64 *nlist = (struct nlist_64 *)opeek(object, offset, sizeof *nlist);
        if (nlist == NULL) return EXIT_FAILURE; /* E_RRNO */

        const uint32_t n_strx = oswap_32(object, nlist->n_un.n_strx);
        if (stroff + n_strx > object->size) {

            meta->errcode = E_SYMSTRX;
            meta->n_strindex = (int)(stroff + strsize + n_strx - ofile->size);
            return EXIT_FAILURE;
        }

        t_entry entry = {
                .name = object->object + stroff + oswap_32(object, nlist->n_un.n_strx),
                .n_sect = nlist->n_sect,
                .n_type = nlist->n_type,
                .n_value = (object->is_64
                        ? oswap_64(object, nlist->n_value)
                        : oswap_32(object, ((struct nlist *)nlist)->n_value))
        };

        t_list *link = ft_lstctor(&entry, sizeof(entry));

        if (ofile->opt & NM_a || (nlist->n_type & N_STAB) == 0) {

            if (ofile->opt & NM_n) ft_lstinsert(&list, link, numerical_sort, (ofile->opt & NM_r ? E_REV : E_REG));
            else if (ofile->opt & NM_p) ft_lstappend(&list, link);
            else ft_lstinsert(&list, link, regular_sort, (ofile->opt & NM_r ? E_REV : E_REG));
        }

        offset += (object->is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist));
    }

    /* Go through out linked list to print the sorted symbols. Clean the list while we're at it. */
    while (list != NULL) {

        output(ofile, object, list->data);
        t_list *tmp = list->next;
        ft_memdtor(&list->data);
        ft_memdtor((void **)&list);
        list = tmp;
    }

    /* We need to clear our symbols array before returning in case of an archive or fat file. */
    ft_varyclr(g_vary, (t_vdtor)symbol_cleanup);

    return EXIT_SUCCESS;
}

static void
populate_symbols (t_object *object, const char *restrict segname, const char *restrict sectname) {

    struct s_symbols *symbols = ft_malloc(sizeof *symbols);
    symbols->sectname = sectname;
    symbols->segname = segname;

    if (ft_strequ(sectname, SECT_BSS)) symbols->letter = 'B';
    else if (ft_strequ(sectname, SECT_COMMON)) symbols->letter = 'C';
    else if (ft_strequ(sectname, SECT_DATA)) symbols->letter = 'D';
    else if (ft_strequ(sectname, SECT_TEXT)) symbols->letter = 'T';
    else symbols->letter = 'S';

    *(struct s_symbols **)ft_varypush(g_vary) = symbols;
    object->k_sect += 1;
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

        populate_symbols(object, section->segname, section->sectname);
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

        populate_symbols(object, section->segname, section->sectname);
        offset += sizeof *section;
    }

    return EXIT_SUCCESS;
}

int
main (int argc, const char *argv[]) {

    int             index = 1, retcode = EXIT_SUCCESS;
    static t_dstr   buffer;
    static t_meta   meta = {
            .n_command = LC_SEGMENT_64 + 1,
            .errcode = E_RRNO,
            .type = E_MACHO,
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
            {FT_OPT_BOOLEAN, 'a', "all", &ofile.opt, "Display all symbol table entries, including those inserted for "
                "use by debuggers.", NM_a},
            {FT_OPT_BOOLEAN, 'n', "num", &ofile.opt, "Sort numerically rather than alphabetically.", NM_n},
            {FT_OPT_BOOLEAN, 'p', "num", &ofile.opt, "Don't sort; display in symbol-table order.", NM_p},
            {FT_OPT_BOOLEAN, 'r', "num", &ofile.opt, "Sort in reverse order.", NM_r},
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

    if (ofile.arch == NULL) {

        ofile.opt |= DUMP_ALL_ARCH;
        ofile.arch = NXGetLocalArchInfo()->name;

        if (ft_strequ(ofile.arch, "x86_64h")) ofile.arch = "x86_64";

    } else if (ft_strequ(ofile.arch, "all") == 0 && NXGetArchInfoFromName(ofile.arch) == NULL) {

        ft_fprintf(stderr, "%1$s: unknown architecture specification flag: --arch %2$s\n%1$s: known architecture flags"
                " are:", argv[0], ofile.arch);
        const NXArchInfo *nxArchInfo = NXGetAllArchInfos();

        for (int k = 0; nxArchInfo[k].name != NULL; k++) ft_fprintf(stderr, " %s", nxArchInfo[k].name);

        ft_fprintf(stderr, "\n");
        ft_optusage(opts, (char *)argv[0], "[file(s)]", "Dump symbols from [file(s)] (a.out by default).");
        return EXIT_FAILURE;
    }

    meta.bin = argv[0];
    for ( ; index < argc; index++) {

        meta.path = argv[index];
        if (open_file(&ofile, &meta) != EXIT_SUCCESS) {

            retcode = EXIT_FAILURE;
            printerr(&meta);
        }
    }

    return retcode;
}
