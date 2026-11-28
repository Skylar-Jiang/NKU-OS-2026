# Host-side RR/CFS regression tests. Compiles the real production scheduler
# sources unchanged and reuses member A's reporter (tests/fifo_sjf/host_io.c).
HOSTCC ?= gcc
ROOT := ../../..
BUILD := build
COMMON := -std=gnu99 -O1 -g -Wall -Wextra -Werror \
          -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
KERNEL := -fno-builtin -D__riscv_xlen=64 \
          -I$(ROOT)/libs -I$(ROOT)/kern/process -I$(ROOT)/kern/schedule \
          -I$(ROOT)/kern/trap -I$(ROOT)/kern/mm -I$(ROOT)/kern/debug
OBJECTS := $(BUILD)/test_main.o $(BUILD)/default_sched.o $(BUILD)/cfs_sched.o \
           $(BUILD)/host_io.o $(BUILD)/host_guard.o

.PHONY: all test clean
all: $(BUILD)/rr_cfs_test

$(BUILD):
	mkdir -p $@

$(BUILD)/test_main.o: test_main.c host_guard.h | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

$(BUILD)/default_sched.o: $(ROOT)/kern/schedule/default_sched.c | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

$(BUILD)/cfs_sched.o: $(ROOT)/kern/schedule/cfs_sched.c | $(BUILD)
	$(HOSTCC) $(COMMON) $(KERNEL) -MMD -MP -c $< -o $@

# libc-only translation units, kept apart from the kernel typedefs.
$(BUILD)/host_io.o: $(ROOT)/tests/fifo_sjf/host_io.c | $(BUILD)
	$(HOSTCC) $(COMMON) -MMD -MP -c $< -o $@

$(BUILD)/host_guard.o: host_guard.c host_guard.h | $(BUILD)
	$(HOSTCC) $(COMMON) -MMD -MP -c $< -o $@

$(BUILD)/rr_cfs_test: $(OBJECTS)
	$(HOSTCC) $(COMMON) -no-pie $^ -o $@

test: all
	./$(BUILD)/rr_cfs_test

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
