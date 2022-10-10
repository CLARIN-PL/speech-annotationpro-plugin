cmake_minimum_required(VERSION 3.23)
project(KaldiAligner)

set(CMAKE_CXX_STANDARD 11)

include_directories(phonetisaurus include include/kaldi)
link_directories(lib)

add_compile_definitions(HAVE_MKL)
add_executable(KaldiAligner src/main.cpp src/g2p.cpp src/lex.cpp src/kaldi.cpp src/signaling.cpp phonetisaurus/util.cc)
target_link_libraries(KaldiAligner fst kaldi-feat kaldi-base kaldi-util kaldi-matrix kaldi-transform kaldi-ivector kaldi-online2 kaldi-decoder kaldi-hmm kaldi-gmm kaldi-fstext kaldi-tree kaldi-nnet3 kaldi-cudamatrix kaldi-lat mkl_rt mkl_intel_thread mkl_core mkl_intel_lp64)