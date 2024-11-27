#ifndef KALDIALIGNER_SRC_LEX_H_
#define KALDIALIGNER_SRC_LEX_H_

#include "g2p.h"
#include <fst/fstlib.h>
#include <string>
#include <vector>

class Lexicon {
private:
  G2P *g2p;
  std::map<std::string, std::vector<std::string>> g2p_cache;

  double sil_prob;
  std::string sil_phone;
  std::vector<std::string> phonelist;
  std::string oov_token;

  std::vector<std::string> wordlist;

  std::map<std::string, int32_t> words;
  std::map<std::string, int32_t> phonemes;
  std::map<int32_t, std::string> rwords;
  std::map<int32_t, std::string> rphonemes;
  fst::StdVectorFst lexicon_fst;

public:
  explicit Lexicon(const std::string &lex_file,
                   const std::string &g2p_model_file,
                   const std::string &phone_list_file,
                   const std::string &oov_token_file);
  ~Lexicon();

  void load_file(const std::string &filename);

  const fst::StdVectorFst &get_fst() { return lexicon_fst; }

  std::vector<int32_t> load_transcript(const std::string &filename);
  std::vector<std::string> int2words(std::vector<int32_t> words);
  std::vector<std::string> int2phones(std::vector<int32_t> words);

  const std::vector<std::string> &get_phonelist() { return phonelist; }
};

#endif // KALDIALIGNER_SRC_LEX_H_
