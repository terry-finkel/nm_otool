#include "ofilep.h"

typedef struct      s_entry {
    const char      *name;
    uint8_t         n_type;
}                   t_entry;

static void
output (t_ofile *ofile, t_entry *entry) {

    ft_dstrfpush(ofile->buffer, "%s\n", entry->name);
}

static int
symtab (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

    const struct symtab_command *symtab = (struct symtab_command *)opeek(object, offset, sizeof *symtab);
    if (symtab == NULL) return EXIT_FAILURE; /* E_RRNO */

    const uint32_t stroff = oswap_32(object, symtab->stroff);
    const uint32_t strsize = oswap_32(object, symtab->strsize);
    offset = oswap_32(object, symtab->symoff);

    for (meta->k_strindex = 0; meta->k_strindex < oswap_32(object, symtab->nsyms); meta->k_strindex++) {

        const struct nlist *nlist = (struct nlist *)opeek(object, offset, sizeof *nlist);
        if (nlist == NULL) return EXIT_FAILURE; /* E_RRNO */

        const uint32_t n_strx = oswap_32(object, nlist->n_un.n_strx);
        if (stroff + n_strx > object->size) {

            meta->errcode = E_SYMSTRX;
            meta->n_strindex = (int)(stroff + strsize + n_strx - ofile->size);
            return EXIT_FAILURE;
        }

/*        t_entry entry = {
                .name = object->object + stroff + oswap_32(object, nlist->n_un.n_strx),
                .n_type = nlist->n_type
        };

        output(ofile, &entry);
*/

        offset += (object->is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist));
    }

    return EXIT_SUCCESS;
}

int
main (int argc, const char *argv[]) {

    int             index = 1, opt = 0, retcode = EXIT_SUCCESS;
    static t_dstr   buffer;
    static t_meta   meta = {
            .n_command = LC_SYMTAB + 1,
            .errcode = E_RRNO,
            .type = E_MACHO,
            .reader = {
                    [LC_SYMTAB] = symtab
            }
    };
    t_ofile          ofile = {
            .arch = NULL,
            .buffer = &buffer,
            .opt = 0
    };
    const t_opt      opts[] = {
            {FT_OPT_BOOLEAN, 'a', "all", &opt, "Display all symbol table entries, including those inserted for use by"
                "debuggers.", NM_a},
            {FT_OPT_STRING, 0, "arch", &ofile.arch, "Specifies the architecture of the file to display when the file "
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

        ofile.arch = NXGetLocalArchInfo()->name;
        if (ft_strequ(ofile.arch, "x86_64h")) ofile.arch = "x86_64";
        ofile.opt |= DUMP_ALL_ARCH;
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
        } else {

            ft_fprintf(stdout, ofile.buffer->buff);
        }

        ft_dstrclr(ofile.buffer);
    }

    return retcode;
}
