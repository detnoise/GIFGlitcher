# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# Detect giflib with pkg-config (preferred). If pkg-config isn't available on the system
# fall back to common Homebrew include/lib paths for macOS (Apple Silicon and Intel).
GIF_CFLAGS := $(shell pkg-config --cflags giflib 2>/dev/null)
GIF_LIBS   := $(shell pkg-config --libs giflib 2>/dev/null)

ifndef GIF_CFLAGS
GIF_CFLAGS := -I/opt/homebrew/include -I/usr/local/include
endif

ifndef GIF_LIBS
GIF_LIBS := -L/opt/homebrew/lib -L/usr/local/lib -lgif
endif

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -I$(RACK_DIR) -g -Wall -Wextra -fopenmp
CFLAGS +=
CXXFLAGS += -I$(RACK_DIR)/dep/include -std=c++17 $(GIF_CFLAGS)

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS += -fopenmp $(GIF_LIBS)

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
