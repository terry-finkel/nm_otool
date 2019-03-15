#ifndef OFILEP_H
# define OFILEP_H

# include <mach-o/arch.h>
# include <mach-o/loader.h>
# include "../libft/include/libft.h"

# define ofile_extract(ofile, offset, peek_size) ((offset + peek_size > ofile->size) ? NULL : ofile->file + offset)
# define oswap_32(ofile, item) (ofile->is_cigam ? OSSwapConstInt32(item) : item)
# define oswap_64(ofile, item) (ofile->is_cigam ? OSSwapConstInt64(item) : item)

enum 					e_error {
	E_RRNO = 0,
	E_MAGIC,
	E_INVAL8,
	E_INVALSEGOFF
};

typedef struct			s_meta {
	const char			*bin;
	const char 			*file;
	uint32_t			k_command;
	uint32_t 			command;
	uint8_t 			errcode;
}						t_meta;

typedef struct			s_ofile {
	const void			*file;
	const NXArchInfo	*nxArchInfo;
	t_dstr				*buffer;
	bool				is_64: 1;
	bool				is_cigam: 1;
	size_t 				n_command;
	size_t 				size;
	int 				(*reader[])(struct s_ofile *, t_meta *, size_t);
}						t_ofile;

int 					open_file(const char *path, t_ofile *ofile, t_meta *meta);
int						printerr (t_meta error);

#endif /* OFILEP_H */
