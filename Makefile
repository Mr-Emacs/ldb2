# Compiler
ifeq ($(OS), Windows_NT)
    ifneq ($(shell where cl 2>nul), )

        CC      = cl
        CFLAGS  = /W4 /O2 /std:c11 /D_CRT_SECURE_NO_WARNINGS /wd4244 /wd4267 /wd4456 /Zi
        LDLIBS  = opengl32.lib gdi32.lib comdlg32.lib
        COMPILE = $(CC) $(CFLAGS) /Fobuild\ /Febuild\ldb2.exe $(SRC) $(LDLIBS)
    else ifneq ($(shell where clang 2>nul), )
        CC      = clang
        CFLAGS  = -Wall -Wextra -std=c11 -O2 -Wno-deprecated-declarations -g
        LDLIBS  = -lopengl32 -lgdi32 -lcomdlg32
        COMPILE = $(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)
    else
        $(error No supported compiler found on Windows. Install clang or MSVC.)
    endif
    EXT := .exe
else
    ifneq ($(shell which gcc 2>/dev/null), )
        CC      = gcc
        CFLAGS  = -Wall -Wextra -std=c11 -O2
        LDLIBS  =
    else ifneq ($(shell which clang 2>/dev/null), )
        CC      = clang
        CFLAGS  = -Wall -Wextra -std=c11 -O2
        LDLIBS  =
    else
        $(error No supported compiler found. Install gcc or clang.)
    endif
    COMPILE = $(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)
    EXT :=
endif

BUILD  := build/
SRC    := main.c debugger.c
TARGET := $(addprefix $(BUILD), $(addsuffix $(EXT), ldb2))

all: $(BUILD) $(TARGET)

$(BUILD):
ifeq ($(OS), Windows_NT)
	if not exist build md build
else
	mkdir -p $(BUILD)
endif

$(TARGET): $(SRC)
	$(COMPILE)

run: $(TARGET)
	$(TARGET)

clean:
ifeq ($(OS), Windows_NT)
	if exist build rd /s /q build
else
	$(RM) -rf $(BUILD)
endif
