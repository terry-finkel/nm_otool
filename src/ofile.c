#include "ofilep.h"
#include <ar.h>
#include <fcntl.h>
#include <mach-o/fat.h>
#include <mach-o/ranlib.h>
#include <sys/mman.h>
#include <sys/stat.h>


#define SAR_EFMT1 3
#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)
#define XTEND "extends past the end of the file)\n"

static int				dispatch(t_ofile * ofile, t_object *object, t_meta *meta);

static const char		*errors[] = {
		"truncated or malformed",
		"is not an object file",
		"invalid magic",
		"cmdsize not a multiple of",
		"terminator characters in archive member not the correct",
		"offset to next archive member past the end of the archive after member",
		"extends past the end all load commands in the file)\n",
		"fileoff field plus filesize field",
		"offset plus size of",
		"bad string table index"
};

static const size_t		header_size[] = {
		[false] = sizeof(struct mach_header),
		[true] = sizeof(struct mach_header_64)
};

static const char 		*filecodes[] = {
		[E_MACHO] = "object",
		[E_FAT] = "fat file",
		[E_AR] = "archive"
};

static const char 		*segcodes[] = {
		[LC_SEGMENT] = "LC_SEGMENT",
		[LC_SEGMENT_64] = "LC_SEGMENT_64"
};


int
printerr (const t_meta *meta) {

	if (meta->errcode == E_RRNO) {

		ft_fprintf(stderr, "%s: \'%s\': %s\n", meta->bin, meta->path, strerror(errno));
	} else if (meta->errcode < E_INV4L) {

		ft_fprintf(stderr, "%s: %s\n", meta->path, errors[meta->errcode]);
	} else {

		ft_fprintf(stderr, "%s: \'%s\': %s %s ", meta->bin, meta->path, errors[0], filecodes[meta->type]);
		switch (meta->errcode) {
			case E_INV4L:
				ft_fprintf(stderr, "(load command %u %s %d)\n", meta->k_command, errors[meta->errcode],
						meta->arch ? 8 : 4);
				break;
			case E_ARFMAG:
				ft_fprintf(stderr, "(%s %s values for the archive member header for %s)\n", errors[meta->errcode],
						STR(ARFMAG), meta->ar_member);
				break;
			case E_AROFFSET:
				ft_fprintf(stderr, "(%s %s)\n", errors[meta->errcode], meta->ar_member);
				break;
			case E_LOADOFF:
				ft_fprintf(stderr, "(load command %u %s", meta->k_command + 1, errors[meta->errcode]);
				break;
			case E_SEGOFF:
				ft_fprintf(stderr, "(load command %u %s in %s %s", meta->k_command, errors[meta->errcode],
						segcodes[meta->command], XTEND);
				break;
			case E_FATOFF:
				ft_fprintf(stderr, "(%s cputype (%d) cpusubtype (%d) %s", errors[meta->errcode],
						(*meta->nxArchInfo)->cputype, (*meta->nxArchInfo)->cpusubtype, XTEND);
				break;
			case E_SYMSTRX:
			default:
				ft_fprintf(stderr, "%s: %d past the end of string table, for symbol at index %u\n",
						errors[meta->errcode], meta->n_strindex, meta->k_strindex);
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
read_macho_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	struct mach_header *header = (struct mach_header *)opeek(object, 0, sizeof *header);
	if (header == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

	uint32_t ncmds = oswap_32(object, header->ncmds);
	size_t offset = header_size[object->is_64];
	if (object->nxArchInfo == NULL) object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object,
			(uint32_t)header->cputype), (cpu_subtype_t)oswap_32(object, (uint32_t)header->cpusubtype));

	if ((ofile->opt & OTOOL_d) || (ofile->opt & OTOOL_t)) {

		if (meta->type == E_AR) {

			ft_dstrfpush(ofile->buffer, "%s(%s):\n", meta->path, object->name);
		} else {

			ft_dstrfpush(ofile->buffer, "%s", object->name);
			if (ofile->opt & ARCH_OUTPUT) ft_dstrfpush(ofile->buffer, " (architecture %s)", ofile->arch);
			ft_dstrfpush(ofile->buffer, ":\n");
		}
	}

	for (meta->k_command = 0; meta->k_command < ncmds; meta->k_command++) {

		const struct load_command *loader = (struct load_command *)opeek(object, offset, sizeof *loader);
		if (loader == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

		const uint32_t command = oswap_32(object, loader->cmd);
		const uint32_t cmdsize = oswap_32(object, loader->cmdsize);

		/* Various command checks. */
		if (offset + cmdsize > object->size) return (meta->errcode = E_LOADOFF), EXIT_FAILURE;
		if (cmdsize % (object->is_64 ? 8 : 4)) {

			meta->arch = object->is_64;
			meta->errcode = E_INV4L;
			return EXIT_FAILURE;
		}
		if (cmdsize == 0) {

			meta->k_command -= 1;
			meta->errcode = E_LOADOFF;
			return EXIT_FAILURE;
		}

		if (command < meta->n_command && meta->reader[command]
			&& meta->reader[command](ofile, object, meta, offset) != EXIT_SUCCESS) return EXIT_FAILURE;

		offset += oswap_32(object, loader->cmdsize);
	}

	if (ofile->opt & OTOOL_h) header_dump(ofile, object);

	return EXIT_SUCCESS;
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
dispatch_fat (t_ofile *ofile, t_object *object, t_meta *meta, const void *ptr) {

	if (object->fat_64 == false) {

		const struct fat_arch *arch = (struct fat_arch *)ptr;
		object->object = ofile->file + oswap_32(object, arch->offset);
		object->size = oswap_32(object, arch->size);
	} else {

		const struct fat_arch_64 *arch_64 = (struct fat_arch_64 *)ptr;
		object->object = ofile->file + oswap_64(object, arch_64->offset);
		object->size = oswap_64(object, arch_64->size);
	}

	const int retcode = dispatch(ofile, object, meta);
	object->is_64 = object->fat_64;
	object->is_cigam = object->fat_cigam;
	object->object = ofile->file;

	return retcode;
}

static int
read_fat_file (t_ofile *ofile, t_object *object, t_meta *meta) {

	struct fat_header *fat_header = (struct fat_header *)opeek(object, 0, sizeof *fat_header);
	if (fat_header == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

	/* is_64 and is_cigam will be overwritten by architecture files in the fat header, so we store them beforehand. */
	object->fat_64 = object->is_64;
	object->fat_cigam = object->is_cigam;
	size_t offset = sizeof *fat_header;

	if (meta->type != E_AR) meta->type = E_FAT;

	/*
	   If the "all" architecture flag hasn't been specified, we first iterate through every available architecture in
	   the fat object in order to find one that matches the specified one if given, or the host one if not.
	*/

	if (ft_strequ(ofile->arch, "all") == 0) {

		for (uint32_t k = 0; k < oswap_32(object, fat_header->nfat_arch); k++) {

			const struct fat_arch *fat_arch = (struct fat_arch *)opeek(object, offset, sizeof *fat_arch);
			if (fat_arch == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

			object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
					(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

			if (test_offset_fat_arch(ofile, object, meta, offset) == EXIT_FAILURE) return EXIT_FAILURE;
			if (ft_strequ(ofile->arch, object->nxArchInfo->name)) return dispatch_fat(ofile, object, meta, fat_arch);

			offset += sizeof *fat_arch;
		}

		if ((ofile->opt & DUMP_ALL_ARCH) == 0) {

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

	ofile->opt |= ARCH_OUTPUT;
	for (uint32_t k = 0; k < oswap_32(object, fat_header->nfat_arch); k++) {

		/* Mutate object architecture specifications to treat it as an independent Mach-O file. */
		const struct fat_arch *fat_arch = (struct fat_arch *)opeek(object, offset, sizeof *fat_arch);
		if (fat_arch == NULL) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;

		object->nxArchInfo = NXGetArchInfoFromCpuType((cpu_type_t)oswap_32(object, (uint32_t)fat_arch->cputype),
				(cpu_subtype_t)oswap_32(object, (uint32_t)fat_arch->cpusubtype));

		if (test_offset_fat_arch(ofile, object, meta, offset) != EXIT_SUCCESS) return EXIT_FAILURE;
		ofile->arch = object->nxArchInfo->name;
		if (dispatch_fat(ofile, object, meta, fat_arch) != EXIT_SUCCESS) return EXIT_FAILURE;

		/* Output object from the fat file and clear buffer. */
		ft_fprintf(stdout, ofile->buffer->buff);
		ft_dstrclr(ofile->buffer);
		offset += sizeof *fat_arch;
	}

	return EXIT_SUCCESS;
}

static int
process_archive (t_ofile *ofile, t_object *object, t_meta *meta, size_t *offset) {

	int retcode = EXIT_SUCCESS;
	const struct ar_hdr *ar_hdr = (struct ar_hdr *)opeek(object, *offset, sizeof *ar_hdr);
	if (ar_hdr == NULL) return (meta->errcode = E_AROFFSET), EXIT_FAILURE;

	/* Consistency check. If it fails, don't return immediately so we can save the archive name to display it later. */
	if (ft_strnequ(ARFMAG, ar_hdr->ar_fmag, 2) == 0) {

		meta->errcode = E_ARFMAG;
		retcode = EXIT_FAILURE;
	}

	const int size = ft_atoi(ar_hdr->ar_size);
	if (size < 0) return EXIT_FAILURE; /* ERRNO */

	/* Populate our object. */
	*offset += sizeof *ar_hdr;
	object->size = (size_t)size;
	int name_size = 0;
	if (ft_strnequ(ar_hdr->ar_name, AR_EFMT1, SAR_EFMT1)) {

		name_size = ft_atoi(ar_hdr->ar_name + SAR_EFMT1);
		if (name_size < 0) return EXIT_FAILURE; /* ERRNO */

		object->object = ofile->file + *offset + name_size;
		object->name = ofile->file + *offset;
	} else {

		object->object = ofile->file + *offset;
		object->name = ar_hdr->ar_name;
	}

	/* Adujst size if name is EFMT1 */
	object->size -= (size_t)name_size;

	if (*offset + object->size > ofile->size) {

		meta->errcode = E_AROFFSET;
		retcode = EXIT_FAILURE;
	}

	*offset += object->size + name_size;
	meta->ar_member = ft_strdup(object->name);
	return retcode;
}

static int
read_archive (t_ofile *ofile, t_object *object, t_meta *meta) {

	meta->type = E_AR;
	size_t offset = SARMAG;

	if (process_archive(ofile, object, meta, &offset) != EXIT_SUCCESS) return EXIT_FAILURE;

	/* Check SYMDEF validity. */
	const char *symdef = ofile->file + sizeof(struct ar_hdr) + SARMAG;
	if (ft_strequ(symdef, SYMDEF) == 0 && ft_strequ(symdef, SYMDEF_SORTED) == 0 && ft_strequ(symdef, SYMDEF_64) == 0
	&& ft_strequ(symdef, SYMDEF_64_SORTED) == 0) return EXIT_FAILURE; /* ERRNO */

	/* Archive looks valid so far, let's loop through each of it's member. */
	ft_dstrfpush(ofile->buffer, "Archive : %s\n", meta->path);
	while (offset != ofile->size) {

		/* Restore full object. */
		object->object = ofile->file;
		object->size = ofile->size;

		if (process_archive(ofile, object, meta, &offset) != EXIT_SUCCESS) return EXIT_FAILURE;
		if (dispatch(ofile, object, meta) != EXIT_SUCCESS) return EXIT_FAILURE;

		ft_fprintf(stdout, ofile->buffer->buff);
		ft_dstrclr(ofile->buffer);
	}

	return EXIT_SUCCESS;
}

static int
dispatch (t_ofile *ofile, t_object *object, t_meta *meta) {

	struct s_magic {
		const char	*arch_magic;
		uint32_t 	magic;
		bool		is_64;
		bool		is_cigam;
		int 		(*read_file)(t_ofile *, t_object *, t_meta *);
	} magic[] = {
			{0, MH_MAGIC, false, false, read_macho_file},
			{0, MH_CIGAM, false, true, read_macho_file},
			{0, MH_MAGIC_64, true, false, read_macho_file},
			{0, MH_CIGAM_64, true, true, read_macho_file},
			{0, FAT_MAGIC, false, false, read_fat_file},
			{0, FAT_CIGAM, false, true, read_fat_file},
			{0, FAT_MAGIC_64, true, false, read_fat_file},
			{0, FAT_CIGAM_64, true, true, read_fat_file},
			{"!<arch>\n", 0, 0, 0, read_archive},
	};

	/* We add 2 to the magic_len as we also need to check for archives. */
	const int magic_len = (int)(sizeof magic / sizeof *magic);

	for (int k = 0; k < magic_len; k++) {

		if (meta->type == E_FAT && k == 4) return (meta->errcode = E_MAGIC), EXIT_FAILURE;
		if (meta->type == E_AR && k == 8) return (meta->errcode = E_MAGIC), EXIT_FAILURE;

		/* Find the magic number of the object. Then dispatch it to the correct reader. */
		if ((magic[k].arch_magic != NULL && ft_strnequ(magic[k].arch_magic, object->object, SARMAG))
		|| *(uint32_t *)object->object == magic[k].magic) {

			object->is_64 = magic[k].is_64;
			object->is_cigam = magic[k].is_cigam;
			return magic[k].read_file(ofile, object, meta);
		}
	}

	/* If nothing matched, the file isn't valid. */
	meta->errcode = E_GARBAGE;
	return EXIT_FAILURE;
}

int
open_file (t_ofile *ofile, t_meta *meta) {

	/* Open the file, perform various checks and map it into memory. */
	const int 	fd = open(meta->path, O_RDONLY);
	struct stat	stat;

	if (fd == -1 || fstat(fd, &stat) == -1) return EXIT_FAILURE;

	ofile->size = (size_t)stat.st_size;
	if (ofile->size < sizeof(uint32_t)) return (meta->errcode = E_GARBAGE), EXIT_FAILURE;
	if (stat.st_mode & S_IFDIR) return (errno = EISDIR), EXIT_FAILURE;

	ofile->file = mmap(NULL, ofile->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (close(fd) == -1 || ofile->file == MAP_FAILED) return EXIT_FAILURE;

	/*
	   We duplicate the file and the file size into the object structure as well, it will allow us to process FAT
	   objects independently.
	*/

	t_object object = {
			.object = ofile->file,
			.name = meta->path,
			.size = ofile->size,
	};
	meta->nxArchInfo = &object.nxArchInfo;

	/* Send the file to the generic dispatcher. */
	int retcode = dispatch(ofile, &object, meta);

	munmap((void *)ofile->file, ofile->size);
	return retcode;
}
