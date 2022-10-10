#ifndef KALDIALIGNER__EXC_H_
#define KALDIALIGNER__EXC_H_

#include <string>

class MsgException :public std::exception {
private:
	std::string message;
public:
	MsgException(std::string msg) :message(msg) {}

	virtual const char* what() const throw() {
		return message.c_str();
	}
};

#endif