# Several things need to be set up for compilation.  We still use cygwin from windows
# but a change recently meant having to put windows paths in.
# ANDROID_NDK_ROOT points to the cygwin Android NDK path (ie /cygdrive/driveletter/androind)
# ANDROID_NDK_ROOTWIN points to the windows Android NDK path (ie driveletter:\android)
# Sorry about that. 
# The rest should just compile all the platforms. 
# NOTE: You must be in this directory when you call make.
# Can be built like so make -f Android.mk rebuild

PATH := ${PATH}:$(ANDROID_NDK_ROOT)/toolchains/x86-4.4.3/prebuilt/windows-x86_64/bin:$(ANDROID_NDK_ROOT)/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows-x86_64/bin:$(ANDROID_NDK_ROOT)/toolchains/mipsel-linux-android-4.4.3/prebuilt/windows-x86_64/bin

ARMOUTPATH=OutDir/Android/armeabi
ARMLIBPATH=Lib/Android/armeabi
CCARM = arm-linux-androideabi-g++
ARARM = arm-linux-androideabi-ar

ARMOUTPATHV7=OutDir/Android/armeabi-v7a
ARMLIBPATHV7=Lib/Android/armeabi-v7a
CCARM = arm-linux-androideabi-g++
ARARM = arm-linux-androideabi-ar

X86OUTPATH=OutDir/Android/x86
X86LIBPATH=Lib/Android/x86
CCX86 = i686-linux-android-g++
ARX86 = i686-linux-android-ar

MIPSOUTPATH=OutDir/Android/mips
MIPSLIBPATH=Lib/Android/mips
CCMIPS = mipsel-linux-android-g++
ARMIPS = mipsel-linux-android-ar

CPU_ARM = -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE_ -Wno-psabi -march=armv5te -mtune=xscale -msoft-float -mthumb -fno-strict-aliasing -finline-limit=64 -fpic -funwind-tables -fstack-protector 
CPU_ARMV7 = -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE_ -Wno-psabi -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -mthumb -fno-strict-aliasing -finline-limit=64 -fpic -funwind-tables -fstack-protector 
CPU_x86 = -funwind-tables -fstrict-aliasing -funswitch-loops -finline-limit=300
CPU_MIPS = -fpic -fno-strict-aliasing -finline-functions -funwind-tables -fmessage-length=0 -fno-inline-functions-called-once -fgcse-after-reload -frerun-cse-after-loop -frename-registers -Wno-psabi -funswitch-loops -finline-limit=300

CCFLAGS = -MMD -MP -DANDROID -fno-exceptions -fno-rtti -Wa,--noexecstack -ffunction-sections 
CINCLUDES = -IInclude -I$(ANDROID_NDK_ROOTWIN)/sources/cxx-stl/system/include
CINCLUDESARM = -I$(ANDROID_NDK_ROOTWIN)/platforms/android-9/arch-arm/usr/include
CINCLUDESX86 = -I$(ANDROID_NDK_ROOTWIN)/platforms/android-9/arch-x86/usr/include
CINCLUDESMIPS = -I$(ANDROID_NDK_ROOTWIN)/platforms/android-9/arch-mips/usr/include
CCCOMPFLAGS = -g

SRC_FILES = $(wildcard Source/*.cpp Source/Android/*.cpp)
OBJ_ARM_FILES = $(addprefix $(ARMOUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))
OBJ_ARMV7_FILES = $(addprefix $(ARMOUTPATHV7)/, $(notdir $(SRC_FILES:%.cpp=%.o)))
OBJ_X86_FILES = $(addprefix $(X86OUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))
OBJ_MIPS_FILES = $(addprefix $(MIPSOUTPATH)/, $(notdir $(SRC_FILES:%.cpp=%.o)))

# Leave the space defined.
define compile-source
@echo $(CCARM)
$(CCARM) -c $1 -o $(ARMOUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_ARM) $(CCFLAGS) $(CINCLUDES) $(CINCLUDESARM) $2 $(MEMORYDEFINES) 

$(CCARM) -c $1 -o $(ARMOUTPATHV7)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_ARMV7) $(CCFLAGS) $(CINCLUDES) $(CINCLUDESARM) $2 $(MEMORYDEFINES) 



$(CCMIPS) -c $1 -o $(MIPSOUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_MIPS) $(CCFLAGS) $(CINCLUDES) $(CINCLUDESMIPS) $2 $(MEMORYDEFINES)

$(CCX86) -c $1 -o $(X86OUTPATH)/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_X86) $(CCFLAGS) $(CINCLUDES) $(CINCLUDESX86) $2 $(MEMORYDEFINES) 

endef


# Make dirs
MakeDir:
	mkdir -p $(ARMOUTPATH)
	mkdir -p $(ARMLIBPATH)
	mkdir -p $(ARMOUTPATHV7)
	mkdir -p $(ARMLIBPATHV7)
	mkdir -p $(X86OUTPATH)
	mkdir -p $(X86LIBPATH)
	mkdir -p $(MIPSOUTPATH)
	mkdir -p $(MIPSLIBPATH)
	
# Debug
JRSMemory_Debug:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -O0))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Debug.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Debug.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Debug.a $(OBJ_MIPS_FILES)
	
# Debug NAC
JRSMemory_Debug_NAC:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -O0))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Debug_NAC.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Debug_NAC.a $(OBJ_MIPS_FILES)

# Debug S
JRSMemory_Debug_S:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -O0))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Debug_S.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Debug_S.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_S.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Debug_S.a $(OBJ_MIPS_FILES)

# Debug NACS
JRSMemory_Debug_NACS:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -O0))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Debug_NACS.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Debug_NACS.a $(OBJ_MIPS_FILES)

# Release
JRSMemory_Release:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -fomit-frame-pointer -Os))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Release.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Release.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Release.a $(OBJ_MIPS_FILES)
	
# Release NAC
JRSMemory_Release_NAC:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -Os))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Release_NAC.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Release_NAC.a $(OBJ_MIPS_FILES)

# Release S
JRSMemory_Release_S:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -fomit-frame-pointer -Os))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Release_S.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Release_S.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_S.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Release_S.a $(OBJ_MIPS_FILES)

# Release NACS
JRSMemory_Release_NACS:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Release_NACS.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Release_NACS.a $(OBJ_MIPS_FILES)
	
# Master
JRSMemory_Master:	MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_MINIMAL -fomit-frame-pointer -Os))
	$(ARARM) crs $(ARMLIBPATH)/libJRSMemory_Master.a $(OBJ_ARM_FILES)
	$(ARARM) crs $(ARMLIBPATHV7)/libJRSMemory_Master.a $(OBJ_ARMV7_FILES)
	$(ARX86) crs $(X86LIBPATH)/libJRSMemory_Master.a $(OBJ_X86_FILES)
	$(ARMIPS) crs $(MIPSLIBPATH)/libJRSMemory_Master.a $(OBJ_MIPS_FILES)
	
# Clean it all
clean:
	rm -r -f $(ARMOUTPATH)
	rm -r -f $(ARMLIBPATH)

# Build all
all: JRSMemory_Master JRSMemory_Release_NACS JRSMemory_Release_S JRSMemory_Release_NAC JRSMemory_Release JRSMemory_Debug_NACS JRSMemory_Debug_S JRSMemory_Debug_NAC JRSMemory_Debug

# Clean and rebuild
rebuild: clean all 