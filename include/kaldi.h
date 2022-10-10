#ifndef KALDIALIGNER_INCLUDE_KALDI_H_
#define KALDIALIGNER_INCLUDE_KALDI_H_

#include <string>
#include <utility>
#include <nnet3/nnet-common.h>
#include <feat/feature-mfcc.h>
#include <fst/vector-fst.h>
#include <online2/online-ivector-feature.h>
#include <decoder/training-graph-compiler.h>
#include <tree/context-dep.h>
#include <decoder/decoder-wrappers.h>
#include <nnet3/nnet-am-decodable-simple.h>
#include <lat/word-align-lattice.h>
#include "lex.h"

using namespace kaldi;
using namespace kaldi::nnet3;

class Segment {
 public:
  Segment(std::string _text, double _start, double _end) : text(std::move(_text)), start(_start), end(_end) {}
  double start, end;
  std::string text;
};

class Result {
 public:
  std::vector<Segment> words, phones;
};

class KaldiProcess {
 private:
  std::unique_ptr<Lexicon> lexicon;
  std::unique_ptr<Mfcc> mfcc;
  OnlineIvectorExtractionConfig ivector_config;
  NnetSimpleComputationOptions decodable_opts;
  ContextDependency tree;
  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  std::unique_ptr<CachingOptimizingCompiler> compiler;
  std::unique_ptr<WordBoundaryInfo> word_boundary_info;

  BaseFloat transition_scale;
  BaseFloat self_loop_scale;
  BaseFloat beam;

  void MakeLatticeFromLinear(const std::vector<int32> &ali,
                             const std::vector<int32> &words,
                             BaseFloat lm_cost,
                             BaseFloat ac_cost,
                             Lattice *lat_out);

 public:
  KaldiProcess(std::string model_dir);

  Result process(std::string wav_file, std::string trans_file);

};

#endif //KALDIALIGNER_INCLUDE_KALDI_H_
