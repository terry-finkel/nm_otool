#include "ofilep.h"
#include <mach-o/fat.h>

/* For open. */
# include <fcntl.h>

/* For mmap and munmap. */
# include <sys/mman.h>

/* For fstat. */
# include <sys/stat.h>

static int				dispatch(t_ofile * ofile, t_object *object, t_meta *meta);

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
printerr (t_ofile ofile, t_meta meta) {

	if (ofile.errcode == E_RRNO) {

		ft_fprintf(stderr, "%s: \'%s\': %s\n", ofile.bin, ofile.path, strerror(errno));
	} else if (ofile.errcode == E_MAGIC) {

		ft_fprintf(stderr, "%s: %s\n", ofile.path, errors[ofile.errcode]);
	} else {

		ft_fprintf(stderr, "%s: \'%s\': %s (load command %d ", ofile.bin, ofile.path, errors[0], meta.k_command);
		switch (ofile.errcode) {
			case E_INVALSEGOFF:
				ft_fprintf(stderr, "fileoff field plus filesize field in %s extends past the end of the file)\n",\
					segcodes[meta.command]);
				break;
			default:
				ft_fprintf(stderr, "%s)\n", errors[ofile.errcode]);
		}
	}

	return EXIT_FAILURE;
}

static int
read_macho_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	int 				retcode = EXIT_SUCCESS;
	struct mach_header	*header = (struct mach_header *)object_extract(object, 0, sizeof *header);
	uint32_t			ncmds = oswap_32(object, header->ncmds);
	size_t 				offset = header_size[object->is_64];

	if (object->nxArchInfo == NULL) {
		object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)header->cputype),
				(cpu_subtype_t)oswap_32(object, (uint32_t)header->cpusubtype));
	}

	for (meta->k_command = 0; meta->k_command < ncmds; meta->k_command++) {

		const struct load_command *loader = (struct load_command *)object_extract(object, offset, sizeof *loader);
		if (oswap_32(object, loader->cmdsize) % (object->is_64 ? 8 : 4)) return (ofile->errcode = E_INVAL8), EXIT_FAILURE;

		uint32_t command = oswap_32(object, loader->cmd);

		if (command < meta->n_command && meta->reader[command]
			&& (retcode = meta->reader[command](ofile, object, meta, offset)) != EXIT_SUCCESS) break;

		offset += oswap_32(object, loader->cmdsize);
	}

	return retcode;
}

static int
dispatch_fat (t_ofile *ofile, t_object *object, t_meta *meta, const struct fat_arch *arch) {

	int	retcode;

	if (object->fat_64 == false) {

		object->object = ofile->file + oswap_32(object, arch->offset);
		object->size = oswap_32(object, arch->size);
		retcode = dispatch(ofile, object, meta);
		object->is_64 = object->fat_64;
		object->is_cigam = object->fat_cigam;
		object->object = ofile->file;
	} else {

		retcode = dispatch(ofile, object, meta);
	}

	return retcode;
}

static int
read_fat_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	int 				retcode = EXIT_SUCCESS;
	struct fat_header	*fat_header = (struct fat_header *)object_extract(object, 0, sizeof *fat_header);
	size_t 				offset = sizeof *fat_header;
	uint32_t 			nfat_arch = oswap_32(object, fat_header->nfat_arch);

	ofile->type = E_FAT;
	object->fat_64 = object->is_64;
	object->fat_cigam = object->is_cigam;

	/*
	   If the "all" architecture flag hasn't been specified, we first iterate through every available architecture in
	   the fat object in order to find one that matches the specified one if given, or the host one if not.
	*/

	if (ft_strequ(ofile->arch, "all") == 0) {

		for (uint32_t k = 0; k < nfat_arch; k++) {

			const struct fat_arch *fat_arch = (struct fat_arch *)object_extract(object, offset, sizeof *fat_arch);
			object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
					(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

			if (ft_strequ(ofile->arch, object->nxArchInfo->name)) return dispatch_fat(ofile, object, meta, fat_arch);

			offset += sizeof *fat_arch;
		}

		if (ofile->dump_all == false) {

			/* The architecture hasn't been found. */
			ft_printf("%s: file: %s does not contain architecture: %s\n", ofile->bin, ofile->path, ofile->arch);
			return EXIT_SUCCESS;
		}

		offset = sizeof *fat_header;
	}

	ofile->arch_output = true;
	for (uint32_t k = 0; k < nfat_arch; k++) {

		const struct fat_arch *fat_arch = (struct fat_arch *)object_extract(object, offset, sizeof *fat_arch);
		object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
				(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));
		ofile->arch = object->nxArchInfo->name;

		if ((retcode = dispatch_fat(ofile, object, meta, fat_arch) != EXIT_SUCCESS)) break;

		offset += sizeof *fat_arch;
	}

	return retcode;
}

static int
dispatch (t_ofile *ofile, t_object *object, t_meta *meta) {

	int retcode = EXIT_SUCCESS;
	struct s_magic {
		uint32_t 	magic;
		bool		is_64: 1;
		bool		is_cigam: 1;
		int 		(*read_file)(t_ofile *ofile, t_object *object, t_meta *);
	} magic[] = {
			{MH_MAGIC, false, false, read_macho_file},
			{MH_CIGAM, false, true, read_macho_file},
			{MH_MAGIC_64, true, false, read_macho_file},
			{MH_CIGAM_64, true, true, read_macho_file},
			{FAT_MAGIC, false, false, read_fat_file},
			{FAT_CIGAM, false, true, read_fat_file},
			{FAT_MAGIC_64, true, false, read_fat_file},
			{FAT_CIGAM_64, true, true, read_fat_file}
	};
	const int magic_len = (int)(sizeof magic / sizeof *magic);

	for (int k = 0; k < magic_len; k++) {

		/* Find the magic number of the object. Then dispatch it to the correct reader. */
		if (*(uint32_t *)object->object == magic[k].magic) {

			object->is_64 = magic[k].is_64;
			object->is_cigam = magic[k].is_cigam;
			retcode = magic[k].read_file(ofile, object, meta);
			break;
		}

		/* The first 4 bytes didn't match any magic number, object is invalid. */
		if (k + 1 == magic_len) {

			ofile->errcode = E_MAGIC;
			retcode = EXIT_FAILURE;
		}
	}

	return retcode;
}

int
open_file (t_ofile *ofile, t_meta *meta) {

	/* Open the file, perform various checks and map it into memory. */
	const int 	fd = open(ofile->path, O_RDONLY);
	struct stat	stat;

	if (fd == -1 || fstat(fd, &stat) == -1) return EXIT_FAILURE;

	ofile->size = (size_t)stat.st_size;
	if (ofile->size < sizeof(uint32_t)) return (errno = EBADMACHO), EXIT_FAILURE;
	if (stat.st_mode & S_IFDIR) return (errno = EISDIR), EXIT_FAILURE;

	ofile->file = mmap(NULL, ofile->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (close(fd) == -1 || ofile->file == MAP_FAILED) return EXIT_FAILURE;

	/*
	   We duplicate the file and the file size into the object structure as well, it will allow us to process FAT
	   objects independently.
	*/

	t_object object = {
			.object = ofile->file,
			.size = ofile->size,
	};

	int retcode = dispatch(ofile, &object, meta);
	munmap((void *)ofile->file, ofile->size);
	return retcode;
}
