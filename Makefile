# --------------------------------------------------------
# Basic Variables
# --------------------------------------------------------
EXECUTABLE := lz_codec
SRCDIR     := src
INCDIR     := include
BUILDDIR   := build
INCLUDES   := -I$(INCDIR) -Ithird_party/argparse -Ithird_party/libdivsufsort

# --------------------------------------------------------
# Collect Sources, Objects, Dependencies
# --------------------------------------------------------
CPP_SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS     := $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/%.o, $(CPP_SOURCES))
# Instead of each .cpp => .d, we map each .o => .d
DEPENDENCIES := $(OBJECTS:.o=.d)

# --------------------------------------------------------
# Compiler & Flags
# --------------------------------------------------------
CXX := g++

DEBUG_FLAGS   := -g -O0
#-fsanitize=address -fsanitize=leak
RELEASE_FLAGS := -O3 -Werror

CXXFLAGS := -std=c++17 -fms-extensions -Wall -Wextra -pedantic

LDFLAGS :=
LDLIBS   := -lm

ifdef RELEASE
	CXXFLAGS += $(RELEASE_FLAGS)
else
	CXXFLAGS += $(DEBUG_FLAGS)
endif

#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t1.txt -o tmp/tests/out/t1.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t2.txt -o tmp/tests/out/t2.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t3.txt -o tmp/tests/out/t3.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t4.txt -o tmp/tests/out/t4.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t5.txt -o tmp/tests/out/t5.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t6-a.txt -o tmp/tests/out/t6-a.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t6-b.txt -o tmp/tests/out/t6-b.txt -w 512 -c
ARGUMENTS_COMPRESSOR := -i tmp/tests/in/t7.txt -o tmp/tests/out/t7.txt -w 512 -c
#ARGUMENTS_COMPRESSOR := -i tmp/tests/in/kko.proj.data/cb.raw -o tmp/tests/out/cb.raw -w 512 -c

#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t1.txt -o tmp/tests/in/t1-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t2.txt -o tmp/tests/in/t2-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t3.txt -o tmp/tests/in/t3-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t4.txt -o tmp/tests/in/t4-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t5.txt -o tmp/tests/in/t5-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t6-a.txt -o tmp/tests/in/t6-a-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t6-b.txt -o tmp/tests/in/t6-b-decompressed.txt -w 512 -d
ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t7.txt -o tmp/tests/in/t7-decompressed.txt -w 512 -d
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/cb.raw -o tmp/tests/in/kko.proj.data/cb.raw-decompressed.txt -w 512 -d


# --------------------------------------------------------
# Targets
# --------------------------------------------------------
.PHONY: all clean run1 pack docker-build docker-run

# Default target builds the executable
all: $(EXECUTABLE)

# Link final executable from object files
$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# Compile each .cpp => .o in build/, generating .d files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -I$(INCLUDES) -MMD -MP -c $< -o $@

# Remove build artifacts
clean:
	rm -rf $(BUILDDIR) *.dSYM *.zip $(EXECUTABLE) compile_commands.json valgrind.log

# Example packaging target (adjust files as needed)
pack:
	zip -r 230614.zip $(SRCDIR)/*.cpp $(INCDIR)/*.hpp Makefile README.md

# Docker targets
docker-build:
	docker compose build

docker-run:
	docker compose run --rm --service-ports kko fish

# Include dependency files if they exist
-include $(DEPENDENCIES)


.PHONY: clang-format
clang-format:
	clang-format -i $(SRCDIR)/*.cpp

.PHONY: clang-tidy
clang-tidy:
	clang-tidy -p . -fix -fix-errors $(SRCDIR)/*.cpp

valgrind: $(EXECUTABLE)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR)

run-compressor: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR)

run-decompressor: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_DECOMPRESSOR)

diff:
	diff tmp/tests/in/t1.txt tmp/tests/out/t1.txt && echo "OK" || echo "FAIL"

check-sizes:
	du -sh zadani/kko.proj.data/info.txt tmp/tests/out/out.txt

bear: ## Create compilation database
	bear $(MAKE) all
