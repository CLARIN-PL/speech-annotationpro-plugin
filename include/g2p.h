#ifndef KALDIALIGNER__G2P_H_
#define KALDIALIGNER__G2P_H_

#include <memory>
#include <string>
#include <vector>

class G2P {
 public:
  virtual ~G2P() = default;
  virtual std::vector<std::string> convert(const std::string &text) = 0;
};

G2P *make_g2p(const std::string &model);

#endif //KALDIALIGNER__G2P_H_
