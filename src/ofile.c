#include "ofilep.h"


static const char		*errors[] = {
		"truncated or malformed object",
		"is not an object file",
		"cmdsize not a multiple of 8"
};

static const size_t		header_size[] = {
		[false] = sizeof(struct mach_header),
		[true] = sizeof(struct mach_header_64)
};

static const char 		*segcodes[] = {
		[LC_SEGMENT] = "LC_SEGMENT",
		[LC_SEGMENT_64] = "LC_SEGMENT_64"
};

int
printerr (t_meta error) {

	if (error.errcode == E_RRNO) {
		ft_fprintf(stderr, "%s: \'%s\': %s\n", error.bin, error.file, strerror(errno));
	} else if (error.errcode == E_MAGIC) {
		ft_fprintf(stderr, "%s: %s\n", error.file, errors[error.errcode]);
	} else {

		ft_fprintf(stderr, "%s: \'%s\': %s (load command %d ", error.bin, error.file, errors[0], error.k_command);
		switch (error.errcode) {
			case E_INVALSEGOFF:
				ft_fprintf(stderr, "fileoff field plus filesize field in %s extends past the end of the file)\n",\
					segcodes[error.command]);
				break;
			default:
				ft_fprintf(stderr, "%s)\n", errors[error.errcode]);
		}
	}

	return EXIT_FAILURE;
}

static int
read_macho_file (t_ofile *ofile, t_meta *meta) {

	int 				retcode = EXIT_SUCCESS;
	struct mach_header	*header = (struct mach_header *)ofile_extract(ofile, 0, sizeof(*header));
	size_t				offset = header_size[ofile->is_64];
	uint32_t			ncmds = oswap_32(ofile, header->ncmds);

	for (meta->k_command = 0; meta->k_command < ncmds; meta->k_command++) {

		struct load_command *loader = (struct load_command *)ofile_extract(ofile, offset, sizeof(*loader));

		if (loader->cmdsize % 8) return (meta->errcode = E_INVAL8), EXIT_FAILURE;

		uint32_t command = oswap_32(ofile, loader->cmd);

		if (command < ofile->n_command && ofile->reader[command]
			&& (retcode = ofile->reader[command](ofile, meta, offset)) != EXIT_SUCCESS) break;

		offset += oswap_32(ofile, loader->cmdsize);
	}

	return retcode;
}

int
open_file (const char *path, t_ofile *ofile, t_meta *meta) {

	int 		retcode = EXIT_SUCCESS;
	const int 	fd = open(path, O_RDONLY);
	struct stat	stat;

	if (fd == -1 || fstat(fd, &stat) == -1) return EXIT_FAILURE;

	ofile->size = (size_t)stat.st_size;
	if (ofile->size < sizeof(uint32_t)) return (errno = EBADMACHO), EXIT_FAILURE;
	if (stat.st_mode & S_IFDIR) return (errno = EISDIR), EXIT_FAILURE;

	ofile->file = mmap(NULL, ofile->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (close(fd) == -1 || ofile->file == MAP_FAILED) return EXIT_FAILURE;

	t_magic magic[] = {
			{MH_MAGIC, false, false, read_macho_file},
			{MH_MAGIC_64, true, false, read_macho_file},
			{MH_CIGAM, false, true, read_macho_file},
			{MH_CIGAM_64, true, true, read_macho_file}
	};
	const int magic_len = (int)(sizeof(magic) / sizeof(*magic));

	for (int k = 0; k < magic_len; k++) {

		if (*(uint32_t *)ofile->file == magic[k].magic) {

			ofile->is_64 = magic[k].is_64;
			ofile->is_cigam = magic[k].is_cigam;
			retcode = magic[k].read_file(ofile, meta);
			break;
		}

		if (k + 1 == magic_len) {

			meta->errcode = E_MAGIC;
			retcode = EXIT_FAILURE;
		}
	}

	munmap((void *)ofile->file, ofile->size);
	return retcode;
}
