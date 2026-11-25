HOSTCC ?= gcc
ROOT := ../..
BUILD := build
COMMON := -std=gnu99 -O1 -g -Wall -Wextra -Werror \
          -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
KERNEL := -fno-builtin -D__riscv_xlen=64 \
          -I$(ROOT)/libs -I$(ROOT)/kern/process -I$(ROOT)/kern/schedule \
          -I$(ROOT)/kern/trap -I$(ROOT)/kern/mm -I$(ROOT)/kern/debug
OBJECTS := $(BUILD)/test_main.o $(BUILD)/fifo_sched.o \
           $(BUILD)/sjf_sched.o $(BUILD)/host_io.o

.PHONY: all test
all: $(BUILD)/fifo_sjf_test

$(BUILD):
	mkdir -p $@

$(BUILD)/test_main.o: test_main.c host_io.h | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

# Compile the real production files unchanged, not copied/extracted algorithms.
$(BUILD)/fifo_sched.o: $(ROOT)/kern/schedule/fifo_sched.c | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

$(BUILD)/sjf_sched.o: $(ROOT)/kern/schedule/sjf_sched.c | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

# libc lives in this translation unit only, isolated from kernel typedefs.
$(BUILD)/host_io.o: host_io.c host_io.h | $(BUILD)
	$(HOSTCC) $(COMMON) -MMD -MP -c $< -o $@

$(BUILD)/fifo_sjf_test: $(OBJECTS)
	$(HOSTCC) $(COMMON) -no-pie $^ -o $@

test: all
	./$(BUILD)/fifo_sjf_test

-include $(wildcard $(BUILD)/*.d)
