OUTPUT_DIR := obj

SCRIPTS := verify_matmul

CFLAGS := -Wall -Werror -O3
LDFLAGS := 

.PHONY: build
build: $(addprefix $(OUTPUT_DIR)/,$(SCRIPTS))

$(OUTPUT_DIR):
	mkdir -p $@

$(OUTPUT_DIR)/verify_matmul: src/verify_matmul.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
