#include "ofilep.h"


static void
header_dump (t_ofile *ofile, t_object *object) {

	const struct mach_header	*header = (struct mach_header *)object->object;
	const uint32_t 				magic = oswap_32(object, header->magic);
	const uint32_t				cputype = oswap_32(object, (uint32_t)header->cputype);
	const uint32_t				cpusubtype = oswap_32(object, (uint32_t)header->cpusubtype) & ~CPU_SUBTYPE_MASK;
	const bool					caps = (oswap_32(object, (uint32_t)header->cpusubtype) & CPU_SUBTYPE_MASK) != 0;
	const uint32_t				filetype = oswap_32(object, header->filetype);
	const uint32_t				ncmds = oswap_32(object, header->ncmds);
	const uint32_t				sizeofcmds = oswap_32(object, header->sizeofcmds);
	const uint32_t 				flags = oswap_32(object, header->flags);

	ft_dstrfpush(ofile->buffer, "Mach header\n");
	ft_dstrfpush(ofile->buffer, "      magic cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags\n");
	ft_dstrfpush(ofile->buffer, "%11#x %7d %10d %5.2#p %11u %5u %10u %#.8x\n", magic, cputype, cpusubtype, caps ? 128 : 0,
		filetype, ncmds, sizeofcmds, flags);
}

static void
hexdump (t_ofile *ofile, t_object *object, const uint64_t offset, const uint64_t addr, const uint64_t size) {

	uint32_t	*ptr = (uint32_t *)(object->object + offset);
	const bool	dbyte = (object->nxArchInfo == NULL
			|| (object->nxArchInfo->cputype != CPU_TYPE_I386 && object->nxArchInfo->cputype != CPU_TYPE_X86_64));

	ft_dstrfpush(ofile->buffer, "Contents of (__TEXT,__text) section\n");
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

	struct segment_command	*segment = (struct segment_command *)(object->object + offset);

	if (oswap_32(object, segment->fileoff) + oswap_32(object, segment->filesize) > object->size) {

		meta->errcode = E_SEGOFF;
		meta->command = oswap_32(object, segment->cmd);
		return EXIT_FAILURE;
	}

	offset += sizeof *segment;
	const uint32_t nsects = oswap_32(object, segment->nsects);
	for (uint32_t k = 0; k < nsects; k++) {

		const struct section *section = (struct section *)(object->object + offset);
		if (oswap_32(object, section->offset) + oswap_32(object, section->size) > ofile->size) {

			meta->errcode = E_SECTOFF;
			meta->command = oswap_32(object, segment->cmd);
			meta->k_section = k;
			return EXIT_FAILURE;
		}

		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			if (ofile->dump_text) ft_dstrfpush(ofile->buffer, "%s", meta->path);
			if (ofile->dump_text && ofile->arch_output) ft_dstrfpush(ofile->buffer, " (architecture %s)", ofile->arch);
			if (ofile->dump_text) ft_dstrfpush(ofile->buffer, ":\n");

			const uint32_t s_offset = oswap_32(object, section->offset);
			const uint32_t s_addr = oswap_32(object, section->addr);
			const uint32_t s_size = oswap_32(object, section->size);

			if (ofile->dump_text) hexdump(ofile, object, s_offset, s_addr, s_size);
			if (ofile->dump_header) header_dump(ofile, object);

			break;
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

static int
segment_64 (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

	struct segment_command_64	*segment = (struct segment_command_64 *)(object->object + offset);

	if (oswap_64(object, segment->fileoff) + oswap_64(object, segment->filesize) > object->size) {

		meta->errcode = E_SEGOFF;
		meta->command = oswap_32(object, segment->cmd);
		return EXIT_FAILURE;
	}

	offset += sizeof *segment;
	const uint32_t nsects = oswap_32(object, segment->nsects);
	for (uint32_t k = 0; k < nsects; k++) {

		const struct section_64 *section = (struct section_64 *)(object->object + offset);
		if (oswap_32(object, section->offset) + oswap_32(object, section->size) > ofile->size) {

			meta->errcode = E_SECTOFF;
			meta->command = oswap_32(object, segment->cmd);
			meta->k_section = k;
			return EXIT_FAILURE;
		}

		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			if (ofile->dump_text) ft_dstrfpush(ofile->buffer, "%s", meta->path);
			if (ofile->dump_text && ofile->arch_output) ft_dstrfpush(ofile->buffer, " (architecture %s)", ofile->arch);
			if (ofile->dump_text) ft_dstrfpush(ofile->buffer, ":\n");

			const uint64_t s_offset = oswap_64(object, section->offset);
			const uint64_t s_addr = oswap_64(object, section->addr);
			const uint64_t s_size = oswap_64(object, section->size);

			if (ofile->dump_text) hexdump(ofile, object, s_offset, s_addr, s_size);
			if (ofile->dump_header) header_dump(ofile, object);

			break;
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

enum		e_opts {
	OPT_h = (1 << 0),
	OPT_t = (1 << 1)
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
		{FT_OPT_BOOLEAN, 'h', "header", &opt, "Display the Mach header.", OPT_h},
		{FT_OPT_BOOLEAN, 't', "text", &opt, "Display the contents of the (__TEXT,__text) section.", OPT_t},
		{FT_OPT_STRING, 'A', "arch", &ofile.arch, "Specifies the architecture of the file to display when the file is "
			"a fat binary. \"all\" can be specified to display all architectures in the file. The default is to "
			"display only the host architecture.", 0},
		{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv)) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return EXIT_FAILURE;
	};

	if (opt < OPT_h) return ft_fprintf(stderr, "%s: one of -t or -h must be specified.\n", argv[0]), EXIT_FAILURE;
	if (argc == index) argv[argc++] = "a.out";

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
		}

		ft_printf(ofile.buffer->buff);
		ft_dstrclr(ofile.buffer);
	}

	return retcode;
}
