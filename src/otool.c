#include "otoolp.h"


int
process_file (const void *ptr, const char *path) {

	t_ctx ctx;
	struct mach_header header = *(struct mach_header *)ptr;
	ctx.is_64 = (header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64);
	ctx.is_cigam = (header.magic == MH_CIGAM_64 || header.magic == MH_CIGAM);

	if (ctx.is_64 == false && ctx.is_cigam == false && header.magic != MH_MAGIC) return EXIT_FAILURE;

	printf("%s:\nContents of (__TEXT,__text) section\n", path);
	ptr = (const void *)((uintptr_t)ptr + (ctx.is_64 ? sizeof(struct mach_header_64) : sizeof(struct mach_header)));
	for (uint32_t k = 0; k < header.ncmds; k++) {

		if (ctx.is_64) {
			ctx.seg64 = *(struct segment_command_64 *)ptr;
			printf("%s, %llu\n", ctx.seg64.segname, ctx.seg64.vmsize);
		} else {
			ctx.seg = *(struct segment_command *)ptr;
			printf("%s, %u\n", ctx.seg64.segname, ctx.seg.vmsize);
		}

		ptr = (const void *)((uintptr_t)ptr + ctx.seg.cmdsize);
	}

	return EXIT_SUCCESS;
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

		ft_optusage(opts, (char *)argv[0], "[file(s)]", "List symbols in [file(s)] (a.out by default).");
		return (opt & OPT_h) ? EXIT_SUCCESS : EXIT_FAILURE;
	};

	if (opt < OPT_t) return ft_printerr(EXIT_FAILURE, "ft_otool: one of -t or --version must be specified");
	if (argc == 1) argv[argc++] = "a.out";

	for ( ; index < argc; index++) {

		const char *path = argv[index];
		const int fd = open(path, O_RDONLY);
		if (fd == -1) return ft_printerr(EXIT_FAILURE, "ft_otool: %s: error opening file descriptor.\n", path);

		struct stat buf;
		if (fstat(fd, &buf)) return ft_printerr(EXIT_FAILURE, "ft_otool: %s: error with fstat.\n", path);

		void *file = mmap(NULL, (size_t)buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (file == MAP_FAILED) return ft_printerr(EXIT_FAILURE, "ft_otool: %s: error mmaping file.\n", path);

		if (process_file(file, path) == EXIT_FAILURE) return ft_printerr(EXIT_FAILURE, "%s: is not an object file.\n", path);

		if (munmap(file, (size_t)buf.st_size)) return ft_printerr(EXIT_FAILURE, "ft_otool: %s: error unmaping file.\n",\
																	path);
		if (close(fd)) return ft_printerr(EXIT_FAILURE, "ft_otool: %s: error closing file descriptor.\n", path);
	}

	return EXIT_SUCCESS;
}
