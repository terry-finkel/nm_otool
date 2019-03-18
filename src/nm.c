#include "ofilep.h"
#include <mach-o/nlist.h>


static int
symtab (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

	const struct symtab_command *symtab = (struct symtab_command *)opeek(object, offset, sizeof *symtab);
	if (symtab == NULL) return (meta->errcode = E_RRNO), EXIT_FAILURE;

	(void)ofile;
	offset = symtab->symoff;

	for (uint32_t k = 0; k < symtab->nsyms; k++) {

		const struct nlist *nlist = (struct nlist *)opeek(object, offset, sizeof *nlist);

		dprintf(1, "%s\n", (char *)(object->object + symtab->stroff + nlist->n_un.n_strx));

		offset += (object->is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist));
	}

	return EXIT_SUCCESS;
}

enum	e_opts {
	OPT_h = (1 << 0),
};

int
main (int argc, const char *argv[]) {

	int				index = 1, opt = 0, retcode = EXIT_SUCCESS;
	static t_dstr	buffer;
	static t_meta	meta = {
			.n_command = LC_SYMTAB + 1,
			.errcode = E_RRNO,
			.type = E_MACHO,
			.reader = {
					[LC_SYMTAB] = symtab
			}
	};
	t_ofile			ofile = {
			.arch = NULL,
			.dump_all_arch = false,
			.buffer = &buffer
	};
	const t_opt		opts[] = {
			{FT_OPT_BOOLEAN, 'h', "header", &opt, "Display the Mach header.", OPT_h},
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
		ofile.dump_all_arch = true;
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
