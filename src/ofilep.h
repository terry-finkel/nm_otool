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

typedef struct						s_ctx {
	bool 							is_64: 1;
	bool							is_cigam: 1;
	union							{
		struct segment_command		__seg;
		struct segment_command_64	__seg64;
	}								u_seg;
}									t_ctx;

# define seg u_seg.__seg
# define seg64 u_seg.__seg64

#endif /* OFILEP_H */
