#include "lex.h"
#include "exc.h"
#include <iostream>
#include <set>
#include <cmath>

std::string strip(const std::string& s) {
  auto a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

Lexicon::Lexicon(const std::string& lex_file, const std::string& g2p_model_file, const std::string& phone_list_file) :
  sil_prob(0.5), sil_phone("sil") {
  g2p = make_g2p(g2p_model_file);
  if (!g2p) {
    MsgException exc("Cannot load G2P model: " + g2p_model_file);
    throw exc;
  }
  std::ifstream phone_file(phone_list_file.c_str());
  if (phone_file.fail()) {
    MsgException exc("Cannot load phone list: " + phone_list_file);
    throw exc;
  }
  std::string ph;
  while (!phone_file.fail()) {
    getline(phone_file, ph);
    if (!ph.empty())
      phonelist.emplace_back(ph);
  }
  int c = 0;
  for (const auto& p : phonelist) {
    phonemes[p] = c;
    rphonemes[c] = p;
    c++;
  }
  std::ifstream cache_file(lex_file.c_str());
  std::string line;
  if (cache_file.fail()) {
    MsgException exc("Cannot load G2P lexicon cache: " + lex_file);
    throw exc;
  }
  while (!cache_file.fail()) {
    getline(cache_file, line);
    auto pos = line.find('\t');
    if (pos == std::string::npos || pos >= (line.size() - 1))
      continue;
    auto word = strip(line.substr(0, pos));
    auto trans = strip(line.substr(pos + 1));
    std::stringstream phsstr(trans);
    std::string phtok;
    auto skip = false;
    while (true) {
      phsstr >> phtok;
      if (phsstr.fail())
        break;
      if (phonemes.count(phtok + "_S") == 0 && phonemes.count(phtok) == 0) {
        std::cerr << "ERROR in word " << word << " in lexicon cache: missing phone " << phtok << std::endl;
      }
    }
    if (skip) {
      std::cerr << "Skipping word " << word << std::endl;
      continue;
    }
    if (g2p_cache.count(word) > 0)
      g2p_cache[word].push_back(trans);
    else
      g2p_cache[word] = std::vector < std::string>({ trans });
  }
}

Lexicon::~Lexicon() {
  delete g2p;
}

std::vector<std::string> transcription_to_position_dependent(const std::string& trans) {
  std::vector<std::string> ret;
  std::stringstream sstr(trans);
  std::string ph;
  while (!sstr.eof()) {
    ph = "";
    sstr >> ph;
    if (ph.empty())
      break;
    ret.push_back(ph);
  }
  if (ret.size() == 1) {
    if (ret[0] != "sil" && ret[0] != "sp" && ret[0] != "<eps>")
      ret[0] = ret[0] + "_S";
  }
  else {
    if (ret[0] != "<eps>")
      ret[0] = ret[0] + "_B";
    size_t n = ret.size();
    if (ret[n - 1] != "<eps>")
      ret[n - 1] = ret[n - 1] + "_E";
    for (int i = 1; i < n - 1; i++) {
      if (ret[i] != "<eps>")
        ret[i] = ret[i] + "_I";
    }
  }
  return ret;
}

void Lexicon::load_file(const std::string& filename) {

  std::ifstream file(filename.c_str());
  std::string line, word;
  std::set<std::string> wordset;
  while (!file.fail()) {
    line = "";
    getline(file, line);
    if (line.empty()) continue;
    std::stringstream sstr(line);
    while (!sstr.eof()) {
      word = "";
      sstr >> word;
      if (word.empty())
        break;
      wordset.emplace(word);
    }
  }
  file.close();

  wordlist.clear();
  wordlist.emplace_back("<eps>");
  wordlist.emplace_back("<unk>");
  for (const auto& w : wordset) {
    wordlist.push_back(w);
  }
  wordlist.emplace_back("#0");
  wordlist.emplace_back("<s>");
  wordlist.emplace_back("</s>");

  words.clear();
  rwords.clear();
  int c = 0;
  for (auto const& w : wordlist) {
    words[w] = c;
    rwords[c] = w;
    c++;
  }

  double sil_cost = -log(sil_prob);
  double no_sil_cost = -log(1.0 - sil_prob);

  int start_state = 0;
  int loop_state = 1; //words enter and leave from here
  int sil_state =
    2; // words terminate here when followed by silence; this state has a silence transition to loop_state.
  int next_state = 3;  //the next un-allocated state, will be incremented as we go.

  fst::SymbolTable psyms;
  psyms.AddSymbol("<eps>");
  for (const auto& p : phonelist)
    psyms.AddSymbol(p);
  fst::SymbolTable wsyms;
  wsyms.AddSymbol("<eps>");
  for (const auto& w : wordlist)
    wsyms.AddSymbol(w);

  lexicon_fst.SetInputSymbols(&psyms);
  lexicon_fst.SetOutputSymbols(&wsyms);

  lexicon_fst.AddState();
  lexicon_fst.AddState();
  lexicon_fst.AddState();

  lexicon_fst.SetStart(start_state);

  lexicon_fst.AddArc(start_state, fst::StdArc(0, 0, no_sil_cost, loop_state));
  lexicon_fst.AddArc(start_state, fst::StdArc(0, 0, sil_cost, sil_state));
  lexicon_fst.AddArc(sil_state, fst::StdArc(psyms.Find(sil_phone), 0, 0, loop_state));

  for (const auto& word : wordlist) {
    std::vector<std::string> transcriptions;
    if (word == "<s>" || word == "</s>" || word == "#0")
      continue;
    if (word == "<eps>")
      transcriptions = { "<eps>" };
    else if (word == "<unk>")
      transcriptions = { "sil" };
    else {
      if (g2p_cache.count(word) > 0)
        transcriptions = g2p_cache[word];
      else
        transcriptions = g2p->convert(word);
    }
    if (transcriptions.empty()) {
      transcriptions = { "sil" };
    }
    for (const auto& t : transcriptions) {
      long output_word = wsyms.Find(word);
      double pron_cost = 0; //usually it's: -log(pronprob);
      int cur_state = loop_state;
      auto pron = transcription_to_position_dependent(t);
      for (int i = 0; i < pron.size() - 1; i++) {
        auto p = pron[i];
        lexicon_fst.AddArc(cur_state, fst::StdArc(psyms.Find(p), output_word, pron_cost, next_state));
        lexicon_fst.AddState();
        cur_state = next_state;
        next_state++;
        output_word = 0;
        pron_cost = 0;
      }
      lexicon_fst.AddArc(cur_state, fst::StdArc(psyms.Find(pron.back()), output_word, no_sil_cost + pron_cost, loop_state));
      lexicon_fst.AddArc(cur_state, fst::StdArc(psyms.Find(pron.back()), output_word, sil_cost + pron_cost, sil_state));
    }
  }

  lexicon_fst.SetFinal(loop_state, 0);

  //  wsyms.WriteText("words.txt");
  //  psyms.WriteText("phones.txt");
  //  std::ofstream debug1("words2.txt");
  //  for (auto const &x: words)
  //    debug1 << x.first << " " << x.second << std::endl;
  //  debug1.close();
  //  std::ofstream debug2("phones2.txt");
  //  for (auto const &x: phonemes)
  //    debug2 << x.first << " " << x.second << std::endl;
  //  debug2.close();
}

std::vector<int32_t> Lexicon::load_transcript(const std::string& filename) {
  std::ifstream file(filename.c_str());
  std::string line, word;
  std::vector<int32_t> ret;
  while (!file.fail()) {
    line = "";
    getline(file, line);
    if (line.empty()) continue;
    std::stringstream sstr(line);
    while (!sstr.eof()) {
      word = "";
      sstr >> word;
      if (word.empty())
        break;
      ret.push_back(words[word]);
    }
  }
  file.close();
  return ret;
}

std::vector<std::string> Lexicon::int2words(std::vector<int32_t> trans) {
  std::vector<std::string> ret;
  for (auto w : trans) {
    ret.emplace_back(rwords[w]);
  }
  return ret;
}

std::vector<std::string> Lexicon::int2phones(std::vector<int32_t> trans) {
  std::vector<std::string> ret;
  for (auto w : trans) {
    ret.emplace_back(rphonemes[w]);
  }
  return ret;
}