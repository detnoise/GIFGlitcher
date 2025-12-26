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
FLAGS += -I$(RACK_DIR) -g -Wall -Wextra -fopenmp

# C flags (giflib is C99)
CFLAGS += -std=c99

# C++ flags
CXXFLAGS += -I$(RACK_DIR)/dep/include -std=c++17

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

