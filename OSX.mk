PATH := ${PATH}
OUTPATH=OutDir/OSX
LIBPATH=Lib/OSX
CC = clang
AR = libtool
CCFLAGS = -x c++ -arch x86_64 -fmessage-length=0 -std=gnu++11 -Wno-trigraphs -fno-exceptions -fno-rtti -fpascal-strings -Wno-missing-field-initializers -Wmissing-prototypes -Wreturn-type -Wno-non-virtual-dtor -Wno-overloaded-virtual -Wno-exit-time-destructors -Wformat -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-newline-eof -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk -fexceptions -fasm-blocks -Wdeprecated-declarations -Winvalid-offsetof -mmacosx-version-min=10.6 -g -fno-threadsafe-statics -Wno-conversion -Wno-sign-conversion
CINCLUDES = -IInclude -ISource
CCCOMPFLAGS = 

SRC_FILES = $(wildcard Source/*.cpp Source/OSX/*.cpp)
OBJ_FILES = $(addprefix $(OUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))

# Leave the space defined.
define compile-source

$(CC) $(CCFLAGS) -c $1 -o $(OUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CINCLUDES) $2 $(MEMORYDEFINES)

endef

# Make dirs
MakeDir:
	mkdir -p $(OUTPATH)
	mkdir -p $(LIBPATH)

# Debug
JRSMemory_Debug: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -D_DEBUG -O0))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Debug.a $(OBJ_FILES)

# Debug NAC
JRSMemory_Debug_NAC: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -D_DEBUG -O0))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_FILES)

# Debug S
JRSMemory_Debug_S: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -D_DEBUG -O0))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Debug_S.a $(OBJ_FILES)

# Debug NACS
JRSMemory_Debug_NACS: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -D_DEBUG -O0))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_FILES)

# Release
JRSMemory_Release: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -Os))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Release.a $(OBJ_FILES)
	
# Release NAC
JRSMemory_Release_NAC: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -Os))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_FILES)
	
# Release S
JRSMemory_Release_S: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Release_S.a $(OBJ_FILES)

# Release NACS
JRSMemory_Release_NACS: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_FILES)
	
# Master
JRSMemory_Master: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_MINIMAL -Os))
	$(AR) -static -arch_only x86_64 -o $(LIBPATH)/libJRSMemory_Master.a $(OBJ_FILES)	

# Clean it all
clean:
	rm -r -f $(OUTPATH)
	rm -r -f $(LIBPATH)
	
# Build all
all: JRSMemory_Master JRSMemory_Release_NACS JRSMemory_Release_S JRSMemory_Release_NAC JRSMemory_Release JRSMemory_Debug_NACS JRSMemory_Debug_S JRSMemory_Debug_NAC JRSMemory_Debug

# Clean and rebuild
rebuild: clean all 