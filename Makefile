NASM = nasm
QEMU = qemu-system-x86_64

BOOT_DIR = boot
BUILD_DIR = build

MBR_BIN = $(BUILD_DIR)/mbr.bin
STAGE2_BIN = $(BUILD_DIR)/stage2.bin
IMG = $(BUILD_DIR)/tinyos.img

all: $(IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(MBR_BIN): $(BOOT_DIR)/mbr.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(STAGE2_BIN): $(BOOT_DIR)/stage2.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(IMG): $(MBR_BIN) $(STAGE2_BIN)
	@echo "Создаём образ диска $(IMG)..."
	# Создаём пустой диск 1.44MB
	truncate -s $$(expr 512 \* 2880) $(IMG)
	# Записываем MBR
	dd if=$(MBR_BIN) of=$(IMG) bs=512 count=1 conv=notrunc status=progress
	# Записываем stage2 после MBR
	dd if=$(STAGE2_BIN) of=$(IMG) bs=512 seek=1 conv=notrunc status=progress

run: $(IMG)
	$(QEMU) -drive file=$(IMG),format=raw -boot a

clean:
	rm -rf $(BUILD_DIR)
