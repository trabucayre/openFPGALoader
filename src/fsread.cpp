#include "fsparser.hpp"

int main(int argc, char *argv[]) {
	FsParser fs(argv[1], true, 1);
	fs.parse();
	return 0;
}
