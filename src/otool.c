#include "otoolp.h"


static void
hexdump (t_ofile *ofile, const uint64_t offset, const uint64_t addr, const uint64_t size) {

	uint32_t	*ptr = (uint32_t *)ofile_extract(ofile, offset, size);
	const bool	byte_dump = ft_strequ(ofile->arch, "i386") || ft_strequ(ofile->arch, "x86-64");

	ft_dstrfpush(ofile->buffer, "Contents of (__TEXT,__text) section\n");
	for (uint64_t k = 0; k < size; k++) {

		if (k % 16 == 0) ft_dstrfpush(ofile->buffer, "%0*llx\t", ofile->is_64 ? 16 : 8, addr + k);

		if (byte_dump == true) {
			ft_dstrfpush(ofile->buffer, "%02x ", ((unsigned char *)ptr)[k]);
		} else {
			ft_dstrfpush(ofile->buffer, "%08x ", oswap_32(ofile, *(ptr + k / 4)));
			k += 3;
		}

		if (k % 16 == 15 || k + 1 == size) ft_dstrfpush(ofile->buffer, "\n");
	}
}

static int
segment (t_ofile *ofile, t_meta *meta, size_t offset) {

	struct segment_command	*segment = (struct segment_command *)ofile_extract(ofile, offset, sizeof *segment);

	offset += sizeof *segment;
	for (uint32_t k = 0; k < segment->nsects; k++) {

		struct section *section = (struct section *)ofile_extract(ofile, offset, sizeof *section);
		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_dstrfpush(ofile->buffer, "%s:\n", meta->file);
			const uint32_t s_offset = oswap_32(ofile, section->offset);
			const uint32_t s_addr = oswap_32(ofile, section->addr);
			const uint32_t s_size = oswap_32(ofile, section->size);
			hexdump(ofile, s_offset, s_addr, s_size);
			break;
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

static int
segment_64 (t_ofile *ofile, t_meta *meta, size_t offset) {

	struct segment_command_64	*segment = (struct segment_command_64 *)ofile_extract(ofile, offset, sizeof *segment);

	if (segment->fileoff + segment->filesize > ofile->size) {

		meta->errcode = E_INVALSEGOFF;
		meta->command = segment->cmd;
		return EXIT_FAILURE;
	}

	offset += sizeof *segment;
	for (uint32_t k = 0; k < segment->nsects; k++) {

		struct section_64 *section = (struct section_64 *)ofile_extract(ofile, offset, sizeof *section);
		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_dstrfpush(ofile->buffer, "%s:\n", meta->file);
			const uint64_t s_offset = oswap_64(ofile, section->offset);
			const uint64_t s_addr = oswap_64(ofile, section->addr);
			const uint64_t s_size = oswap_64(ofile, section->size);
			hexdump(ofile, s_offset, s_addr, s_size);
			break;
		}

		offset += sizeof *section;
	}

	return EXIT_SUCCESS;
}

int
main (int argc, const char *argv[]) {

	int index = 1, opt = 0;
	const t_opt	opts[] = {
		{FT_OPT_BOOLEAN, 'h', "help", &opt, "Display available options", OPT_h},
		{FT_OPT_BOOLEAN, 't', "text", &opt, "Display the contents of the (__TEXT,__text) section.", OPT_t},
		{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv) || opt & OPT_h) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return (opt & OPT_h) ? EXIT_SUCCESS : EXIT_FAILURE;
	};

	if (opt < OPT_t) return ft_fprintf(stderr, "ft_otool: one of -t or -h must be specified.\n"), EXIT_FAILURE;
	if (argc == index) argv[argc++] = "a.out";

	static t_dstr buffer;
	static t_ofile ofile = {
			.buffer = &buffer,
			.n_command = LC_SEGMENT_64 + 1,
			.reader = {
					[LC_SEGMENT] = segment,
					[LC_SEGMENT_64] = segment_64
			}
	};
	t_meta meta = { .bin = argv[0], .errcode = E_RRNO };

	for ( ; index < argc; index++) {

		const char *path = argv[index];
		meta.file = path;

		if (open_file(path, &ofile, &meta) != EXIT_SUCCESS) return printerr(meta);

		ft_printf(ofile.buffer->buff);
		ft_dstrclr(ofile.buffer);
	}

	return EXIT_SUCCESS;
}
