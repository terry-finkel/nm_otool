#################
##  VARIABLES  ##
#################

#	Environment
OS :=					$(shell uname -s)

#	Output
NM :=					ft_nm
OTOOL :=				ft_otool

#	Compiler
CC :=					gcc

#	Flags
FLAGS =					-Wall -Wextra -Wcast-align -Wconversion -Werror -g3
ifeq ($(OS), Darwin)
	THREADS :=			$(shell sysctl -n hw.ncpu)
else
	THREADS :=			4
endif
HEADERS :=				-I $(LIBFTDIR)/include

FAST :=					-j$(THREADS)
O_FLAG :=				-O0

#	Directories
LIBFTDIR :=				./libft/
OBJDIR :=				./build/
SRCDIR :=				./src/

#	Sources
NM_SRCS +=				ofile.c nm.c
OTOOL_SRCS +=			ofile.c otool.c
OBJECTS +=				$(patsubst %.c,$(OBJDIR)%.o,$(NM_SRCS))
OBJECTS +=				$(patsubst %.c,$(OBJDIR)%.o,$(OTOOL_SRCS))

vpath %.c $(SRCDIR)

#################
##    RULES    ##
#################

all: $(NM) $(OTOOL)

$(NM): libft $(OBJECTS)
	@$(CC) $(FLAGS) $(O_FLAG) $(patsubst %.c,$(OBJDIR)%.o,$(notdir $(NM_SRCS))) -L $(LIBFTDIR) -lft -o $@
	@printf  "\033[92m\033[1;32mCompiling -------------> \033[91m$(NM)\033[0m\033[1;32m:\033[0m%-15s\033[32m[✔]\033[0m\n"

$(OTOOL): libft $(OBJECTS)
	@$(CC) $(FLAGS) $(O_FLAG) $(patsubst %.c,$(OBJDIR)%.o,$(notdir $(OTOOL_SRCS))) -L $(LIBFTDIR) -lft -o $@
	@printf  "\033[92m\033[1;32mCompiling -------------> \033[91m$(OTOOL)\033[0m\033[1;32m:\033[0m%-12s\033[32m[✔]\033[0m\n"

$(OBJECTS): | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)%.o: %.c
	@printf  "\033[1;92mCompiling $(NM)/$(OTOOL)\033[0m %-21s\033[32m[$<]\033[0m\n"
	@$(CC) $(FLAGS) $(O_FLAG) $(HEADERS) -fpic -c $< -o $@
	@printf "\033[A\033[2K"

clean:
	@/bin/rm -rf $(OBJDIR)
	@printf  "\033[1;32mCleaning object files -> \033[91m$(NM)/$(OTOOL)\033[0m\033[1;32m:\033[0m%-6s\033[32m[✔]\033[0m\n"

fast:
	@$(MAKE) --no-print-directory $(FAST)

fclean: clean
	@/bin/rm -f $(NM)
	@/bin/rm -f $(OTOOL)
	@printf  "\033[1;32mCleaning binary -------> \033[91m$(NM)/$(OTOOL)\033[0m\033[1;32m:\033[0m%-6s\033[32m[✔]\033[0m\n"

libft:
	@$(MAKE) fast -C $(LIBFTDIR)

noflags: FLAGS := 
noflags: re

re: fclean all

.PHONY: all clean fast fclean ft_nm ft_otool libft noflags re
