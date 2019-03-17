#include "ofilep.h"
#include <fcntl.h>
#include <mach-o/fat.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define XTEND "extends past the end of the file)\n"

static int				dispatch(t_ofile * ofile, t_object *object, t_meta *meta);

static const char		*errors[] = {
		"truncated or malformed",
		"is not an object file",
		"cmdsize not a multiple of 8",
		"extends past the end all load commands in the file)\n",
		"fileoff field plus filesize field",
		"offset field plus size field of section",
		"offset plus size of"
};

static const size_t		header_size[] = {
		[false] = sizeof(struct mach_header),
		[true] = sizeof(struct mach_header_64)
};

static const char 		*filecodes[] = {
		[E_MACHO] = "object",
		[E_FAT] = "fat file"
};

static const char 		*segcodes[] = {
		[LC_SEGMENT] = "LC_SEGMENT",
		[LC_SEGMENT_64] = "LC_SEGMENT_64"
};


int
printerr (const t_meta *meta) {

	if (meta->errcode == E_RRNO) {

		ft_fprintf(stderr, "%s: \'%s\': %s\n", meta->bin, meta->path, strerror(errno));
	} else if (meta->errcode == E_MAGIC) {

		ft_fprintf(stderr, "%s: %s\n", meta->path, errors[meta->errcode]);
	} else {

		ft_fprintf(stderr, "%s: \'%s\': %s %s ", meta->bin, meta->path, errors[0], filecodes[meta->type]);
		switch (meta->errcode) {
			case E_LOADOFF:
				ft_fprintf(stderr, "(load command %u %s", meta->k_command + 1, errors[meta->errcode]);
				break;
			case E_SEGOFF:
				ft_fprintf(stderr, "(load command %u %s in %s %s", meta->k_command, errors[meta->errcode],
					segcodes[meta->command], XTEND);
				break;
			case E_SECTOFF:
				ft_fprintf(stderr, "(%s %u in %s command %u %s", errors[meta->errcode], meta->k_section,\
					segcodes[meta->command], meta->k_command, XTEND);
				break;
			case E_FATOFF:
				ft_fprintf(stderr, "(%s cputype (%d) cpusubtype (%d) %s", errors[meta->errcode],\
					(*meta->nxArchInfo)->cputype, (*meta->nxArchInfo)->cpusubtype, XTEND);
				break;
			default:
				ft_fprintf(stderr, "%s)\n", errors[meta->errcode]);
		}
	}

	return EXIT_FAILURE;
}

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
	ft_dstrfpush(ofile->buffer, "%11#x %7d %10d %5.2#p %11u %5u %10u %#.8x\n", magic, cputype, cpusubtype,
				 caps ? 128 : 0, filetype, ncmds, sizeofcmds, flags);
}

static int
test_offset_fat_arch (t_ofile *ofile, t_object *object, t_meta *meta, size_t offset) {

	int retcode = EXIT_SUCCESS;

	if (object->fat_64) {

		const struct fat_arch_64 *fat_arch = (struct fat_arch_64 *)(object->object + offset);
		if (oswap_64(object, fat_arch->offset) + oswap_64(object, fat_arch->size) > ofile->size) retcode = EXIT_FAILURE;
	} else {

		const struct fat_arch *fat_arch = (struct fat_arch *)(object->object + offset);
		if (oswap_32(object, fat_arch->offset) + oswap_32(object, fat_arch->size) > ofile->size) retcode = EXIT_FAILURE;
	}

	if (retcode == EXIT_FAILURE) meta->errcode = E_FATOFF;

	return retcode;
}

static int
read_macho_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	int 				retcode = EXIT_SUCCESS;
	struct mach_header	*header = (struct mach_header *)object->object;
	uint32_t			ncmds = oswap_32(object, header->ncmds);
	size_t 				offset = header_size[object->is_64];

	if (object->nxArchInfo == NULL) {
		object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)header->cputype),
				(cpu_subtype_t)oswap_32(object, (uint32_t)header->cpusubtype));
	}

	if (ofile->dump_text) ft_dstrfpush(ofile->buffer, "%s", meta->path);
	if (ofile->dump_text && ofile->arch_output) ft_dstrfpush(ofile->buffer, " (architecture %s)", ofile->arch);
	if (ofile->dump_text) ft_dstrfpush(ofile->buffer, ":\n");

	for (meta->k_command = 0; meta->k_command < ncmds; meta->k_command++) {

		const struct load_command *loader = (struct load_command *)(object->object + offset);
		if (oswap_32(object, loader->cmdsize) % (object->is_64 ? 8 : 4)) return (meta->errcode = E_INVAL8), EXIT_FAILURE;
		if (oswap_32(object, loader->cmdsize) > ofile->size) return (meta->errcode = E_LOADOFF), EXIT_FAILURE;

		uint32_t command = oswap_32(object, loader->cmd);

		if (command < meta->n_command && meta->reader[command]
			&& (retcode = meta->reader[command](ofile, object, meta, offset)) != EXIT_SUCCESS) break;

		offset += oswap_32(object, loader->cmdsize);
	}

	if (ofile->dump_header) header_dump(ofile, object);

	return retcode;
}

static int
dispatch_fat (t_ofile *ofile, t_object *object, t_meta *meta, const void *ptr) {

	if (object->fat_64 == false) {

		const struct fat_arch *arch = (struct fat_arch *)ptr;
		object->object = ofile->file + oswap_32(object, arch->offset);
		object->size = oswap_32(object, arch->size);
	} else {

		const struct fat_arch_64 *arch = (struct fat_arch_64 *)ptr;
		object->object = ofile->file + oswap_64(object, arch->offset);
		object->size = oswap_64(object, arch->size);
	}

	const int retcode = dispatch(ofile, object, meta);
	object->is_64 = object->fat_64;
	object->is_cigam = object->fat_cigam;
	object->object = ofile->file;

	return retcode;
}

static int
read_fat_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	int 				retcode = EXIT_SUCCESS;
	struct fat_header	*fat_header = (struct fat_header *)object->object;
	size_t 				offset = sizeof *fat_header;
	uint32_t 			nfat_arch = oswap_32(object, fat_header->nfat_arch);

	/* is_64 and is_cigam will be overwritten by architecture files in the fat header, so we store them beforehand. */
	meta->type = E_FAT;
	object->fat_64 = object->is_64;
	object->fat_cigam = object->is_cigam;

	/*
	   If the "all" architecture flag hasn't been specified, we first iterate through every available architecture in
	   the fat object in order to find one that matches the specified one if given, or the host one if not.
	*/

	if (ft_strequ(ofile->arch, "all") == 0) {

		for (uint32_t k = 0; k < nfat_arch; k++) {

			const struct fat_arch *fat_arch = (struct fat_arch *)(object->object + offset);
			object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
					(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

			if (test_offset_fat_arch(ofile, object, meta, offset) == EXIT_FAILURE) return EXIT_FAILURE;
			if (ft_strequ(ofile->arch, object->nxArchInfo->name)) return dispatch_fat(ofile, object, meta, fat_arch);

			offset += sizeof *fat_arch;
		}

		if (ofile->dump_all_arch == false) {

			/* The architecture hasn't been found. */
			ft_printf("%s: file: %s does not contain architecture: %s\n", meta->bin, meta->path, ofile->arch);
			return EXIT_SUCCESS;
		}

		offset = sizeof *fat_header;
	}

	/*
	   If this code is reached, either "all" was specified, or the host architecture wasn't found in the fat file.
	   Either way, we can dump everything.
	*/

	ofile->arch_output = true;
	for (uint32_t k = 0; k < nfat_arch; k++) {

		/* Mutate object architecture specifications to treat it as an independent Mach-O file. */
		const struct fat_arch *fat_arch = (struct fat_arch *)(object->object + offset);
		object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
				(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

		if (test_offset_fat_arch(ofile, object, meta, offset) == EXIT_FAILURE) return EXIT_FAILURE;

		ofile->arch = object->nxArchInfo->name;
		if (dispatch_fat(ofile, object, meta, fat_arch) != EXIT_SUCCESS) {

			printerr(meta);
			retcode = EXIT_FAILURE;
		}

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

			meta->errcode = E_MAGIC;
			retcode = EXIT_FAILURE;
		}
	}

	return retcode;
}

int
open_file (t_ofile *ofile, t_meta *meta) {

	/* Open the file, perform various checks and map it into memory. */
	const int 	fd = open(meta->path, O_RDONLY);
	struct stat	stat;

	if (fd == -1 || fstat(fd, &stat) == -1) return EXIT_FAILURE;

	ofile->size = (size_t)stat.st_size;
	if (ofile->size < sizeof(uint32_t)) return (meta->errcode = E_MAGIC), EXIT_FAILURE;
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
	meta->nxArchInfo = &object.nxArchInfo;

	/* Send the file to the generic dispatcher. */
	int retcode = dispatch(ofile, &object, meta);

	munmap((void *)ofile->file, ofile->size);
	return retcode;
}
