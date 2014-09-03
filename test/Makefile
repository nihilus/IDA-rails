BUILD_DIR=build
OBJS=../CommCenter.o ../moc_CommCenter.o moc_main.o main.o

CC=gcc
CXX=g++
AR=ar

PLATFORM_ARCH=-m32 -arch i386
PLATFORM_CFLAGS=-g -Wall ${PLATFORM_ARCH} -D__MAC__
PLATFORM_QT=/Users/Shared/Qt/4.8.0/lib

QT_DEFINES      = -D__IDP__ -DNO_OBSOLETE_FUNCS -DQT_PLUGIN -DQT_GUI_LIB -DQT_CORE_LIB -DQT_NAMESPACE=QT -DQT_NAMESPACE_MAC_CRC=2390747911 -DQT_SHARED
QT_CFLAGS       = -pipe -W -fPIC $(QT_DEFINES)
QT_CXXFLAGS     = ${QT_CFLAGS}
QT_INCLUDES     = -I${PLATFORM_QT}/QtCore.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtGui.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtXml.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtNetwork.framework/Headers
QT_INCLUDES    += -F${PLATFORM_QT}
QT_LIBS         = -framework QtCore -framework QtNetwork -framework QtXml -framework QtGui
QT_LDFLAGS      = -headerpad_max_install_names -single_module -F${PLATFORM_QT} -L${PLATFORM_QT} ${QT_LIBS}
QT_MOC          = moc

TEST_INCLUDES   = -I../

all: ${BUILD_DIR} test

clean:
	rm -f ${BUILD_DIR}/*.o
	rm -f ${BUILD_DIR}/*.d
	rm -f test
	rm -f *.o
	rm -f *~

${BUILD_DIR}:
	@mkdir -p ${BUILD_DIR}

moc_%.cpp: %.hpp
	@echo "\tCompiling (moc) $<"
	@$(QT_MOC) -D__MAC__ ${QT_DEFINES} ${QT_INCLUDES} ${TEST_INCLUDES} $< -o $@

%.o: %.cpp
	@echo "\tCompiling (g++) $<"
	@$(CXX) -c ${PLATFORM_CFLAGS} ${QT_CXXFLAGS} ${QT_INCLUDES} ${TEST_INCLUDES} $< -o ${BUILD_DIR}/$@

test: $(OBJS)
	@echo "\tLinking $@"
	@$(CXX) ${PLATFORM_CFLAGS} ${QT_LDFLAGS} -o $@ ${addprefix ${BUILD_DIR}/,$(OBJS)}
