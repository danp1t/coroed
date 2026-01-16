SRC_DIR   = source
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin

COMPILER           ?= clang
OPTIMIZATION_LEVEL ?= -O3
SANITIZERS         ?=

CC        = $(COMPILER)

CFLAGS    = -g -I $(SRC_DIR)
CFLAGS   += -Wall -Werror
CFLAGS   += -fno-omit-frame-pointer
CFLAGS   += $(OPTIMIZATION_LEVEL)
CFLAGS   += $(SANITIZERS)
CFLAGS   += -Wa,--noexecstack

LDFLAGS   = -Wl,-z,noexecstack 

SRCS     := $(shell find source -iname '*.c') $(shell find source -iname '*.S')

MAIN_EXCLUDE := demo.c test_%.c example_%.c
MAIN_SRCS    := $(filter-out $(addprefix $(SRC_DIR)/%, $(MAIN_EXCLUDE)), $(SRCS))

APP_TARGETS := main_app demo

MAIN_OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(MAIN_SRCS))
MAIN_OBJS     := $(patsubst $(SRC_DIR)/%.S, $(OBJ_DIR)/%.o, $(MAIN_OBJS))

DEMO_OBJS := $(filter-out $(OBJ_DIR)/main.o, $(MAIN_OBJS))
DEMO_OBJS += $(OBJ_DIR)/demo.o

run: $(BIN_DIR)/main_app
	./$(BIN_DIR)/main_app

run-demo: $(BIN_DIR)/demo
	./$(BIN_DIR)/demo

compile: clean all

clean:
	rm -rf $(BUILD_DIR)

$(BIN_DIR)/main_app: $(BIN_DIR) $(OBJ_DIR) $(MAIN_OBJS)
	$(CC) $(MAIN_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/demo: $(BIN_DIR) $(OBJ_DIR) $(DEMO_OBJS)
	$(CC) $(DEMO_OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(BIN_DIR): $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

check-style:
	

check-format:
	find source -iname '*.h' -o -iname '*.c' \
	| xargs clang-format -Werror --dry-run --fallback-style=Google --verbose

fix-format:
	find source -iname '*.h' -o -iname '*.c' \
	| xargs clang-format -i --fallback-style=Google --verbose

info:
	$(info CC        = $(CC))
	$(info CFLAGS    = $(CFLAGS))
	$(info LDFLAGS   = $(LDFLAGS))
	$(info SRC_DIR   = $(SRC_DIR))
	$(info BUILD_DIR = $(BUILD_DIR))
	$(info SRCS      = $(SRCS))
	$(info MAIN_SRCS = $(MAIN_SRCS))

compile_commands.json: $(SRCS)
	@echo '[' > $@
	@for src in $(SRCS); do \
		echo '  {' >> $@; \
		echo '    "directory": "'`pwd`'",' >> $@; \
		echo '    "command": "'$(CC) $(CFLAGS) -c -o $(OBJ_DIR)/$$(echo $$src | sed "s|$(SRC_DIR)/||" | sed "s/\.c$$/.o/" | sed "s/\.S$$/.o/") $$src'",' >> $@; \
		echo '    "file": "'$$src'"' >> $@; \
		echo '  },' >> $@; \
	done
	@sed -i '$$s/,//' $@
	@echo ']' >> $@

bear: compile_commands.json

all: $(addprefix $(BIN_DIR)/, $(APP_TARGETS))

.PHONY: run run-demo compile clean check-style check-format fix-format info bear all