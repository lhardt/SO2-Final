# --------------------
# Folders
# --------------------
SRCDIR 	:= src
OBJDIR 	:= obj
BINDIR 	:= bin
INCDIR  := inc
LIBDIR  := lib

# --------------------
# Files
# --------------------
SRC 	:= $(wildcard $(SRCDIR)/*.cpp)
OBJ 	:= $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET 	:= $(BINDIR)/main

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
all: $(TARGET)

run: $(TARGET)
	./bin/main

clean:
	-rm $(OBJ) $(TARGET)

# The recipe for object files (i.e.  ./obj/XXX.o) depends on its corresponding ./src/XXX.c file.
obj/%.o: src/%.cpp
	g++  $(CXXFLAGS)  $(SANITIZE_FLAGS) -c $(@:$(OBJDIR)/%.o=$(SRCDIR)/%.cpp) -o $@

$(TARGET): $(OBJ)
	g++ -o $(TARGET) $(OBJ) $(LIB) $(CXXFLAGS) $(SANITIZE_FLAGS)
