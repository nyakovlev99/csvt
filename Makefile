OUTPUT_DIR := obj

# SCRIPTS := verify_matmul
SCRIPTS := atlas-stress

LIBS := \
	vfio_utils \
	pci_utils \
	dma

LIB_PATHS := $(patsubst %,src/%.c,$(LIBS))

CFLAGS := -Wall -Werror -O3
CFLAGS := -march=native
LDFLAGS := -lpci

.PHONY: build
build: $(addprefix $(OUTPUT_DIR)/,$(SCRIPTS))

$(OUTPUT_DIR):
	mkdir -p $@

$(OUTPUT_DIR)/verify_matmul: src/verify_matmul.c $(LIB_PATHS) | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OUTPUT_DIR)/atlas-stress: src/atlas_stress.c $(LIB_PATHS) | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
