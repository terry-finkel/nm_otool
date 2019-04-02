#ifndef OFILEP_H
# define OFILEP_H

# include <mach-o/arch.h>
# include <mach-o/loader.h>
# include <mach-o/nlist.h>
# include "../libft/include/libft.h"

# define opeek(object, offset, osize) (offset + osize > object->size ? NULL : object->object + offset)
# define oswap_32(object, item) (object->is_cigam ? OSSwapConstInt32(item) : item)
# define oswap_64(object, item) (object->is_cigam ? OSSwapConstInt64(item) : item)

enum                    e_errcode {
    E_RRNO,
    E_GARBAGE,
    E_MAGIC,
    E_INV4L,
    E_ARFMAG,
    E_AROFFSET,
    E_AROVERLAP,
    E_LOADOFF,
    E_SEGOFF,
    E_FATOFF,
    E_SYMSTRX
};

enum                    e_obin {
    FT_NM,
    FT_OTOOL
};

enum                    e_opts {
    ARCH_OUTPUT = (1 << 0),
    NAME_OUTPUT = (1 << 1),
    NM_a = (1 << 2),
    NM_g = (1 << 3),
    NM_j = (1 << 4),
    NM_n = (1 << 5),
    NM_p = (1 << 6),
    NM_r = (1 << 7),
    NM_u = (1 << 8),
    NM_U = (1 << 9),
    OTOOL_d = (1 << 10),
    OTOOL_h = (1 << 11),
    OTOOL_t = (1 << 12)
};

enum                    e_type {
    E_MACHO,
    E_FAT,
    E_AR
};

typedef struct          s_object {
    const void          *object;
    const char          *name;
    size_t              size;
    const NXArchInfo    *nxArchInfo;
    bool                is_64;
    bool                is_cigam;
    uint8_t             k_sect;
}                       t_object;

typedef struct          s_ofile {
    const char          *arch;
    const void          *file;
    t_dstr              *buffer;
    size_t              size;
    uint16_t            opt;
}                       t_ofile;

typedef struct          s_meta {
    const char          *bin;
    const char          *path;
    const char          *ar_member;
    uint8_t             errcode: 4;
    uint8_t             type: 2;
    uint8_t             obin: 1;
    bool                arch;
    uint32_t            k_command; //TODO put in union
    union {
        uint32_t        k_strindex;
        int             k_cpu;
    }                   u_k;
    union {
        int             n_strindex;
        int             n_cpu;
    }                   u_n;
    uint32_t            command; //TODO put in union
    int                 (*reader[])(t_ofile *, t_object *, struct s_meta *, size_t);
}                       t_meta;

int                     open_file(t_ofile *ofile, t_meta *meta);
int                     printerr (const t_meta *meta);

#endif /* OFILEP_H */
