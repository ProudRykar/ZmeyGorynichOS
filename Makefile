NASM = nasm
CC = gcc
LD = ld
OBJCOPY = objcopy
QEMU = qemu-system-x86_64

BOOT_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

MBR_BIN = $(BUILD_DIR)/mbr.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin

ENTRY_OBJ = $(BUILD_DIR)/entry.o
MAIN_OBJ = $(BUILD_DIR)/main.o
TERMINAL_OBJ = $(BUILD_DIR)/terminal.o
SERIAL_OBJ = $(BUILD_DIR)/serial.o
BOOT_INFO_PRINT_OBJ = $(BUILD_DIR)/boot_info_print.o

ISR_OBJ = $(BUILD_DIR)/interrupt_stubs.o
IDT_OBJ = $(BUILD_DIR)/idt.o
PIC_OBJ = $(BUILD_DIR)/pic.o
PIT_OBJ = $(BUILD_DIR)/pit.o
KEYBOARD_OBJ = $(BUILD_DIR)/keyboard.o
SHELL_OBJ = $(BUILD_DIR)/shell.o

GDT_OBJ = $(BUILD_DIR)/gdt.o
GDT_LOAD_OBJ = $(BUILD_DIR)/gdt_load.o

VMM_OBJ = $(BUILD_DIR)/vmm.o

VGA_FONT_OBJ = $(BUILD_DIR)/vga_font.o
VGA_FONT_ASM_OBJ = $(BUILD_DIR)/vga_font_asm.o

KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

PMM_OBJ = $(BUILD_DIR)/pmm.o

HEAP_OBJ = $(BUILD_DIR)/heap.o

ADDRESS_SPACE_OBJ = $(BUILD_DIR)/address_space.o

USER_MEMORY_OBJ = $(BUILD_DIR)/user_memory.o

USER_MODE_OBJ = $(BUILD_DIR)/user_mode.o

SYSCALL_OBJ = $(BUILD_DIR)/syscall.o

PROCESS_MANAGER_OBJ = $(BUILD_DIR)/process_manager.o

DEBUG_TESTS_OBJ = $(BUILD_DIR)/debug_tests.o

IMG = $(BUILD_DIR)/tinyos.img

CFLAGS = \
	-m64 \
	-std=gnu11 \
	-O2 \
	-Wall \
	-Wextra \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-fno-builtin \
	-mno-red-zone \
	-mcmodel=small \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-finput-charset=UTF-8 \
	-fexec-charset=CP866

LDFLAGS = \
	-m elf_x86_64 \
	-T linker.ld \
	--build-id=none

.PHONY: all run debug clean check

all: $(IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ================================
# BOOTLOADER
# ================================

$(MBR_BIN): $(BOOT_DIR)/mbr.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(STAGE2_BIN): $(BOOT_DIR)/stage2.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

# ================================
# KERNEL
# ================================

$(ENTRY_OBJ): $(KERNEL_DIR)/entry.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(MAIN_OBJ): $(KERNEL_DIR)/main.c $(KERNEL_DIR)/boot_info.h $(KERNEL_DIR)/boot_info_print.h $(KERNEL_DIR)/terminal.h $(KERNEL_DIR)/shell.h $(KERNEL_DIR)/idt.h $(KERNEL_DIR)/pic.h $(KERNEL_DIR)/pit.h $(KERNEL_DIR)/keyboard.h $(KERNEL_DIR)/serial.h $(KERNEL_DIR)/gdt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(TERMINAL_OBJ): $(KERNEL_DIR)/terminal.c $(KERNEL_DIR)/terminal.h $(KERNEL_DIR)/serial.h $(KERNEL_DIR)/vga_font.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(SERIAL_OBJ): $(KERNEL_DIR)/serial.c $(KERNEL_DIR)/serial.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(BOOT_INFO_PRINT_OBJ): $(KERNEL_DIR)/boot_info_print.c $(KERNEL_DIR)/boot_info_print.h $(KERNEL_DIR)/boot_info.h $(KERNEL_DIR)/terminal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(ISR_OBJ): $(KERNEL_DIR)/interrupt_stubs.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(IDT_OBJ): $(KERNEL_DIR)/idt.c $(KERNEL_DIR)/idt.h $(KERNEL_DIR)/terminal.h $(KERNEL_DIR)/syscall.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(PIC_OBJ): $(KERNEL_DIR)/pic.c $(KERNEL_DIR)/pic.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(PIT_OBJ): $(KERNEL_DIR)/pit.c $(KERNEL_DIR)/pit.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(KEYBOARD_OBJ): $(KERNEL_DIR)/keyboard.c $(KERNEL_DIR)/keyboard.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(SHELL_OBJ): $(KERNEL_DIR)/shell.c $(KERNEL_DIR)/shell.h $(KERNEL_DIR)/keyboard.h $(KERNEL_DIR)/terminal.h $(KERNEL_DIR)/pit.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(GDT_OBJ): $(KERNEL_DIR)/gdt.c $(KERNEL_DIR)/gdt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(GDT_LOAD_OBJ): $(KERNEL_DIR)/gdt_load.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(VGA_FONT_OBJ): $(KERNEL_DIR)/vga_font.c $(KERNEL_DIR)/vga_font.h $(KERNEL_DIR)/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(KERNEL_DIR) -c $< -o $@

$(VGA_FONT_ASM_OBJ): $(KERNEL_DIR)/vga_font.asm assets/cp866-8x16.bin | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(PMM_OBJ): kernel/memory/pmm.c kernel/memory/pmm.h
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(VMM_OBJ): kernel/memory/vmm.c kernel/memory/vmm.h kernel/memory/pmm.h
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(HEAP_OBJ): $(KERNEL_DIR)/memory/heap.c
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(ADDRESS_SPACE_OBJ): $(KERNEL_DIR)/memory/address_space.c
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(USER_MEMORY_OBJ): $(KERNEL_DIR)/memory/user_memory.c $(KERNEL_DIR)/memory/user_memory.h
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(USER_MODE_OBJ): $(KERNEL_DIR)/user_mode.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(SYSCALL_OBJ): $(KERNEL_DIR)/syscall.c $(KERNEL_DIR)/syscall.h $(KERNEL_DIR)/terminal.h $(KERNEL_DIR)/memory/vmm.h $(KERNEL_DIR)/user_mode.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(PROCESS_MANAGER_OBJ): $(KERNEL_DIR)/process/process_manager.c $(KERNEL_DIR)/process/process_manager.h $(KERNEL_DIR)/process/process.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(DEBUG_TESTS_OBJ): kernel/debug/tests.c kernel/debug/tests.h
	$(CC) $(CFLAGS) -Ikernel -c kernel/debug/tests.c -o $(DEBUG_TESTS_OBJ)

$(KERNEL_ELF): $(ENTRY_OBJ) $(ISR_OBJ) $(IDT_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(KEYBOARD_OBJ) $(SHELL_OBJ) $(MAIN_OBJ) $(TERMINAL_OBJ) $(SERIAL_OBJ) $(BOOT_INFO_PRINT_OBJ) $(VGA_FONT_OBJ) $(VGA_FONT_ASM_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ) $(ADDRESS_SPACE_OBJ) $(USER_MEMORY_OBJ) $(GDT_OBJ) $(GDT_LOAD_OBJ) $(USER_MODE_OBJ) $(SYSCALL_OBJ) $(PROCESS_MANAGER_OBJ) $(DEBUG_TESTS_OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ENTRY_OBJ) $(ISR_OBJ) $(IDT_OBJ) $(PIC_OBJ) $(PIT_OBJ) $(KEYBOARD_OBJ) $(SHELL_OBJ) $(MAIN_OBJ) $(TERMINAL_OBJ) $(SERIAL_OBJ) $(BOOT_INFO_PRINT_OBJ) $(VGA_FONT_OBJ) $(VGA_FONT_ASM_OBJ) $(PMM_OBJ) $(VMM_OBJ) $(HEAP_OBJ) $(ADDRESS_SPACE_OBJ) $(USER_MEMORY_OBJ) $(GDT_OBJ) $(GDT_LOAD_OBJ) $(USER_MODE_OBJ) $(SYSCALL_OBJ) $(PROCESS_MANAGER_OBJ) $(DEBUG_TESTS_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# ================================
# DISK IMAGE
# ================================

$(IMG): $(MBR_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	@echo "Checking bootloader size..."
	@test $$(stat -c%s $(MBR_BIN)) -eq 512 || \
		(echo "ERROR: MBR must be exactly 512 bytes"; exit 1)

	@test $$(stat -c%s $(STAGE2_BIN)) -le 16384 || \
		(echo "ERROR: Stage 2 exceeds 16 KiB"; exit 1)

	@echo "Checking kernel size..."
	@test $$(stat -c%s $(KERNEL_BIN)) -le 32768 || \
		(echo "ERROR: Kernel exceeds 32 KiB"; exit 1)

	@echo "Creating disk image..."
	rm -f $(IMG)
	truncate -s $$((512 * 2880)) $(IMG)

	@echo "Writing MBR..."
	dd if=$(MBR_BIN) of=$(IMG) bs=512 seek=0 conv=notrunc status=none

	@echo "Writing Stage 2..."
	dd if=$(STAGE2_BIN) of=$(IMG) bs=512 seek=1 conv=notrunc status=none

	@echo "Writing kernel..."
	dd if=$(KERNEL_BIN) of=$(IMG) bs=512 seek=33 conv=notrunc status=none

	@echo "Disk image successfully created: $(IMG)"

# ================================
# QEMU
# ================================

run: $(IMG)
	$(QEMU) \
		-drive file=$(IMG),format=raw,if=ide \
		-boot c \
		-m 64M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		2>&1 | tee $(BUILD_DIR)/qemu.log

debug: $(IMG)
	$(QEMU) \
		-drive file=$(IMG),format=raw,if=ide \
		-boot c \
		-m 64M \
		-serial stdio \
		-s -S \
		-no-reboot \
		-no-shutdown

# ================================
# CLEAN
# ================================

clean:
	rm -rf $(BUILD_DIR)