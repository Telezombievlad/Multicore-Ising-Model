#================#
# COMPILER FLAGS #
#================#

CCFLAGS += -std=c++11 -Werror -Wall -O0 -pthread

#==============#
# INSTALLATION #
#==============#

# install : 
# 	# cnpy installation
# 	git clone https://github.com/rogersce/cnpy.git model/vendor/cnpy
# 	mkdir -p model/vendor model/vendor/cnpy/build log
# 	cd model/vendor/cnpy/build && cmake ..
# 	cd model/vendor/cnpy/build && sudo make && sudo make install

# clean : 


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
	g++ ${CCFLAGS} ${MODEL_SRC} -o ${MODEL_EXE} # ${LINK_TO_CNPY_FLAGS}

compile_rendering : ${RENDER_SRC} ${MODEL_HDRS}
	g++ ${CCFLAGS} ${RENDER_SRC} -o ${RENDER_EXE}

compile_profile : ${MODEL_SRC} ${MODEL_HDRS}
	g++ -S ${CCFLAGS} -g ${MODEL_SRC} -o ${MODEL_ASM} # ${LINK_TO_CNPY_FLAGS}
	g++    ${CCFLAGS} -g ${MODEL_SRC} -o ${MODEL_EXE} # ${LINK_TO_CNPY_FLAGS}

#===========#
# EXECUTION #
#===========#

CONFIG_FILE = res/simulation-0.conf

render : ${RENDER_EXE}
	${RENDER_EXE} ${CONFIG_FILE}

LOG_FILE  = log/16_PERF_BATTLES.log
DATA_FILE = res/2020-03-09-18:00-FERRUM-[600:700:10][-5:+5:0.2].npy

test_ladder : compile_model
	@ ${MODEL_EXE} 1 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m Let me cool down\033[0m\n"
	@ sleep 15
	@ ${MODEL_EXE} 2 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m Let me chill a sec\033[0m\n"
	@ sleep 15
	@ ${MODEL_EXE} 3 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m Let me have a nap, please\033[0m\n"
	@ sleep 15
	@ printf "[CPU]\033[1;33m Oh, here we go AGAIN\033[0m\n"
	@ ${MODEL_EXE} 4 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m It is so hot. Don't do it again, please\033[0m\n"
	@ sleep 15
	@ ${MODEL_EXE} 5 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m I want to go home\033[0m\n"
	@ sleep 15
	@ ${MODEL_EXE} 6 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;31m AAAAA! Stop those senseless computations!\033[0m\n"
	@ sleep 15
	@ printf "[CPU]\033[1;33m No-no-no-no\033[0m\n"
	@ ${MODEL_EXE} 7 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;33m LET ME GOOOOO!\033[0m\n"
	@ sleep 15
	@ printf "[CPU]\033[1;31m YOU BASTARD!!\033[0m\n"
	@ ${MODEL_EXE} 8 ${CONFIG_FILE} ${DATA_FILE} ${LOG_FILE}
	@ printf "[CPU]\033[1;31m ITS TOO HOT. AAAAA!\033[0m\n"

spawn_terminals:
	mate-terminal -x watch 'cat /proc/cpuinfo | grep MHz'
	mate-terminal -x htop
	mate-terminal -x watch 'sensors | grep Core'

profile : compile_profile
	valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes ${MODEL_EXE} 8 ${CONFIG_FILE} /dev/null /dev/null






