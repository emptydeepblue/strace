NAME=ft_strace
CC=gcc
CFLAGS=-Wall -Wextra -Werror -Iinc
LDFLAGS=-lm
OBJDIR=obj
SOURCES=src/main.c
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