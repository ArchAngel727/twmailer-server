# Makefile

CXX := clang++

TARGET := twmailer-server

SRC_DIR := src
OBJ_DIR := build

CPPFLAGS := -Iheaders

SANITIZER_FLAGS := \
	-fsanitize=address,undefined \
	-fno-omit-frame-pointer

CXXFLAGS := \
	-std=c++23 \
	-g \
	-O0 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wsign-conversion \
	-MMD \
	-MP \
	$(SANITIZER_FLAGS)

LDFLAGS := $(SANITIZER_FLAGS)
LDLIBS :=

# App sources, objects, and dependency files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all build run clean dirs

all: build

# Build the executable
build: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Run with optional arguments:
# make run ARGS="/path file1 file2 -i"
run: build
	./$(TARGET) $(ARGS)8080 data

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Create the object directory
dirs:
	mkdir -p $(OBJ_DIR)

# Automatically include generated header dependencies
-include $(DEPS)

# Remove build files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
