# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= 

# --------------------------------------------------------------------
# Vendored giflib (static build, no system dependencies)
# --------------------------------------------------------------------

VENDOR_DIR := vendor/giflib
VENDOR_SRCS := $(wildcard $(VENDOR_DIR)/*.c)

# --------------------------------------------------------------------
# FLAGS
# --------------------------------------------------------------------

# FLAGS are shared between C and C++
# Ensure vendor include is present in the main FLAGS so the Rack SDK's compile step
# will get the vendor include directory.
FLAGS += -I$(RACK_DIR) -I$(VENDOR_DIR) -g -Wall -Wextra -fopenmp

# C flags (giflib is C99)
CFLAGS += -std=c99

# C++ flags
CXXFLAGS += -I$(RACK_DIR)/dep/include -I$(VENDOR_DIR) -std=c++17

# Include vendored headers
CPPFLAGS += -I$(VENDOR_DIR)

# Link flags (NO system giflib)
LDFLAGS += -fopenmp

# --------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------

# Plugin C++ sources
SOURCES += $(wildcard src/*.cpp)

# Vendored giflib sources
SOURCES += $(VENDOR_SRCS)

# --------------------------------------------------------------------
# Distribution
# --------------------------------------------------------------------

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)
DISTRIBUTABLES += $(VENDOR_DIR)/COPYING

# --------------------------------------------------------------------
# Rack build system
# --------------------------------------------------------------------

include $(RACK_DIR)/plugin.mk
