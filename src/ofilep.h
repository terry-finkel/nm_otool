#ifndef OFILEP_H
# define OFILEP_H

/* For open. */
# include <fcntl.h>

/* For mmap and munmap. */
# include <sys/mman.h>

/* For fstat. */
# include <sys/stat.h>

# include <mach-o/loader.h>
# include "../libft/include/libft.h"

# define ofile_extract(ofile, offset, peek_size) (offset + peek_size > ofile->size ? NULL : ofile->file + offset)
# define oswap_32(ofile, item) (ofile->is_cigam ? OSSwapConstInt32(item) : item)
# define oswap_64(ofile, item) (ofile->is_cigam ? OSSwapConstInt64(item) : item)

enum 			e_error {
	E_FAILURE = -1,
	E_SUCCESS,
	E_INVAL,
	E_INVAL8
};

typedef struct	s_ofile {
	const void *file;
	bool		is_64: 1;
	bool		is_cigam: 1;
	size_t 		ncommand;
	size_t 		size;
	int 		(*reader[])(const char *, struct s_ofile *, size_t);
}				t_ofile;

typedef struct	s_magic {
	uint32_t 	magic;
	bool		is_64: 1;
	bool		is_cigam: 1;
	int 		(*read_file)(const char *, t_ofile *);
}				t_magic;

int 			open_file(const char *path, t_ofile *ofile);
int				printerr (const char *bin, const char *file, int error);

#endif /* OFILEP_H */
