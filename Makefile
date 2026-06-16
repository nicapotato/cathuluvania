# Cathuluvania - Cross-platform Makefile
# CI workflows use workflow_dispatch; run `gh` from this directory.
# raylib 6.0 is vendored under external/raylib-master (gitignored).

.SUFFIXES:

.DEFAULT_GOAL := all

.PHONY: all clean run help app-bundle run-app assets vendor-raylib itch-macos itch-windows package-windows smoke run-smoke smoke-test-bundle cathuluvania-ci cathuluvania-ci-watch cathuluvania-release cathuluvania-release-watch

CATHULUVANIA_WORKFLOW := .github/workflows/cathuluvania-cicd.yml
# Default platform: all | web | macos | windows
PLATFORM ?= all
# git ref to run the workflow on (gh uses the default branch if omitted). Default = current branch.
# Your branch must be pushed to origin. Override: make cathuluvania-ci REF=main
REF ?= $(shell git branch --show-current 2>/dev/null)

APP_NAME = Cathuluvania
SMOKE_BIN = Cathuluvania_smoke
PRODUCT_NAME = Cathuluvania
BUNDLE_ID = io.itch.nicapotato.cathuluvania
COPYRIGHT = (c) Cathuluvania
PRODUCT_VERSION := $(shell grep '^VERSION=' project.conf 2>/dev/null | cut -d= -f2)
ifeq ($(PRODUCT_VERSION),)
  PRODUCT_VERSION := 0.0.0
endif

APP_BUNDLE = $(PRODUCT_NAME).app
CONTENTS_DIR = $(APP_BUNDLE)/Contents
MACOS_DIR = $(CONTENTS_DIR)/MacOS
RESOURCES_DIR = $(CONTENTS_DIR)/Resources
SRC_DIR = src
BIN_DIR = bin
BUILD_CONFIG = Debug

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
UNAME_M := $(shell uname -m 2>/dev/null || echo x86_64)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
  IS_WINDOWS = 1
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
  IS_WINDOWS = 1
else ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
  IS_WINDOWS = 1
endif

ifeq ($(IS_WINDOWS),1)
  EXE_EXT = .exe
  RAYLIB_MAKE ?= mingw32-make
else
  EXE_EXT =
  RAYLIB_MAKE ?= make
endif

ifeq ($(UNAME_S),Darwin)
  ifeq ($(UNAME_M),arm64)
    config ?= debug_arm64
  else
    config ?= debug_x64
  endif
else ifeq ($(UNAME_S),Linux)
  config ?= debug_x64
else
  config ?= debug_x64
endif

ifeq ($(findstring debug,$(config)),debug)
  BUILD_CONFIG = Debug
  CFLAGS_EXTRA = -g -O0
else
  BUILD_CONFIG = Release
  CFLAGS_EXTRA = -O2 -DNDEBUG
endif

EXECUTABLE = $(BIN_DIR)/$(BUILD_CONFIG)/$(APP_NAME)$(EXE_EXT)
SMOKE_EXECUTABLE = $(BIN_DIR)/$(BUILD_CONFIG)/$(SMOKE_BIN)$(EXE_EXT)
SMOKE_SRCS = $(SRC_DIR)/smoke_main.c $(SRC_DIR)/platform_path.c
WIN_RELEASE_DIR = release
WIN_EXE = $(WIN_RELEASE_DIR)/$(APP_NAME).exe
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/game.c $(SRC_DIR)/level.c $(SRC_DIR)/tile_config.c $(SRC_DIR)/platform_path.c

ASEPRITE ?= $(shell if [ -x /Applications/Aseprite.app/Contents/MacOS/aseprite ]; then echo /Applications/Aseprite.app/Contents/MacOS/aseprite; else echo aseprite; fi)
ACTS = resources/visual/green-act.aseprite resources/visual/dark-act.aseprite
EXPORT_SCRIPT = scripts/aesprite/export-act-level.lua
REGISTRY_SCRIPT = scripts/aesprite/gen-acts-registry.lua
ACTS_GEN = $(SRC_DIR)/acts.gen.h

RAYLIB_VERSION_REQUIRED = 6.0
RAYLIB_ROOT = external/raylib-master
RAYLIB_SRC = $(RAYLIB_ROOT)/src
RAYLIB_LIB = $(RAYLIB_SRC)/libraylib.a

CFLAGS += -I$(RAYLIB_SRC) -I$(RAYLIB_SRC)/external -I$(RAYLIB_SRC)/external/glfw/include -DPLATFORM_DESKTOP
LDFLAGS += -L$(RAYLIB_SRC) -lraylib
ifeq ($(UNAME_S),Darwin)
  LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else ifeq ($(IS_WINDOWS),1)
  LDFLAGS += -lgdi32 -lwinmm
  ifeq ($(WINDOWS_GUI),1)
    CFLAGS += -mwindows
  endif
  ifneq ($(findstring release,$(config)),)
    LDFLAGS += -static
  endif
else ifeq ($(UNAME_S),Linux)
  LDFLAGS += -lGL -lm -lpthread -ldl -lrt
endif
BUILD_MODE = raylib-$(RAYLIB_VERSION_REQUIRED)

vendor-raylib:
	@chmod +x scripts/vendor_raylib.sh
	@./scripts/vendor_raylib.sh

$(RAYLIB_LIB): vendor-raylib
	@rm -f $(RAYLIB_SRC)/libraylib.a $(RAYLIB_SRC)/libraylib.*.dylib $(RAYLIB_SRC)/libraylib.dylib
	@$(RAYLIB_MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC -j4

CFLAGS += -std=c99 -Wall -I$(SRC_DIR) -Iinclude $(CFLAGS_EXTRA)

# Aepsprite CLI Docs: https://www.aseprite.org/docs/cli/
assets: $(ACTS_GEN)

$(ACTS_GEN): $(ACTS) $(EXPORT_SCRIPT) $(REGISTRY_SCRIPT) scripts/aesprite/lib/json.lua scripts/aesprite/lib/slice_data.lua $(SRC_DIR)/acts_metadata.h
	@set -e; \
	for act in $(ACTS); do \
	  echo "Exporting $$act..."; \
	  $(ASEPRITE) -b "$$act" -script $(EXPORT_SCRIPT) || { \
	    echo ""; \
	    echo "ERROR: Aseprite export failed for $$act."; \
	    echo "Open the .aseprite file in Aseprite, fix slice/layer issues listed above, then run: make assets"; \
	    exit 1; \
	  }; \
	done; \
	echo "Generating act registry..."; \
	$(ASEPRITE) -b --script $(REGISTRY_SCRIPT) || { \
	  echo ""; \
	  echo "ERROR: Act registry generation failed."; \
	  echo "Fix export validation errors first, or check src/acts_metadata.h matches exported acts."; \
	  exit 1; \
	}

all: $(EXECUTABLE)
	@echo "Build complete: $(EXECUTABLE) ($(BUILD_MODE))"

$(EXECUTABLE): $(SRCS) $(ACTS_GEN) $(RAYLIB_LIB)
	@mkdir -p $(dir $(EXECUTABLE))
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

$(SMOKE_EXECUTABLE): $(SMOKE_SRCS) $(ACTS_GEN)
	@mkdir -p $(dir $(SMOKE_EXECUTABLE))
	$(CC) $(CFLAGS) -o $@ $(SMOKE_SRCS)

smoke: $(SMOKE_EXECUTABLE)
	@echo "Smoke binary ready: $(SMOKE_EXECUTABLE)"

# Headless CI smoke test: validate act PNGs resolve from cwd (no GLFW/OpenGL).
run-smoke: smoke
	@echo "==== Running $(SMOKE_BIN) ===="
	@if [ ! -f "$(SMOKE_EXECUTABLE)" ]; then \
		echo "Error: Smoke binary not found at $(SMOKE_EXECUTABLE)"; \
		exit 1; \
	fi
	@mkdir -p "$(dir $(SMOKE_EXECUTABLE))resources"
	@rsync -a resources/ "$(dir $(SMOKE_EXECUTABLE))resources/"
	@cd "$(dir $(SMOKE_EXECUTABLE))" && ./$(SMOKE_BIN)$(EXE_EXT)

# Validate packaged .app: bundle layout, static link, and resource paths from MacOS/.
smoke-test-bundle:
	@echo "==== Smoke test: $(APP_BUNDLE) ===="
	@test -d "$(APP_BUNDLE)/Contents/MacOS"
	@test -f "$(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)"
	@test -d "$(APP_BUNDLE)/Contents/Resources/resources"
	@test -d "$(APP_BUNDLE)/Contents/MacOS/resources"
	@file "$(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)"
	@if otool -L "$(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)" 2>/dev/null | grep -qE 'libraylib|/opt/homebrew|/usr/local/lib|@rpath/libraylib'; then \
		echo "Error: bundled binary is not statically linked"; \
		exit 1; \
	fi
	@cp "$(SMOKE_EXECUTABLE)" "$(MACOS_DIR)/$(SMOKE_BIN)"
	@"$(MACOS_DIR)/$(SMOKE_BIN)"
	@rm -f "$(MACOS_DIR)/$(SMOKE_BIN)"
	@echo "==== Bundle smoke test passed ===="

run: $(EXECUTABLE)
	@echo "Running $(APP_NAME)..."
	@./$(EXECUTABLE)

clean:
	rm -rf $(BIN_DIR)
	rm -rf "$(APP_BUNDLE)"
	rm -rf "$(WIN_RELEASE_DIR)"

help:
	@echo "Cathuluvania - raylib platformer"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build (default)"
	@echo "  assets     - Export acts from Aseprite + regenerate acts.gen.h"
	@echo "  run        - Build and run"
	@echo "  clean      - Remove build artifacts"
	@echo "  app-bundle           - macOS .app bundle (local + itch.io / S3)"
	@echo "  run-app              - Build .app and open in Finder"
	@echo "  itch-macos           - Release build + .app for itch.io / S3 (vendored raylib 6.0)"
	@echo "  itch-windows   - Release build + release/ folder for itch.io / S3 (MinGW)"
	@echo "  package-windows - Stage release/ from an existing Release build (Windows)"
	@echo "  smoke        - Build headless smoke binary (CI resource validation)"
	@echo "  run-smoke    - Run headless smoke test (no display required)"
	@echo "  smoke-test-bundle - Validate .app layout + resources after packaging"
	@echo "  vendor-raylib  - Clone raylib $(RAYLIB_VERSION_REQUIRED) into external/raylib-master"
	@echo "  cathuluvania-ci            - Dispatch CI workflow (PLATFORM, VERSION, CHANNEL, REF)"
	@echo "  cathuluvania-ci-watch      - Dispatch CI workflow and watch the run"
	@echo "  cathuluvania-release       - Full release: all platforms, itch.io + S3 + GitHub Release"
	@echo "  cathuluvania-release-watch - Dispatch release and watch the run"
	@echo ""
	@echo "Raylib: vendored at external/raylib-master ($(RAYLIB_VERSION_REQUIRED))"
	@echo "CI: PLATFORM=all|web|macos|windows  REF=branch  VERSION=  CHANNEL="

# Release + static raylib + .app bundle (matches CI).
itch-macos:
	@$(MAKE) --no-print-directory config=release_arm64 all smoke app-bundle
	@echo "==== itch.io macOS bundle ready: $(APP_BUNDLE) ===="

run-app: app-bundle
	@open "$(APP_BUNDLE)"

# Create .app bundle (for itch.io, local run). Does not depend on `all` so CI can build then bundle.
app-bundle:
	@echo "==== Creating macOS .app bundle ===="
	@if [ "$(UNAME_S)" != "Darwin" ]; then \
		echo "Error: app-bundle is macOS only"; \
		exit 1; \
	fi
	@if [ ! -f "$(EXECUTABLE)" ]; then \
		echo "Error: Executable not found at $(EXECUTABLE)"; \
		echo "Build first: make config=$(config) all"; \
		exit 1; \
	fi
	@if [ "$(BUILD_CONFIG)" = "Release" ] && otool -L "$(EXECUTABLE)" 2>/dev/null | grep -qE 'libraylib|/opt/homebrew|/usr/local/lib|@rpath/libraylib'; then \
		echo "Error: $(EXECUTABLE) is not statically linked (found external libraylib)."; \
		echo "Run: make clean && make itch-macos"; \
		exit 1; \
	fi
	@echo "Building app bundle from $(BUILD_CONFIG) configuration..."
	@rm -rf "$(APP_BUNDLE)"
	@mkdir -p "$(MACOS_DIR)" "$(RESOURCES_DIR)"
	@cp "$(EXECUTABLE)" "$(MACOS_DIR)/$(APP_NAME)"
	@chmod +x "$(MACOS_DIR)/$(APP_NAME)"
	@if [ -d "resources" ]; then \
		rsync -a --delete resources/ "$(RESOURCES_DIR)/resources/"; \
		rsync -a --delete resources/ "$(MACOS_DIR)/resources/"; \
	fi
	@if [ -d "$(dir $(EXECUTABLE))resources" ]; then \
		rsync -a "$(dir $(EXECUTABLE))resources/" "$(RESOURCES_DIR)/resources/" 2>/dev/null || true; \
		rsync -a "$(dir $(EXECUTABLE))resources/" "$(MACOS_DIR)/resources/" 2>/dev/null || true; \
	fi
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > "$(CONTENTS_DIR)/Info.plist"
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '<plist version="1.0">' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '<dict>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleExecutable</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(APP_NAME)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleIdentifier</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(BUNDLE_ID)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleName</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(PRODUCT_NAME)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleVersion</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(PRODUCT_VERSION)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleShortVersionString</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(PRODUCT_VERSION)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundlePackageType</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>APPL</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>CFBundleInfoDictionaryVersion</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>6.0</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>LSMinimumSystemVersion</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>10.9</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<key>NSHumanReadableCopyright</key>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '	<string>$(COPYRIGHT)</string>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '</dict>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo '</plist>' >> "$(CONTENTS_DIR)/Info.plist"
	@echo "Signing app bundle..."
	@if security find-identity -v -p codesigning 2>/dev/null | grep -q "Developer ID Application"; then \
		SIGN_IDENTITY=$$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)".*/\1/'); \
		echo "Using Developer ID certificate: $$SIGN_IDENTITY"; \
		codesign --force --deep --sign "$$SIGN_IDENTITY" --options runtime --timestamp "$(APP_BUNDLE)" 2>/dev/null || \
		codesign --force --deep --sign "$$SIGN_IDENTITY" --options runtime "$(APP_BUNDLE)"; \
	else \
		echo "No Developer ID certificate found, using ad-hoc signing"; \
		codesign --force --deep --sign - --options runtime "$(APP_BUNDLE)"; \
	fi
	@echo "Verifying app bundle signature..."
	@if codesign --verify --deep --strict --verbose=2 "$(APP_BUNDLE)" 2>&1; then \
		echo "Signature verification passed"; \
	else \
		echo "Warning: Signature verification failed (common for ad-hoc signing)"; \
	fi
	@echo "==== App bundle complete: $(APP_BUNDLE) ===="
itch-windows:
	@$(MAKE) --no-print-directory WINDOWS_GUI=1 config=release_x64 CC=gcc package-windows
	@echo "==== itch.io Windows bundle ready: $(WIN_RELEASE_DIR)/ ===="

# Runs inside nested make (config=release_x64) so $(EXECUTABLE) is bin/Release/*.exe.
package-windows: all
	@rm -rf "$(WIN_RELEASE_DIR)"
	@mkdir -p "$(WIN_RELEASE_DIR)"
	@cp "$(EXECUTABLE)" "$(WIN_EXE)"
	@cp -R resources "$(WIN_RELEASE_DIR)/"

# Dispatch the Cathuluvania workflow. Examples:
#   make cathuluvania-ci
#   make cathuluvania-ci PLATFORM=macos
#   make cathuluvania-ci PLATFORM=windows VERSION=1.2.3
#   make cathuluvania-ci PLATFORM=web CHANNEL=web
# Requires: gh (https://cli.github.com/), authenticated (`gh auth login`).
cathuluvania-ci:
	gh workflow run "$(CATHULUVANIA_WORKFLOW)" \
		$(if $(REF),-r "$(REF)",) \
		-f build_platform="$(PLATFORM)" \
		$(if $(VERSION),-f version="$(VERSION)",) \
		$(if $(CHANNEL),-f channel="$(CHANNEL)",)

# Full release: build all platforms, publish itch.io + S3 + GitHub Release (tag from project.conf).
# Examples:
#   make cathuluvania-release
#   make cathuluvania-release REF=main
cathuluvania-release:
	gh workflow run "$(CATHULUVANIA_WORKFLOW)" \
		$(if $(REF),-r "$(REF)",) \
		-f build_platform=all \
		-f publish_gh_release=true \
		$(if $(VERSION),-f version="$(VERSION)",) \
		$(if $(CHANNEL),-f channel="$(CHANNEL)",)

cathuluvania-ci-watch: cathuluvania-ci
	@sleep 2
	@RID=$$(gh run list --workflow="$(CATHULUVANIA_WORKFLOW)" -L 1 --json databaseId -q '.[0].databaseId'); \
		test -n "$$RID"; \
		gh run watch "$$RID"

cathuluvania-release-watch: cathuluvania-release
	@sleep 2
	@RID=$$(gh run list --workflow="$(CATHULUVANIA_WORKFLOW)" -L 1 --json databaseId -q '.[0].databaseId'); \
		test -n "$$RID"; \
		gh run watch "$$RID"
