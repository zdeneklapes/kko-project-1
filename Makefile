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


#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t1.txt -o tests/out/t1.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t1.txt -o tests/in/static/t1-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t1.txt tests/in/static/t1-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t2.txt -o tests/out/t2.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t2.txt -o tests/in/static/t2-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t2.txt tests/in/static/t2-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t3.txt -o tests/out/t3.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t3.txt -o tests/in/static/t3-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t3.txt tests/in/static/t3-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t4.txt -o tests/out/t4.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t4.txt -o tests/in/static/t4-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t4.txt tests/in/static/t4-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t5.txt -o tests/out/t5.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t5.txt -o tests/in/static/t5-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t5.txt tests/in/static/t5-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t6-a.txt -o tests/out/t6-a.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t6-a.txt -o tests/in/static/t6-a-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t6-a.txt tests/in/static/t6-a-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t6-b.txt -o tests/out/t6-b.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t6-b.txt -o tests/in/static/t6-b-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t6-b.txt tests/in/static/t6-b-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t7.txt -o tests/out/t7.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t7.txt -o tests/in/static/t7-decompressed.txt -d
#DIFF_ARGS=tests/in/static/t7.txt tests/in/static/t7-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t8.txt -o tests/out/t8.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t8.txt -o tests/in/static/t8-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t9.txt -o tests/out/t9.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t9.txt -o tests/in/static/t9-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t10.txt -o tests/out/t10.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t10.txt -o tests/in/static/t10-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t11.txt -o tests/out/t11.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t11.txt -o tests/in/static/t11-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t12.txt -o tests/out/t12.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t12.txt -o tests/in/static/t12-decompressed.txt -w 512 -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/static/t14.txt -o tests/out/t14.txt -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t14.txt -o tests/in/static/t14-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/cb.raw -o tests/out/cb.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/cb.raw -o tests/in/kko.proj.data/cb.raw-decompressed.txt -d
#DIFF_ARGS=tests/in/kko.proj.data/cb.raw tests/in/kko.proj.data/cb.raw-decompressed.txt

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/cb2.raw -o tests/out/cb2.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/cb2.raw -o tests/in/kko.proj.data/cb2.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/df1h.raw -o tests/out/df1h.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/df1h.raw -o tests/in/kko.proj.data/df1h.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/df1hvx.raw -o tests/out/df1hvx.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/df1hvx.raw -o tests/in/kko.proj.data/df1hvx.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/nk01.raw -o tests/out/nk01.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/nk01.raw -o tests/in/kko.proj.data/nk01.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/df1v.raw -o tests/out/df1v.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/df1v.raw -o tests/in/kko.proj.data/df1v.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/shp.raw -o tests/out/shp.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/shp.raw -o tests/in/kko.proj.data/shp.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/shp1.raw -o tests/out/shp1.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/shp1.raw -o tests/in/kko.proj.data/shp1.raw-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_STATIC := -i tests/in/kko.proj.data/shp2.raw -o tests/out/shp2.raw -w 512 -c
#ARGUMENTS_DECOMPRESSOR := -i tests/out/shp2.raw -o tests/in/kko.proj.data/shp2.raw-decompressed.txt -d

#-------------------------------------------------------------------------------
# Adaptive compressor
#-------------------------------------------------------------------------------
#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t13-a.txt -o tests/out/t13-a.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t13-a.txt -o tests/in/adaptive/t13-a-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t13-b.txt -o tests/out/t13-b.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t13-b.txt -o tests/in/adaptive/t13-b-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t13-c.txt -o tests/out/t13-c.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t13-c.txt -o tests/in/adaptive/t13-c-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t15-b.txt -o tests/out/t15-b.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t15-b.txt -o tests/in/adaptive/t15-b-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t17-a.txt -o tests/out/t17-a.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t17-a.txt -o tests/in/adaptive/t17-a-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t18-a.txt -o tests/out/t18-a.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t18-a.txt -o tests/in/adaptive/t18-a-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t18-b.txt -o tests/out/t18-b.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t18-b.txt -o tests/in/adaptive/t18-b-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t18-c.txt -o tests/out/t18-c.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t18-c.txt -o tests/in/adaptive/t18-c-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t18-d.txt -o tests/out/t18-d.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t18-d.txt -o tests/in/adaptive/t18-d-decompressed.txt -d

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t19-a.txt -o tests/out/t19-a.txt -w 4 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t19-a.txt -o tests/in/adaptive/t19-a-decompressed.txt -d
#DIFF_ARGS=tests/in/adaptive/t19-a.txt tests/in/adaptive/t19-a-decompressed.txt

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/adaptive/t20-a.txt -o tests/out/t20-a.txt -w 8 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/t20-a.txt -o tests/in/adaptive/t20-a-decompressed.txt -d
#DIFF_ARGS=tests/in/adaptive/t20-a.txt tests/in/adaptive/t20-a-decompressed.txt

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/kko.proj.data/cb.raw -o tests/out/cb.raw -w 512 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/cb.raw -o tests/in/kko.proj.data/cb.raw-decompressed.txt -d
#DIFF_ARGS=tests/in/kko.proj.data/cb.raw tests/in/kko.proj.data/cb.raw-decompressed.txt

#ARGUMENTS_COMPRESSOR_ADAPTIVE := -i tests/in/kko.proj.data/nk01.raw -o tests/out/nk01.raw -w 512 -c -a
#ARGUMENTS_DECOMPRESSOR := -i tests/out/nk01.raw -o tests/in/kko.proj.data/nk01.raw-decompressed.txt -d
#DIFF_ARGS=tests/in/kko.proj.data/nk01.raw tests/in/kko.proj.data/nk01.raw-decompressed.txt



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

run-static: $(EXECUTABLE)
	$(MAKE) run-compressor-static
	$(MAKE) run-decompressor
	diff $(DIFF_ARGS)

run-compressor-static: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR_STATIC)

run-adaptive:
	if [ $(TR_ARGS) ]; then tr $(TR_ARGS) ; fi
	$(MAKE) run-compressor-adaptive
	$(MAKE) run-decompressor
	diff $(DIFF_ARGS)

run-compressor-adaptive: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_COMPRESSOR_ADAPTIVE)

run-decompressor: $(EXECUTABLE)
	./$(EXECUTABLE) $(ARGUMENTS_DECOMPRESSOR)

diff:
	diff tests/in/t1.txt tests/out/t1.txt && echo "OK" || echo "FAIL"

check-sizes:
	du -sh zadani/kko.proj.data/info.txt tests/out/out.txt

bear: ## Create compilation database
	bear $(MAKE) all
