#include "otoolp.h"


static void
hexdump (t_ofile *ofile, const uint64_t offset, const uint64_t addr, const uint64_t size) {

	unsigned char *ptr = (unsigned char *)ofile_extract(ofile, offset, size);
	ft_printf("Contents of (__TEXT,__text) section\n");
	for (uint64_t k = 0; k < size; k++) {

		if (k % 16 == 0) ft_printf("%0*llx\t", ofile->is_64 ? 16 : 8, addr + k);

		ft_printf("%.2x ", ptr[k]);

		if (k % 16 == 15 || k + 1 == size) ft_printf("\n");
	}
}

static int
segment (const char *path, t_ofile *ofile, size_t offset) {

	struct segment_command *segment = (struct segment_command *)ofile_extract(ofile, offset, sizeof *segment);
	if (segment == NULL) return E_FAILURE;

	offset += sizeof *segment;
	for (uint32_t k = 0; k < segment->nsects; k++) {

		struct section *section = (struct section *)ofile_extract(ofile, offset, sizeof *section);
		if (section == NULL) return E_FAILURE;

		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_printf("%s:\n", path);
			const uint32_t s_offset = oswap_32(ofile, section->offset);
			const uint32_t s_addr = oswap_32(ofile, section->addr);
			const uint32_t s_size = oswap_32(ofile, section->size);
			hexdump(ofile, s_offset, s_addr, s_size);
			break;
		}

		offset += sizeof *section;
	}

	return E_SUCCESS;
}

static int
segment_64 (const char *path, t_ofile *ofile, size_t offset) {

	struct segment_command_64 *segment = (struct segment_command_64 *)ofile_extract(ofile, offset, sizeof *segment);
	if (segment == NULL) return E_FAILURE;

	offset += sizeof *segment;
	for (uint32_t k = 0; k < segment->nsects; k++) {

		struct section_64 *section = (struct section_64 *)ofile_extract(ofile, offset, sizeof *section);
		if (section == NULL) return E_FAILURE;

		if (ft_strequ(section->segname, "__TEXT") && ft_strequ(section->sectname, "__text")) {

			ft_printf("%s:\n", path);
			const uint64_t s_offset = oswap_64(ofile, section->offset);
			const uint64_t s_addr = oswap_64(ofile, section->addr);
			const uint64_t s_size = oswap_64(ofile, section->size);
			hexdump(ofile, s_offset, s_addr, s_size);
			break;
		}

		offset += sizeof *section;
	}

	return E_SUCCESS;
}

int
main (int argc, const char *argv[]) {

	int index = 1, opt = 0;
	const t_opt opts[] = {
		{FT_OPT_BOOLEAN, 'h', "help", &opt, "Display available options", OPT_h},
		{FT_OPT_BOOLEAN, 't', "text", &opt, "Display the contents of the (__TEXT,__text) section.", OPT_t},
		{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv) || opt & OPT_h) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "Hexdump [file(s)] (a.out by default).");
		return (opt & OPT_h) ? EXIT_SUCCESS : EXIT_FAILURE;
	};

	if (opt < OPT_t) return ft_fprintf(stderr, "ft_otool: one of -t or -h must be specified.\n"), E_FAILURE;
	if (argc == index) argv[argc++] = "a.out";

	static t_ofile ofile = { .ncommand = LC_SEGMENT_64 + 1, .reader = {
			[LC_SEGMENT] = segment,
			[LC_SEGMENT_64] = segment_64
	}};

	for ( ; index < argc; index++) {

		const char *path = argv[index];
		const int ret = open_file(path, &ofile);
		if (ret) return printerr("ft_otool", path, ret);
	}

	return EXIT_SUCCESS;
}
