TARGET = proj
CC = gcc
FLAGS = -g -Wall -Wextra -Iinclude

SRCDIR = src
OBJDIR = obj
INCDIR = include

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) $(FLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@$(CC) $(FLAGS) -MMD -c $< -o $@

$(OBJDIR):
	@if not exist $(OBJDIR) mkdir $(OBJDIR)

-include $(DEPS)

clean:
	@if exist $(OBJDIR) rmdir /s /q $(OBJDIR)
	@if exist $(TARGET).exe del /q $(TARGET).exe