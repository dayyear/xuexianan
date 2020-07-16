# mingw32-make clean
# mingw32-make clean & mingw32-make type=static -j
# mingw32-make clean & mingw32-make type=dynamic -j

CXX = g++
CXXFLAGS = -std=c++11 -Wall -fmessage-length=0

# Includes
CXXFLAGS += -I third_party/pugixml-1.10/include
CXXFLAGS += -I third_party/jsoncpp-1.8.4/include
CXXFLAGS += -I third_party/spdlog-1.4.0/include
CXXFLAGS += -I mingw32-7.3.0/opt/include

# Preprocessor
CXXFLAGS += -DSPDLOG_COMPILED_LIB
ifeq ($(type), static)
    CXXFLAGS += -O3
else
    CXXFLAGS += -O3
endif

# Linker flags
ifeq ($(type), static)
    LDFLAGS = -static
endif

# Libraries
LDFLAGS += -L third_party/pugixml-1.10/lib -lpugixml
LDFLAGS += -L third_party/spdlog-1.4.0/lib -lspdlog
LDFLAGS += -L third_party/jsoncpp-1.8.4/lib -ljsoncpp
LDFLAGS += -liconv
LDFLAGS += -L mingw32-7.3.0/opt/lib -lsqlite3

OBJS = obj/xuexianan.o obj/adb.o obj/log.o obj/common.o obj/database.o
EXE = bin/xuexianan.exe

$(EXE): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

-include $(OBJS:.o=.d)

# pull in dependency info for *existing* .o files
obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o obj/$*.o src/$*.cpp
	@$(CXX) -std=c++11 -MM src/$*.cpp > obj/$*.d
	@bin\ad obj/$*.d obj

clean:
	-@del /q bin\xuexianan*.exe obj\*.d obj\*.o 2>NUL

