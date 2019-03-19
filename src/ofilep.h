#ifndef OFILEP_H
# define OFILEP_H

# include <mach-o/arch.h>
# include <mach-o/loader.h>
# include <mach-o/nlist.h>
# include "../libft/include/libft.h"

# define opeek(object, offset, osize) (offset + osize > object->size ? NULL : object->object + offset)
# define oswap_32(object, item) (object->is_cigam ? OSSwapConstInt32(item) : item)
# define oswap_64(object, item) (object->is_cigam ? OSSwapConstInt64(item) : item)

enum 					e_errcode {
	E_RRNO = 0,
	E_GARBAGE,
	E_MAGIC,
	E_INV4L,
	E_ARFMAG,
	E_AROFFSET,
	E_LOADOFF,
	E_SEGOFF,
	E_FATOFF,
	E_SYMSTRX
};


enum					e_opts {
	ARCH_OUTPUT = (1 << 0),
	DUMP_ALL_ARCH = (1 << 1),
	NM_a = (1 << 2),
	OTOOL_d = (1 << 3),
	OTOOL_h = (1 << 4),
	OTOOL_t = (1 << 5)
};

enum 					e_type {
	E_MACHO,
	E_FAT,
	E_AR
};

typedef struct 			s_object {
	const void 			*object;
	const char 			*name;
	size_t 				size;
	const NXArchInfo	*nxArchInfo;
	bool				is_64;
	bool				is_cigam;
	bool				fat_64;
	bool				fat_cigam;
}						t_object;

typedef struct			s_ofile {
	const char 			*arch;
	const void			*file;
	t_dstr				*buffer;
	size_t				size;
	uint8_t 			opt;
}						t_ofile;

typedef struct			s_meta {
	const char			*bin;
	const char 			*path;
	const char 			*ar_member;
	const NXArchInfo	**nxArchInfo;
	uint8_t 			errcode: 4;
	uint8_t 			type: 3;
	bool				arch;
	uint32_t			k_command;
	uint32_t 			k_strindex;
	union {
		uint32_t 		__n_command;
		int 			__n_strindex;
	}					n_err;
	uint32_t 			command;
	int 				(*reader[])(t_ofile *, t_object *, struct s_meta *, size_t);
}						t_meta;

# define n_command		n_err.__n_command
# define n_strindex		n_err.__n_strindex

int 					open_file(t_ofile *ofile, t_meta *meta);
int						printerr (const t_meta *meta);

#endif /* OFILEP_H */
