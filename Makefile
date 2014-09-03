BUILD_DIR=build
OBJS=CommCenter.o moc_CommCenter.o moc_RailsResponder.o Rails.o

CC=gcc
CXX=g++
AR=ar
INT=install_name_tool -change

PLATFORM_ARCH=-m32 -arch i386
PLATFORM_CFLAGS=${PLATFORM_ARCH} -g -Wall -D__MAC__
PLATFORM_LDFLAGS=${PLATFORM_ARCH} -dynamiclib
PLATFORM_QT=/Users/Shared/Qt/4.8.0/lib

IDA_SDK         = ${HOME}/Dropbox/idapro/sdk
IDA_BASE        = /Applications/IDA\ Starter\ 6.3
IDA_APP		= ${IDA_BASE}/idaq.app/Contents/MacOS/idaq
IDA_LIB         = ida
IDA_DIR         = $(IDA_BASE)/idaq.app/Contents/MacOS
IDA_INCLUDES    = -I${IDA_SDK}/include
IDA_PLUGIN_NAME = Rails
IDA_PLUGIN_EXT  = .pmc
IDA_PLUGIN      = ${BUILD_DIR}/${IDA_PLUGIN_NAME}${IDA_PLUGIN_EXT}
IDA_CFLAGS      = -Wextra -Os ${PLATFORM_CFLAGS}
IDA_LDFLAGS     = ${PLATFORM_LDFLAGS} -L$(IDA_DIR) -l$(IDA_LIB)

QT_DEFINES      = -D__IDP__ -DNO_OBSOLETE_FUNCS -DQT_PLUGIN -DQT_GUI_LIB 
QT_DEFINES	+= -DQT_CORE_LIB -DQT_NAMESPACE=QT 
QT_DEFINES 	+= -DQT_NAMESPACE_MAC_CRC=2390747911 -DQT_SHARED
QT_CFLAGS       = -pipe -W -fPIC $(QT_DEFINES)
QT_CXXFLAGS     = ${QT_CFLAGS}
QT_INCLUDES     = -I${PLATFORM_QT}/QtCore.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtGui.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtXml.framework/Headers
QT_INCLUDES    += -I${PLATFORM_QT}/QtNetwork.framework/Headers
QT_INCLUDES    += -F${PLATFORM_QT}
QT_LIBS         = -framework QtCore -framework QtNetwork -framework QtXml 
QT_LIBS	       += -framework QtGui
QT_LDFLAGS      = -headerpad_max_install_names -single_module -F${PLATFORM_QT}
QT_LDFLAGS     += -L${PLATFORM_QT} ${QT_LIBS}
QT_MOC          = moc

.SILENT:

all: ${BUILD_DIR} ${IDA_PLUGIN}

install:
	@echo "\tCP\t${IDA_PLUGIN_NAME}"
	@cp ${IDA_PLUGIN} ${IDA_DIR}/plugins

clean-ida: clean
	rm -f ${IDA_DIR}/plugins/${IDA_PLUGIN_NAME}${IDA_PLUGIN_EXT}

clean:
	rm -f ${BUILD_DIR}/*.o
	rm -f ${BUILD_DIR}/*.d
	rm -f ${IDA_PLUGIN}
	rm -f *~

${BUILD_DIR}:
	@mkdir -p ${BUILD_DIR}

moc_%.cpp: %.hpp
	@echo "\tMOC\t$<"
	@$(QT_MOC) -D__MAC__ ${QT_DEFINES} ${IDA_INCLUDES} ${QT_INCLUDES} \
		$< -o $@

%.o: %.cpp
	@echo "\tCC\t$<"
	@$(CXX) -c $(IDA_CFLAGS) ${QT_CXXFLAGS} ${IDA_INCLUDES} \
		${QT_INCLUDES} $< -o ${BUILD_DIR}/$@

$(IDA_PLUGIN): $(OBJS)
	@echo "\tLD\t$@"
	@$(CXX) $(IDA_LDFLAGS) ${QT_LDFLAGS} -o $@ ${addprefix \
		${BUILD_DIR}/,$(OBJS)}
	@echo "\tINT\tQtGui"
	@${INT} ${PLATFORM_QT}/QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui $@
	@echo "\tINT\tQtCore"
	@${INT} ${PLATFORM_QT}/QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore $@
	@echo "\tINT\tQtXml"
	@${INT} ${PLATFORM_QT}/QtXml.framework/Versions/4/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/4/QtXml $@
	@echo "\tINT\tQtNetwork"
	@${INT} ${PLATFORM_QT}/QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork $@
