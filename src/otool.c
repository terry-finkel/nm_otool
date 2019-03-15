#include "ofilep.h"


static void
hexdump (t_ofile *ofile, t_object *object, const uint64_t offset, const uint64_t addr, const uint64_t size) {

	uint32_t	*ptr = (uint32_t *)object_extract(object, offset, size);
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

	struct segment_command	*segment = (struct segment_command *)object_extract(object, offset, sizeof *segment);

	if (oswap_32(object, segment->fileoff) + oswap_32(object, segment->filesize) > object->size) {

		ofile->errcode = E_INVALSEGOFF;
		meta->command = oswap_32(object, segment->cmd);
		return EXIT_FAILURE;
	}

	offset += sizeof *segment;
	const uint32_t nsects = oswap_32(object, segment->nsects);
	for (uint32_t k = 0; k < nsects; k++) {

		const struct section *section = (struct section *)object_extract(object, offset, sizeof *section);
		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_dstrfpush(ofile->buffer, "%s:\n", ofile->path);
			const uint32_t s_offset = oswap_32(object, section->offset);
			const uint32_t s_addr = oswap_32(object, section->addr);
			const uint32_t s_size = oswap_32(object, section->size);
			hexdump(ofile, object, s_offset, s_addr, s_size);
			break;
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

static int
segment_64 (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

	struct segment_command_64	*segment = (struct segment_command_64 *)object_extract(object, offset, sizeof *segment);

	if (oswap_64(object, segment->fileoff) + oswap_64(object, segment->filesize) > object->size) {

		ofile->errcode = E_INVALSEGOFF;
		meta->command = oswap_32(object, segment->cmd);
		return EXIT_FAILURE;
	}

	offset += sizeof *segment;
	const uint32_t nsects = oswap_32(object, segment->nsects);
	for (uint32_t k = 0; k < nsects; k++) {

		const struct section_64 *section = (struct section_64 *)object_extract(object, offset, sizeof *section);
		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_dstrfpush(ofile->buffer, "%s:\n", ofile->path);
			const uint64_t s_offset = oswap_64(object, section->offset);
			const uint64_t s_addr = oswap_64(object, section->addr);
			const uint64_t s_size = oswap_64(object, section->size);
			hexdump(ofile, object, s_offset, s_addr, s_size);
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

	int index = 1, opt = 0;
	const char *arch = NULL;
	const t_opt	opts[] = {
		{FT_OPT_BOOLEAN, 'h', "help", &opt, "Display available options.", OPT_h},
		{FT_OPT_BOOLEAN, 't', "text", &opt, "Display the contents of the (__TEXT,__text) section.", OPT_t},
		{FT_OPT_STRING, 'A', "arch", &arch, "Specifies the architecture of the file to display when the file is a "\
			"fat binary. \"all\" can be specified to display all architectures in the file. The default is to display "\
			"only the host architecture.", 0},
		{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv) || opt & OPT_h) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return (opt & OPT_h) ? EXIT_SUCCESS : EXIT_FAILURE;
	};

	if (opt < OPT_t) return ft_fprintf(stderr, "ft_otool: one of -t or -h must be specified.\n"), EXIT_FAILURE;
	if (argc == index) argv[argc++] = "a.out";

	static t_meta meta = {
			.n_command = LC_SEGMENT_64 + 1,
			.reader = {
					[LC_SEGMENT] = segment,
					[LC_SEGMENT_64] = segment_64
			}
	};
	static t_dstr buffer;
	t_ofile ofile = {
			.type = E_MACHO,
			.buffer = &buffer,
			.bin = argv[0],
			.errcode = E_RRNO,
	};

	for ( ; index < argc; index++) {

		const char *path = argv[index];
		ofile.path = path;

		if (open_file(&ofile, &meta) != EXIT_SUCCESS) return printerr(ofile, meta);

		ft_printf(ofile.buffer->buff);
		ft_dstrclr(ofile.buffer);
	}

	return EXIT_SUCCESS;
}
