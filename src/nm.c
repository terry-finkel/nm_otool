#include "nmp.h"


int
main (int argc, const char *argv[]) {

	int opt = 1;
	t_opt opts[] = {
		{FT_OPT_BOOLEAN, 'h',"-help",&opt,"Display available options",NM_OPT_h}
	};

	if (ft_optparse(opts, &opt, argc, (char **)argv)) {
		ft_optusage(opts, (char *)argv[0], "test2", "hi");
		return EXIT_FAILURE;
	};
	return 0;
}
