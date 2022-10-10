cmake_minimum_required(VERSION 3.23)
project(KaldiAligner)

set(CMAKE_CXX_STANDARD 11)

set(KALDI_ROOT /opt/kaldi)

include_directories(include ${KALDI_ROOT}/tools/phonetisaurus-g2p/src/ ${KALDI_ROOT}/tools/phonetisaurus-g2p/src/3rdparty/utfcpp ${KALDI_ROOT}/tools/openfst/include ${KALDI_ROOT}/src /usr/local/cuda/include)
link_directories(${KALDI_ROOT}/tools/openfst/lib ${KALDI_ROOT}/src/lib)

SET(PHONETISAURUS_OBJ ${KALDI_ROOT}/tools/phonetisaurus-g2p/src/lib/phonetisaurus_g2pfst-util.o)

add_compile_definitions(HAVE_CUDA)
add_executable(KaldiAligner src/main.cpp src/g2p.cpp src/lex.cpp src/kaldi.cpp src/signaling.cpp ${PHONETISAURUS_OBJ})
target_link_libraries(KaldiAligner fst kaldi-feat kaldi-base kaldi-util kaldi-matrix kaldi-transform kaldi-ivector kaldi-online2 kaldi-decoder kaldi-hmm kaldi-tree kaldi-nnet3 kaldi-cudamatrix kaldi-lat)
