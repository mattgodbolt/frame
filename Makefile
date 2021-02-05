.DEFAULT_GOAL := help
export PICO_SDK_PATH=$(CURDIR)/../pico-sdk
CMAKE:=cmake
NINJA:=ninja
OUTPUT_DIR:=cmake-build-deploy
OUTPUT_UF2:=test.uf2
RPI_DIR:=/media/$(USER)/RPI-RP2

.PHONY: help
help:
	@grep -E '^[0-9a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":"}; {split($$3,a,"\#\#"); printf "\033[36m%-30s\033[0m %s (%s)\n", $$2, a[2], $$1}'

$(OUTPUT_DIR)/CMakeCache.txt:
	$(CMAKE) -B $(OUTPUT_DIR) -S . -DCMAKE_BUILD_TYPE=Release -GNinja

.PHONY: build
build: $(OUTPUT_DIR)/CMakeCache.txt  ## Build the project
	$(NINJA) -C $(OUTPUT_DIR)

.PHONY: await-pico
await-pico:  ## wait for the pico to be ready for deploy (BOOTSEL)
	@echo -n "Waiting for Raspberry Pi to mount...";
	@while [ ! -d $(RPI_DIR) ]; do sleep 0.25; done
	@echo "done";

deploy: build | await-pico
	cp $(OUTPUT_DIR)/$(OUTPUT_UF2) $(RPI_DIR)