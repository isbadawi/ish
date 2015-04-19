CC = clang
CFLAGS = -Wall -Werror -pedantic -g
OUTPUT_OPTION = -MMD -MP -o $@

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
HDRS = $(wildcard *.h)

ish: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

-include $(DEPS)

tags: $(SRCS) $(HDRS)
	ctags $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS)
