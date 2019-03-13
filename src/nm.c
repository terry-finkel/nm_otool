#include "nmp.h"


void
process_file (const void *ptr) {

	t_ctx ctx;
	struct mach_header header = *(struct mach_header *)ptr;
	ctx.is_64 = (header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64);
	ctx.is_cigam = (header.magic == MH_CIGAM_64 || header.magic == MH_CIGAM);

	ptr = (const void *)((uintptr_t)ptr + (ctx.is_64 ? sizeof(struct mach_header_64) : sizeof(struct mach_header)));
	for (uint32_t k = 0; k < header.ncmds; k++) {

		const struct segment_command segment = *(struct segment_command *)ptr;
		printf("%s\n", segment.segname);
		ptr = (const void *)((uintptr_t)ptr + segment.cmdsize);
	}
}

int
main (int argc, const char *argv[]) {

	int index = 1, opt = 0;
	const t_opt opts[] = {
		{FT_OPT_BOOLEAN, 'h', "help", &opt, "Display available options", OPT_h},
		{FT_OPT_BOOLEAN, 'A', NULL, &opt, "Write the pathname or library name of an object on each line.", OPT_A},
		{FT_OPT_END, 0, 0, 0, 0, 0}
	};

	if (ft_optparse(opts, &index, argc, (char **)argv) || opt & OPT_h) {

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "List symbols in [file(s)] (a.out by default).");
		return (opt & OPT_h) ? EXIT_SUCCESS : EXIT_FAILURE;
	};

	if (argc == 1) argv[argc++] = "a.out";

	for ( ; index < argc; index++) {

		const char *path = argv[index];
		const int fd = open(path, O_RDONLY);
		if (fd == -1) return ft_printerr(EXIT_FAILURE, "ft_nm: %s: error opening file descriptor.\n", path);

		struct stat buf;
		if (fstat(fd, &buf)) return ft_printerr(EXIT_FAILURE, "ft_nm: %s: error with fstat.\n", path);

		void *file = mmap(NULL, (size_t)buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (file == MAP_FAILED) return ft_printerr(EXIT_FAILURE, "ft_nm: %s: error mmaping file.\n", path);

		process_file(file);

		if (munmap(file, (size_t)buf.st_size)) return ft_printerr(EXIT_FAILURE, "ft_nm: %s: error unmaping file.\n", path);
		if (close(fd)) return ft_printerr(EXIT_FAILURE, "ft_nm: %s: error closing file descriptor.\n", path);
	}

	return EXIT_SUCCESS;
}
