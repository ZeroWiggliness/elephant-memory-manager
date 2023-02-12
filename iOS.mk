PATH := ${PATH}:/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin:/Developer/usr/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin
ARMOUTPATH=OutDir/iOS/7.0
ARMLIBPATH=Lib/iOS/7.0
CC = clang
ARARM = libtool
IPHONEOS_DEPLOYMENT_TARGET=4.3
IPHONESDKROOT=iPhoneOS7.0
IPHONESIMSDKROOT=iPhoneSimulator7.0
CPU_ARM =  
CCARMFLAGS = -x c++ -arch armv7 -fmessage-length=0 -Wno-trigraphs -fno-exceptions -fno-rtti -fpascal-strings -Wno-missing-field-initializers -Wmissing-prototypes -Wreturn-type -Wno-non-virtual-dtor -Wno-overloaded-virtual -Wno-exit-time-destructors -Wformat -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-newline-eof -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/$(IPHONESDKROOT).sdk -Wdeprecated-declarations -Winvalid-offsetof -g -fno-threadsafe-statics -Wno-conversion -Wno-sign-conversion -mthumb -miphoneos-version-min=4.3
CCSIMFLAGS = -x c++ -arch i386 -fmessage-length=0 -Wno-trigraphs -fno-exceptions -fno-rtti -fpascal-strings -Wno-missing-field-initializers -Wmissing-prototypes -Wreturn-type -Wno-non-virtual-dtor -Wno-overloaded-virtual -Wno-exit-time-destructors -Wformat -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-newline-eof -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/$(IPHONESIMSDKROOT).sdk -fexceptions -fasm-blocks -Wdeprecated-declarations -Winvalid-offsetof -mmacosx-version-min=10.6 -g -fno-threadsafe-statics -Wno-conversion -Wno-sign-conversion -D__IPHONE_OS_VERSION_MIN_REQUIRED=40300
CINCLUDES = -IInclude -ISource
CCCOMPFLAGS = 
SRC_FILES = $(wildcard Source/*.cpp Source/iOS/*.cpp)
OBJ_ARM_FILES = $(addprefix $(ARMOUTPATH)/iPhone/, $(notdir $(SRC_FILES:%.cpp=%.o)))
OBJ_SIM_FILES = $(addprefix $(ARMOUTPATH)/Simulator/, $(notdir $(SRC_FILES:%.cpp=%.o)))

# Leave the space defined.
define compile-source

$(CC) $(CCARMFLAGS) -c $1 -o $(ARMOUTPATH)/iPhone/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_ARM) $(CINCLUDES) $2 $(MEMORYDEFINES)
$(CC) $(CCSIMFLAGS) -c $1 -o $(ARMOUTPATH)/Simulator/$(notdir $(1:%.cpp=%.o)) $(CCCOMPFLAGS) $(CPU_ARM) $(CINCLUDES) $2 $(MEMORYDEFINES)

endef

# Make dirs
MakeDir:
	mkdir -p $(ARMOUTPATH)/iPhone
	mkdir -p $(ARMLIBPATH)/iPhone
	mkdir -p $(ARMOUTPATH)/Simulator
	mkdir -p $(ARMLIBPATH)/Simulator

# Debug
JRSMemory_Debug: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -D_DEBUG -O0))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Debug.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Debug.a $(OBJ_SIM_FILES)
	
# Debug NAC
JRSMemory_Debug_NAC: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -D_DEBUG -O0))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Debug_NAC.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Debug_NAC.a $(OBJ_SIM_FILES)

# Debug S
JRSMemory_Debug_S: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -D_DEBUG -O0))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Debug_S.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Debug_S.a $(OBJ_SIM_FILES)

# Debug NACS
JRSMemory_Debug_NACS: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -D_DEBUG -O0))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Debug_NACS.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Debug_NACS.a $(OBJ_SIM_FILES)

# Release
JRSMemory_Release: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -Os))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Release.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Release.a $(OBJ_SIM_FILES)
	
# Release NAC
JRSMemory_Release_NAC: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -Os))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Release_NAC.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Release_NAC.a $(OBJ_SIM_FILES)

# Release S
JRSMemory_Release_S: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Release_S.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Release_S.a $(OBJ_SIM_FILES)

# Release NACS
JRSMemory_Release_NACS: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_ENABLENAMEANDSTACKCHECKS -DMEMORYMANAGER_ENABLESENTINELCHECKS -Os))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Release_NACS.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Release_NACS.a $(OBJ_SIM_FILES)
	
# Master
JRSMemory_Master: MakeDir
	$(foreach src,$(filter %.cpp,$(SRC_FILES)), $(call compile-source,$(src), -DMEMORYMANAGER_MINIMAL -Os))
	$(ARARM) -static -arch_only armv7 -o $(ARMLIBPATH)/iPhone/libJRSMemory_Master.a $(OBJ_ARM_FILES)
	$(ARARM) -static -arch_only i386 -o $(ARMLIBPATH)/Simulator/libJRSMemory_Master.a $(OBJ_SIM_FILES)
	
# Clean it all
clean:
	rm -r -f $(ARMOUTPATH)
	rm -r -f $(ARMLIBPATH)

# Build all
all: JRSMemory_Master JRSMemory_Release_NACS JRSMemory_Release_S JRSMemory_Release_NAC JRSMemory_Release JRSMemory_Debug_NACS JRSMemory_Debug_S JRSMemory_Debug_NAC JRSMemory_Debug

# Clean and rebuild
rebuild: clean all 