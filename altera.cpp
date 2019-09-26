#include "altera.hpp"
#include "ftdijtag.hpp"
#include "device.hpp"

Altera::Altera(FtdiJtag *jtag, enum prog_mode mode, std::string filename):Device(jtag, mode, filename),
	_bitfile(filename)
{}
Altera::~Altera()
{}
void Altera::program()
{}
int Altera::idCode()
{}
