PROJECT_NAME=uuid2bin
CURDIR := $(shell pwd)
BASEDIR := $(abspath $(CURDIR)/..)

C_SRC_DIR = $(CURDIR)
C_SRC_OUTPUT ?= $(CURDIR)/$(PROJECT_NAME).so

# System type and C compiler/flags.

UNAME_SYS_ORG := $(shell uname -s)
UNAME_SYS = $(shell echo $(UNAME_SYS_ORG) | tr A-Z a-z)

ifeq ($(UNAME_SYS), darwin)
    CC ?= cc
	CFLAGS ?= -arch x86_64 -I/usr/local/include/mysql
	CXXFLAGS ?= -arch x86_64 -I/usr/local/include/mysql
    LDFLAGS ?= -arch x86_64
else ifeq ($(UNAME_SYS), freebsd)
	CC ?= cc
	CFLAGS ?= -I/usr/include/mysql
	CXXFLAGS ?= -I/usr/include/mysql
    LDFLAGS ?= -Wl,--exclude-libs=ALL
else ifeq ($(UNAME_SYS), linux)
	CC ?= gcc
	CFLAGS ?= -I/usr/include/mysql
	CXXFLAGS ?= -I/usr/include/mysql
    LDFLAGS ?= -Wl,--exclude-libs=ALL
endif

CFLAGS += -O3 -std=c99 -Wextra -Wall -fPIC
CXXFLAGS += -O3 -Wextra -Wall -fPIC
LDFLAGS += -shared -lstdc++

# Verbosity.

c_verbose_0 = @echo " C     " $(?F);
c_verbose = $(c_verbose_$(V))

cpp_verbose_0 = @echo " CPP   " $(?F);
cpp_verbose = $(cpp_verbose_$(V))

link_verbose_0 = @echo " LD    " $(@F);
link_verbose = $(link_verbose_$(V))

SOURCES := $(shell find $(C_SRC_DIR) -type f \( -name "*.c" -o -name "*.C" -o -name "*.cc" -o -name "*.cpp" \))
OBJECTS = $(addsuffix .o, $(basename $(SOURCES)))

COMPILE_C = $(c_verbose) $(CC) $(CFLAGS) $(CPPFLAGS) -c
COMPILE_CPP = $(cpp_verbose) $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c

$(C_SRC_OUTPUT): $(OBJECTS)
	@mkdir -p $(BASEDIR)/priv/
	$(link_verbose) $(CC) $(OBJECTS) $(LDFLAGS) -o $(C_SRC_OUTPUT)

%.o: %.c
	$(COMPILE_C) $(OUTPUT_OPTION) $<

%.o: %.cc
	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

%.o: %.C
	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

%.o: %.cpp
	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

clean:
	@rm -f $(C_SRC_OUTPUT) $(OBJECTS);

