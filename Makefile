#================#
# COMPILER FLAGS #
#================#

CCFLAGS += -std=c++17 -Werror -Wall -O0 -pthread

#==============#
# INSTALLATION #
#==============#

install : 
	# cnpy installation
	git clone https://github.com/rogersce/cnpy.git model/vendor/cnpy
	mkdir -p model/vendor model/vendor/cnpy/build log
	cd model/vendor/cnpy/build && cmake ..
	cd model/vendor/cnpy/build && sudo make && sudo make install

#============#
# CNPY STUFF #
#============#

LINK_TO_CNPY_FLAGS = -L/usr/local -lcnpy -lz

#=============#
# COMPILATION #
#=============#

MODEL_SRC  = model/model.cpp
MODEL_ASM  = model/model.asm
MODEL_HDRS = model/ThreadCoreScalability.hpp model/Model.hpp 
RENDER_SRC = model/render.cpp
MODEL_EXE  = model/model
RENDER_EXE = model/render

compile_model : ${MODEL_SRC} ${MODEL_HDRS}
	g++ ${CCFLAGS} ${MODEL_SRC} -o ${MODEL_EXE} ${LINK_TO_CNPY_FLAGS}

compile_rendering : ${RENDER_SRC} ${MODEL_HDRS}
	g++ ${CCFLAGS} ${RENDER_SRC} -o ${RENDER_EXE}

compile_profile : ${MODEL_SRC} ${MODEL_HDRS}
	g++ -S ${CCFLAGS} -g ${MODEL_SRC} -o ${MODEL_ASM} ${LINK_TO_CNPY_FLAGS}
	g++    ${CCFLAGS} -g ${MODEL_SRC} -o ${MODEL_EXE} ${LINK_TO_CNPY_FLAGS}

#===========#
# EXECUTION #
#===========#

CONFIG_FILE = res/simulation-0.conf

render : ${RENDER_EXE}
	${RENDER_EXE} ${CONFIG_FILE}

LOG_FILE  = log/10_LUNEV_MACHINE.txt
DATA_FILE = res/2020-03-09-18:00-FERRUM-[600:700:10][-5:+5:0.2].npy

test : compile_model
	@ ${MODEL_EXE} 1 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ ${MODEL_EXE} 8 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}

profile : compile_profile
	valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ${MODEL_EXE} 8 ${CONFIG_FILE} /dev/null /dev/null






