#include <iostream>
#include "kaldi.h"
#include "g2p.h"
#include "signaling.h"

int main(int argc, char** argv) {

  if (argc == 2 && std::string(argv[1]) == "-s") {
    do_signal();
    return 0;
  }

  if (argc < 6 || argc > 7) {
    std::cerr << "./KaldiAligner [wav] [txt] [models] [out words] [out phones] <-b>" << std::endl;
    std::cerr << "./KaldiAligner <-s>" << std::endl;
    return -1;
  }

  bool background_mode = false;
  if (argc > 6 && std::string(argv[6]) == "-b") {
    init_wait();
    background_mode = true;
  }

  //try 
  {
    KaldiProcess kaldi(argv[3]);

    while (true) {
      if (background_mode)
        wait_for_signal();

      auto res = kaldi.process(argv[1], argv[2]);

      std::ofstream out_words(argv[4]);
      for (const auto& w : res.words) {
        if (w.text == "<eps>")
          continue;
        out_words << w.start << "\t" << w.end - w.start << "\t" << w.text << std::endl;
      }
      out_words.close();

      std::ofstream out_phones(argv[5]);
      for (const auto& p : res.phones) {
        std::string pt = p.text;
        if (pt[pt.size() - 2] == '_')
          pt = pt.substr(0, pt.size() - 2);
        out_phones << p.start << "\t" << p.end - p.start << "\t" << pt << std::endl;
      }
      out_phones.close();

      if (!background_mode)
        break;
    }
  }
  /*
  catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
  }*/

  return 0;
}
