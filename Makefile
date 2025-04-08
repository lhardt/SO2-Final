# A C++ makefile. 
# --------------------
# Creates targes based on file names on src/.  If a file 
# is named "src/large_main.cpp", then a "bin/large" target 
# is created.  A test binary can be created with "src/*_test" 
# files using gtest.

# --------------------
# Folders
# --------------------
SRCDIR 	:= src
OBJDIR 	:= obj
BINDIR 	:= bin
INCDIR  := inc
LIBDIR  := lib


# --------------------
# Sources
# --------------------
SRC 	  := $(wildcard $(SRCDIR)/*.cpp)
SRC_TEST  := $(wildcard $(SRCDIR)/*_test.cpp)
SRC_MAIN  := $(wildcard $(SRCDIR)/*_main.cpp)
SRC_OTHER := $(filter-out $(SRC_MAIN) $(SRC_TEST), $(SRC))


# --------------------
# Objects
# --------------------
OBJ   	  := $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
OBJ_TEST  := $(SRC_TEST:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
OBJ_MAIN  := $(SRC_MAIN:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
OBJ_OTHER := $(filter-out $(OBJ_MAIN) $(OBJ_TEST), $(OBJ))


# --------------------
# Targets
# --------------------
TRG_TEST  := $(BINDIR)/test
TRG_MAIN  := $(patsubst $(SRCDIR)/%_main.cpp, $(BINDIR)/%,$(SRC_MAIN)) 
TARGET    := $(TRG_MAIN) $(TRG_TEST)


# --------------------
# Compilation Flags
# --------------------
# Address sanitizers are REALLY helpful, but they aren't available on MINGW.
CXXFLAGS:= -O2 -Iinc -Wall -g -Wall -Wextra 
SANITIZE_FLAGS = -fsanitize=undefined,address


# Remove the sanitizer flags from MINGW-based systems.
UNAME:= $(shell uname)
IS_MINGW:=$(shell if [[ $(UNAME) == *"MINGW"* ]]; then echo 1; else echo 0; fi;)
ifeq ($(IS_MINGW) ,  1)
	SANITIZE_FLAGS:=
endif

# --------------------
# Rules
# --------------------
all: $(TRG_MAIN) 

run: $(TARGET)
	./run.sh

clean:
	-rm $(OBJ) $(TARGET)

folders:
	-@mkdir lib inc obj bin 2>/dev/null | true 

test: $(TRG_TEST)
	./bin/test

obj/%.o: src/%.cpp | folders
	g++  $(CXXFLAGS)  $(SANITIZE_FLAGS) -c $(@:$(OBJDIR)/%.o=$(SRCDIR)/%.cpp) -o $@



$(TRG_MAIN): $(OBJ_MAIN) $(OBJ_OTHER)
	g++ -o $@ $(@:$(BINDIR)/%=$(OBJDIR)/%_main.o) $(OBJ_OTHER) $(LIB) $(CXXFLAGS) $(SANITIZE_FLAGS)


