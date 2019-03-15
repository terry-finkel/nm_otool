#include "ofilep.h"


static const char	*errors[] = {
	"truncated or malformed object",
	"is not an object file",
	"load command cmdsize not a multiple of 8"
};

static const size_t	header_size[] = {
	[false] = sizeof(struct mach_header),
	[true] = sizeof(struct mach_header_64)
};


int
printerr (const char *bin, const char *file, int error) {

	switch(error) {
		case E_FAILURE:
			ft_fprintf(stderr, "%s: %s: %s.\n", bin, file, strerror(errno));
			break;
		case E_INVAL8:
			ft_fprintf(stderr, "%s: \'%s\': %s (load command %d cmdsize not a multiple of 8)\n", bin, file, errors[0],\
				errno);
			break;
		default:
			ft_fprintf(stderr, "%s: %s\n", file, errors[error]);
	}

	return EXIT_FAILURE;
}

static int
read_macho_file (const char *path, t_ofile *ofile) {

	int 				retcode = E_SUCCESS;
	struct mach_header	*header = (struct mach_header *)ofile_extract(ofile, 0, sizeof(*header));
	size_t				offset = header_size[ofile->is_64];
	uint32_t			ncmds = oswap_32(ofile, header->ncmds);

	for (uint32_t k = 0; k < ncmds; k++) {

		struct load_command *loader = (struct load_command *)ofile_extract(ofile, offset, sizeof(*loader));
		if (loader->cmdsize % 8) return (errno = (int)k), E_INVAL8;

		uint32_t command = oswap_32(ofile, loader->cmd);

		if (command < ofile->ncommand && ofile->reader[command]
			&& (retcode = ofile->reader[command](path, ofile, offset)) != E_SUCCESS) break;

		offset += oswap_32(ofile, loader->cmdsize);
	}

	return retcode;
}

int
open_file (const char *path, t_ofile *ofile) {

	const int 	fd = open(path, O_RDONLY);
	int 		retcode = E_SUCCESS;
	struct stat stat;

	if (fd == -1 || fstat(fd, &stat) == -1) return E_FAILURE;

	ofile->size = (size_t)stat.st_size;
	if (ofile->size < sizeof(uint32_t)) return (errno = EBADMACHO), E_FAILURE;
	if (stat.st_mode & S_IFDIR) return (errno = EISDIR), E_FAILURE;

	ofile->file = mmap(NULL, ofile->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (close(fd) == -1 || ofile->file == MAP_FAILED) return E_FAILURE;

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
			retcode = magic[k].read_file(path, ofile);
			break;
		}

		if (k + 1 == magic_len) return E_INVAL;
	}

	munmap((void *)ofile->file, ofile->size);
	return retcode;
}
