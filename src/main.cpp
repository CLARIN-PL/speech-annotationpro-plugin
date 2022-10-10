#include <iostream>
#include "kaldi.h"
#include "g2p.h"

int main(int argc, char** argv) {

	if (argc != 6) {
		std::cerr << "./KaldiAligner [wav] [txt] [models] [out words] [out phones]" << std::endl;
		return -1;
	}


	/*auto g2p = make_g2p("models.studio/g2p/model.fst");
  auto res=g2p->convert("pięknie");
  for (auto t : res) {
	  std::cout << t << std::endl;
  }
  delete g2p;*/

	try {
		KaldiProcess kaldi(argv[3]);

		auto res = kaldi.process(argv[1], argv[2]);

		std::ofstream out_words(argv[4]);
		for (const auto& w : res.words) {
			if (w.text == "<eps>")
				continue;
			out_words << w.start << "\t" << w.end << "\t" << w.text << std::endl;
		}
		out_words.close();

		std::ofstream out_phones(argv[5]);
		for (const auto& p : res.phones) {
			std::string pt = p.text;
			if (pt[pt.size() - 2] == '_')
				pt = pt.substr(0, pt.size() - 2);
			out_phones << p.start << "\t" << p.end << "\t" << pt << std::endl;
		}
		out_phones.close();
	}
	catch (const std::exception& e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "UNKNOWN ERROR" << std::endl;
	}

	return 0;
}
