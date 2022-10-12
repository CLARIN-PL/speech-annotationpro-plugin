#include <feat/wave-reader.h>
#include <hmm/hmm-utils.h>
#include <nnet3/nnet-am-decodable-simple.h>
#include <nnet3/nnet-utils.h>
#include <decoder/faster-decoder.h>
#include <lat/lattice-functions.h>
#include "kaldi.h"

KaldiProcess::KaldiProcess(std::string model_dir) {
  std::string use_gpu = "yes";
  std::string g2p_model_file = model_dir + "/g2p/model.fst";
  std::string g2p_cache_file = model_dir + "/g2p/lexicon.txt";
  std::string mfcc_config = model_dir + "/conf/mfcc_hires.conf";
  std::string model_file = model_dir + "/nnet3/final.mdl";
  std::string tree_file = model_dir + "/nnet3/tree";
  std::string phone_list_file = model_dir + "/phones.list";
  std::string word_boundary_file = model_dir + "/word_boundary.int";

  transition_scale = 1.0;
  self_loop_scale = 1.0;
  beam = 200;

  ivector_config.cmvn_config_rxfilename = model_dir + "/extractor/online_cmvn.conf";
  ivector_config.ivector_period = 10;
  ivector_config.splice_config_rxfilename = model_dir + "/extractor/splice.conf";
  ivector_config.lda_mat_rxfilename = model_dir + "/extractor/final.mat";
  ivector_config.global_cmvn_stats_rxfilename = model_dir + "/extractor/global_cmvn.stats";
  ivector_config.diag_ubm_rxfilename = model_dir + "/extractor/final.dubm";
  ivector_config.ivector_extractor_rxfilename = model_dir + "/extractor/final.ie";
  ivector_config.num_gselect = 5;
  ivector_config.min_post = 0.025;
  ivector_config.posterior_scale = 0.1;
  ivector_config.max_remembered_frames = 1000;
  ivector_config.max_count = 0;
  ivector_config.use_most_recent_ivector = false;

#if HAVE_CUDA == 1
  CuDevice::Instantiate().SelectGpuId(use_gpu);
#endif

  lexicon = std::unique_ptr<Lexicon>(new Lexicon(g2p_cache_file, g2p_model_file, phone_list_file));

  ParseOptions po("");
  MfccOptions mfcc_opts;
  mfcc_opts.Register(&po);
  po.ReadConfigFile(mfcc_config);
  mfcc = std::unique_ptr<Mfcc>(new Mfcc(mfcc_opts));

  TrainingGraphCompilerOptions gopts;
  int32 batch_size = 250;
  gopts.transition_scale = 0.0;  // Change the default to 0.0 since we will generally add the
  // transition probs in the alignment phase (since they change eacm time)
  gopts.self_loop_scale = 0.0;  // Ditto for self-loop probs.

  ReadKaldiObject(tree_file, &tree);

  bool binary;
  Input ki(model_file, &binary);
  trans_model.Read(ki.Stream(), binary);
  am_nnet.Read(ki.Stream(), binary);
  SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
  SetDropoutTestMode(true, &(am_nnet.GetNnet()));
  CollapseModel(CollapseModelConfig(), &(am_nnet.GetNnet()));

  compiler = std::unique_ptr<CachingOptimizingCompiler>(new CachingOptimizingCompiler(
    am_nnet.GetNnet(), decodable_opts.optimize_config));

  WordBoundaryInfoNewOpts opts;

  word_boundary_info = std::unique_ptr<WordBoundaryInfo>(new WordBoundaryInfo(opts, word_boundary_file));

  //  std::ofstream trans_debug("trans.txt");
  //  trans_model.Print(trans_debug, lexicon->get_phonelist());
  //  trans_debug.close();

}

void KaldiProcess::MakeLatticeFromLinear(const std::vector<int32>& ali,
  const std::vector<int32>& words,
  BaseFloat lm_cost,
  BaseFloat ac_cost,
  Lattice* lat_out) {
  typedef LatticeArc::StateId StateId;
  typedef LatticeArc::Weight Weight;
  typedef LatticeArc::Label Label;
  lat_out->DeleteStates();
  StateId cur_state = lat_out->AddState(); // will be 0.
  lat_out->SetStart(cur_state);
  for (size_t i = 0; i < ali.size() || i < words.size(); i++) {
    Label ilabel = (i < ali.size() ? ali[i] : 0);
    Label olabel = (i < words.size() ? words[i] : 0);
    StateId next_state = lat_out->AddState();
    lat_out->AddArc(cur_state,
      LatticeArc(ilabel, olabel, Weight::One(), next_state));
    cur_state = next_state;
  }
  lat_out->SetFinal(cur_state, Weight(lm_cost, ac_cost));
}

Result KaldiProcess::process(std::string wav_file, std::string trans_file) {
  Result ret;
  Matrix<BaseFloat> features;

  WaveData wave_data;
  std::ifstream file(wav_file.c_str(), std::ios::binary);
  wave_data.Read(file);
  file.close();

  SubVector<BaseFloat> waveform(wave_data.Data(), 0);
  mfcc->ComputeFeatures(waveform, wave_data.SampFreq(), 1.0, &features);

  //  BaseFloatMatrixWriter feat_writer("ark:feats.ark");
  //  feat_writer.Write("sent001", features);
  //  feat_writer.Close();

  //  Matrix<double> cmvn_stats;
  //  InitCmvnStats(features.NumCols(), &cmvn_stats);
  //  AccCmvnStats(features, nullptr, &cmvn_stats);

  OnlineIvectorExtractionInfo ivector_info(ivector_config);
  OnlineIvectorExtractorAdaptationState adaptation_state(ivector_info);
  int32 feat_dim = features.NumCols();
  SubMatrix<BaseFloat> range = features.ColRange(0, feat_dim);
  OnlineMatrixFeature matrix_feature(range);
  OnlineIvectorFeature ivector_feature(ivector_info, &matrix_feature);

  ivector_feature.SetAdaptationState(adaptation_state);

  int32 T = features.NumRows(),
    n = ivector_config.ivector_period,
    num_ivectors = (T + n - 1) / n;

  Matrix<BaseFloat> ivectors(num_ivectors, ivector_feature.Dim());

  for (int32 i = 0; i < num_ivectors; i++) {
    int32 t = i * n;
    SubVector<BaseFloat> ivector(ivectors, i);
    ivector_feature.GetFrame(t, &ivector);
  }

  //  BaseFloatMatrixWriter ivector_writer("ark:ivectors.ark");
  //  ivector_writer.Write("sent001", ivectors);
  //  ivector_writer.Close();

  TrainingGraphCompilerOptions gopts;
  gopts.transition_scale = 0.0;  // Change the default to 0.0 since we will generally add the
  // transition probs in the alignment phase (since they change eacm time)
  gopts.self_loop_scale = 0.0;  // Ditto for self-loop probs.

  std::vector<int32> disambig_syms;

  lexicon->load_file(trans_file);

  auto* lex_fst = new fst::StdVectorFst(lexicon->get_fst());
  TrainingGraphCompiler gc(trans_model, tree, lex_fst, disambig_syms, gopts);

  std::vector<int32_t> transcription = lexicon->load_transcript(trans_file);

  fst::VectorFst<fst::StdArc> graph;

  gc.CompileGraphFromText(transcription, &graph);

  //  TableWriter<fst::VectorFstHolder> fst_writer("ark:graph.fst");
  //  fst_writer.Write("sent001", graph);
  //  fst_writer.Close();

  AddTransitionProbs(trans_model, disambig_syms, transition_scale, self_loop_scale, &graph);

  DecodableAmNnetSimple nnet_decodable(
    decodable_opts, trans_model, am_nnet,
    features, NULL, &ivectors,
    ivector_config.ivector_period, compiler.get());

  FasterDecoderOptions decode_opts;
  decode_opts.beam = beam;

  FasterDecoder decoder(graph, decode_opts);
  decoder.Decode(&nnet_decodable);

#if HAVE_CUDA == 1
  CuDevice::Instantiate().PrintProfile();
#endif

  fst::VectorFst<LatticeArc> decoded;  // linear FST.
  decoder.GetBestPath(&decoded);

  std::vector<int32> alignment, words, times, lengths;
  LatticeWeight weight;

  GetLinearSymbolSequence(decoded, &alignment, &words, &weight);

  //  Int32VectorWriter ali_writer("ark,t:ali.txt");
  //  ali_writer.Write("sent001", alignment);
  //  ali_writer.Close();

  Lattice lat;
  MakeLatticeFromLinear(alignment, transcription, 0, 0, &lat);
  CompactLattice clat, aligned_clat;
  ConvertLattice(lat, &clat);

  WordAlignLattice(clat, trans_model, *word_boundary_info, 0, &aligned_clat);

  CompactLatticeToWordAlignment(aligned_clat, &words, &times, &lengths);

  auto word_text = lexicon->int2words(words);
  for (int i = 0; i < word_text.size(); i++)
    ret.words.emplace_back(word_text[i], times[i] / 100.0, (times[i] + lengths[i]) / 100.0);

  ConvertLattice(aligned_clat, &lat);
  ConvertLatticeToPhones(trans_model, &lat);
  ConvertLattice(lat, &aligned_clat);

  CompactLatticeToWordAlignment(aligned_clat, &words, &times, &lengths);

  auto phone_text = lexicon->int2phones(words);
  for (int i = 0; i < phone_text.size(); i++) {
    auto ph = phone_text[i];
    if (ph != "sil" && ph != "sp") {
      ret.phones.emplace_back(ph, times[i] / 100.0, (times[i] + lengths[i]) / 100.0);
    }
  }

  return ret;
}