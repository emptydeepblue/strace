NAME=ft_strace
CC=gcc
CFLAGS=-Wall -Wextra -Werror -Iinc -D_GNU_SOURCE
LDFLAGS=-lm
OBJDIR=obj
SOURCES = src/main.c src/tracer.c src/printer.c src/syscall_table.c src/decode.c src/decode_struct.c src/signals.c
OBJECTS=$(SOURCES:src/%.c=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all
	@echo "Rebuild complete."

.PHONY: all clean fclean re