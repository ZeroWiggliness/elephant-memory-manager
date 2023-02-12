# Linux 
# NOTE: You must be in this directory when you call make.
# Can be built like so make -f Linux.mk rebuild
PATH := ${PATH}
X86OUTPATH=$(CURDIR)/OutDir/Linux/x86
X86LIBPATH=$(CURDIR)/Lib/Linux/x86
X64OUTPATH=$(CURDIR)/OutDir/Linux/x64
X64LIBPATH=$(CURDIR)/Lib/Linux/x64
CCX86 = g++
ARX86 = ar
CPU_X86 = -m32 -funwind-tables -fstrict-aliasing -funswitch-loops -finline-limit=300
CPU_X64 = -m64 -funwind-tables -fstrict-aliasing -funswitch-loops -finline-limit=300
CCFLAGS = -MMD -MP -fno-exceptions -fno-rtti -Wa,--noexecstack -ffunction-sections -c
CINCLUDES = -IInclude
CCCOMPFLAGS = -g

SRC_FILES = $(wildcard Source/*.cpp Source/Linux/*.cpp)
OBJ_X86_FILES = $(addprefix $(X86OUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))
OBJ_X64_FILES = $(addprefix $(X64OUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))

# Leave the space defined.
define compile-source

$(CCX86) -c $1 -o $(X86OUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_X86) $(CCFLAGS) $(CINCLUDES) $2
$(CCX86) -c $1 -o $(X64OUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_X64) $(CCFLAGS) $(CINCLUDES) $2
endef

# Make dirs
MakeDir:
	mkdir -p $(X86OUTPATH)
	mkdir -p $(X86LIBPATH)
	mkdir -p $(X64OUTPATH)
	mkdir -p $(X64LIBPATH)
	
# Debug
JRSMemory_Debug:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -O0))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Debug.a $(OBJ_X64_FILES)
	
# Debug NAC
JRSMemory_Debug_NAC:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -O0))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_X64_FILES)

# Debug S
JRSMemory_Debug_S:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -O0))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_S.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Debug_S.a $(OBJ_X64_FILES)

# Debug NACS
JRSMemory_Debug_NACS:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -O0))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_X64_FILES)

# Release
JRSMemory_Release:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -fomit-frame-pointer -Os))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Release.a $(OBJ_X64_FILES)
	
# Release NAC
JRSMemory_Release_NAC:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -Os))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_X64_FILES)

# Release S
JRSMemory_Release_S:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -fomit-frame-pointer -Os))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_S.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Release_S.a $(OBJ_X64_FILES)

# Release NACS
JRSMemory_Release_NACS:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_X64_FILES)
	
# Master
JRSMemory_Master:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_MINIMAL -fomit-frame-pointer -Os))
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Master.a $(OBJ_X86_FILES)
	$(ARX86) crs $(X64LIBPATH)/libJRSMemory_Master.a $(OBJ_X64_FILES)
	
# Clean it all
clean:
	rm -r -f $(X86OUTPATH)
	rm -r -f $(X86LIBPATH)
	rm -r -f $(X64OUTPATH)
	rm -r -f $(X64LIBPATH)

# Build all
all: JRSMemory_Master JRSMemory_Release_NACS JRSMemory_Release_S JRSMemory_Release_NAC JRSMemory_Release JRSMemory_Debug_NACS JRSMemory_Debug_S JRSMemory_Debug_NAC JRSMemory_Debug

# Clean and rebuild
rebuild: clean all 
