#ifndef OFILEP_H
# define OFILEP_H

# include <mach-o/arch.h>
# include <mach-o/loader.h>
# include "../libft/include/libft.h"

# define oswap_32(object, item) (object->is_cigam ? OSSwapConstInt32(item) : item)
# define oswap_64(object, item) (object->is_cigam ? OSSwapConstInt64(item) : item)

enum 					e_errcode {
	E_RRNO = 0,
	E_MAGIC,
	E_INVAL8,
	E_LOADOFF,
	E_SEGOFF,
	E_SECTOFF,
	E_FATOFF
};

enum 					e_type {
	E_MACHO,
	E_FAT,
	E_AR
};

typedef struct 			s_object {
	const void 			*object;
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
	bool				arch_output;
	bool				dump_all_arch;
	bool				dump_header;
	bool				dump_text;
}						t_ofile;

typedef struct			s_meta {
	const char			*bin;
	const char 			*path;
	const NXArchInfo	**nxArchInfo;
	uint8_t 			errcode: 4;
	uint8_t 			type: 4;
	uint32_t 			k_section;
	uint32_t			k_command;
	size_t 				n_command;
	uint32_t 			command;
	int 				(*reader[])(t_ofile *, t_object *, struct s_meta *, size_t);
}						t_meta;

int 					open_file(t_ofile *ofile, t_meta *meta);
int						printerr (const t_meta *meta);

#endif /* OFILEP_H */
