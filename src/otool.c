#include "ofilep.h"


static void
hexdump (t_ofile *ofile, t_object *object, const uint64_t offset, const uint64_t addr, const uint64_t size) {

	uint32_t	*ptr = (uint32_t *)(object->object + offset);
	const bool	dbyte = (object->nxArchInfo == NULL
			|| (object->nxArchInfo->cputype != CPU_TYPE_I386 && object->nxArchInfo->cputype != CPU_TYPE_X86_64));

	for (uint64_t k = 0; k < size; k++) {

		if (k % 16 == 0) ft_dstrfpush(ofile->buffer, "%0*llx\t", object->is_64 ? 16 : 8, addr + k);

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

		const struct section *section = (struct section *)opeek(object, offset, sizeof *segment);
		if (section == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

		if (oswap_32(object, section->offset) + oswap_32(object, section->size) > ofile->size) {

			meta->errcode = E_SECTOFF;
			meta->command = oswap_32(object, segment->cmd);
			meta->k_section = k;
			return EXIT_FAILURE;
		}

		if ((ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text") && ofile->dump_text)
			|| (ft_strequ(section->segname, "__DATA") && ft_strequ(section->sectname, "__data") && ofile->dump_data)) {

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

		const struct section_64 *section = (struct section_64 *)(object->object + offset);
		if (section == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

		if (oswap_32(object, section->offset) + oswap_32(object, section->size) > ofile->size) {

			meta->errcode = E_SECTOFF;
			meta->command = oswap_32(object, segment->cmd);
			meta->k_section = k;
			return EXIT_FAILURE;
		}

		if ((ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text") && ofile->dump_text)
			|| (ft_strequ(section->segname, "__DATA") && ft_strequ(section->sectname, "__data") && ofile->dump_data)) {

			ft_dstrfpush(ofile->buffer, "Contents of (%s,%s) section\n", section->segname, section->sectname);
			hexdump(ofile, object, oswap_64(object, section->offset), oswap_64(object, section->addr),
					oswap_64(object, section->size));
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

enum		e_opts {
	OPT_d = 1,
	OPT_h = 2,
	OPT_t = 4
};

int
main (int argc, const char *argv[]) {

	int				index = 1, opt = 0, retcode = EXIT_SUCCESS;
	static t_dstr	buffer;
	static t_meta	meta = {
			.n_command = LC_SEGMENT_64 + 1,
			.errcode = E_RRNO,
			.type = E_MACHO,
			.reader = {
					[LC_SEGMENT] = segment,
					[LC_SEGMENT_64] = segment_64
			}
	};
	t_ofile			ofile = {
			.arch = NULL,
			.dump_all_arch = false,
			.buffer = &buffer
	};
	const t_opt		opts[] = {
			{FT_OPT_BOOLEAN, 'd', "data", &opt, "Display the contents of the (__DATA, __data) section.", OPT_d},
			{FT_OPT_BOOLEAN, 'h', "header", &opt, "Display the Mach header.", OPT_h},
			{FT_OPT_BOOLEAN, 't', "text", &opt, "Display the contents of the (__TEXT,__text) section.", OPT_t},
			{FT_OPT_STRING, 0, "arch", &ofile.arch, "Specifies the architecture of the file to display when the file "
				"is a fat binary. \"all\" can be specified to display all architectures in the file. The default is to "
				"display only the host architecture.", 0},
			{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv)) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return EXIT_FAILURE;
	};

	if (opt < OPT_h) return ft_fprintf(stderr, "%s: one of -dht must be specified.\n", argv[0]), EXIT_FAILURE;
	if (argc == index) argv[argc++] = "a.out";

	ofile.dump_data = (opt & OPT_d) != 0;
	ofile.dump_header = (opt & OPT_h) != 0;
	ofile.dump_text = (opt & OPT_t) != 0;

	if (ofile.arch == NULL) {

		ofile.arch = NXGetLocalArchInfo()->name;
		ofile.dump_all_arch = true;
	} else if (ft_strequ(ofile.arch, "all") == 0 && NXGetArchInfoFromName(ofile.arch) == NULL) {

		ft_fprintf(stderr, "%1$s: unknown architecture specification flag: --arch %2$s\n%1$s: known architecture flags"
					 " are:", argv[0], ofile.arch);
		const NXArchInfo *nxArchInfo = NXGetAllArchInfos();

		for (int k = 0; nxArchInfo[k].name != NULL; k++) ft_fprintf(stderr, " %s", nxArchInfo[k].name);

		ft_fprintf(stderr, "\n");
		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return EXIT_FAILURE;
	}

	meta.bin = argv[0];
	for ( ; index < argc; index++) {

		meta.path = argv[index];
		if (open_file(&ofile, &meta) != EXIT_SUCCESS) {

			retcode = EXIT_FAILURE;
			printerr(&meta);
		} else {

			ft_printf(ofile.buffer->buff);
		}

		ft_dstrclr(ofile.buffer);
	}

	return retcode;
}
