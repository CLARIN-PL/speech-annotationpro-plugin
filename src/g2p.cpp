#include "g2p.h"
using std::string;
#include "include/PhonetisaurusScript.h"
using namespace fst;

class PhonetisaurusModel : public G2P {
private:
	std::unique_ptr<PhonetisaurusScript> phonetisaurus;
public:
	explicit PhonetisaurusModel(const std::string& model) {
		phonetisaurus = std::unique_ptr<PhonetisaurusScript>(new PhonetisaurusScript(model));
	}

	std::vector<std::string> convert(const std::string& text) override {
		std::vector<std::string> ret;
		const SymbolTable* osyms = phonetisaurus->osyms_;
		vector<PathData> results = phonetisaurus->Phoneticize(text, 10, 10000, 99, false, false, 0.22);
		for (const auto& d : results) {
			std::string w;
			for (auto u : d.Uniques) {
				w += osyms->Find(u) + " ";
			}
			if(!w.empty())
				ret.push_back(w);
		}
		return ret;
	}
};

G2P* make_g2p(const std::string& model) {
	G2P* ret;
	try {
		ret = new PhonetisaurusModel(model);
	}
	catch (exception e) {
		return nullptr;
	}
	return ret;
}
