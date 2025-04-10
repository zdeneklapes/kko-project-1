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


#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t1.txt -o tmp/tests/out/t1.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t1.txt -o tmp/tests/in/t1-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t2.txt -o tmp/tests/out/t2.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t2.txt -o tmp/tests/in/t2-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t3.txt -o tmp/tests/out/t3.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t3.txt -o tmp/tests/in/t3-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t4.txt -o tmp/tests/out/t4.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t4.txt -o tmp/tests/in/t4-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t5.txt -o tmp/tests/out/t5.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t5.txt -o tmp/tests/in/t5-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t6-a.txt -o tmp/tests/out/t6-a.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t6-a.txt -o tmp/tests/in/t6-a-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t6-b.txt -o tmp/tests/out/t6-b.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t6-b.txt -o tmp/tests/in/t6-b-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t7.txt -o tmp/tests/out/t7.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t7.txt -o tmp/tests/in/t7-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t8.txt -o tmp/tests/out/t8.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t8.txt -o tmp/tests/in/t8-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t9.txt -o tmp/tests/out/t9.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t9.txt -o tmp/tests/in/t9-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t10.txt -o tmp/tests/out/t10.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t10.txt -o tmp/tests/in/t10-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t11.txt -o tmp/tests/out/t11.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t11.txt -o tmp/tests/in/t11-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t12.txt -o tmp/tests/out/t12.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t12.txt -o tmp/tests/in/t12-decompressed.txt -w 512 -d

ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/t14.txt -o tmp/tests/out/t14.txt -w 512 -c
ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t14.txt -o tmp/tests/in/t14-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/cb.raw -o tmp/tests/out/cb.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/cb.raw -o tmp/tests/in/kko.proj.data/cb.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/cb2.raw -o tmp/tests/out/cb2.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/cb2.raw -o tmp/tests/in/kko.proj.data/cb2.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/df1h.raw -o tmp/tests/out/df1h.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/df1h.raw -o tmp/tests/in/kko.proj.data/df1h.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/df1hvx.raw -o tmp/tests/out/df1hvx.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/df1hvx.raw -o tmp/tests/in/kko.proj.data/df1hvx.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/nk01.raw -o tmp/tests/out/nk01.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/nk01.raw -o tmp/tests/in/kko.proj.data/nk01.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/df1v.raw -o tmp/tests/out/df1v.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/df1v.raw -o tmp/tests/in/kko.proj.data/df1v.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/shp.raw -o tmp/tests/out/shp.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/shp.raw -o tmp/tests/in/kko.proj.data/shp.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/shp1.raw -o tmp/tests/out/shp1.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/shp1.raw -o tmp/tests/in/kko.proj.data/shp1.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tmp/tests/in/kko.proj.data/shp2.raw -o tmp/tests/out/shp2.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/shp2.raw -o tmp/tests/in/kko.proj.data/shp2.raw-decompressed.txt -d

#-------------------------------------------------------------------------------
# Adaptive compressor
#-------------------------------------------------------------------------------
#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tmp/tests/in/t13-a.txt -o tmp/tests/out/t13-a.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t13-a.txt -o tmp/tests/in/t13-a-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tmp/tests/in/t13-b.txt -o tmp/tests/out/t13-b.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/t13-b.txt -o tmp/tests/in/t13-b-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tmp/tests/in/kko.proj.data/nk01.raw -o tmp/tests/out/nk01.raw -w 512 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tmp/tests/out/nk01.raw -o tmp/tests/in/kko.proj.data/nk01.raw-decompressed.txt -d


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

valgrind-compress: $(EXECUTABLE)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR_STATIC)

valgrind-decompress: $(EXECUTABLE)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(EXECUTABLE) $(ARGUMENTS_DECOMPRESSOR)

run-compressor-static: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR_STATIC)

run-compressor-adaptive: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR_ADAPTIVE)

run-decompressor: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_DECOMPRESSOR)

diff:
	diff tmp/tests/in/t1.txt tmp/tests/out/t1.txt && echo "OK" || echo "FAIL"

check-sizes:
	du -sh zadani/kko.proj.data/info.txt tmp/tests/out/out.txt

bear: ## Create compilation database
	bear $(MAKE) all
