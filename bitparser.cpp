#include "bitparser.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <arpa/inet.h>

using namespace std;

#ifdef DEBUG
#define display(...) \
	do { if (1) fprintf(stdout, __VA_ARGS__);} while(0)
#else
#define display(...) do {} while(0)
#endif

BitParser::BitParser(string filename):
	fieldA(), part_name(), date(), hour(),
	design_name(), userID(), toolVersion(),
	file_length(0), _filename(filename)
{
	bit_data = NULL;
}
BitParser::~BitParser() 
{
	if (bit_data != NULL)
		free(bit_data);
}

int BitParser::parseField(unsigned char type, FILE *fd)
{
	short length;
	char tmp[64];
	int pos, prev_pos;
	if (type != 'e') {
		fread(&length, sizeof(short), 1, fd);
		length = ntohs(length);
	} else {
		length = 4;
	}
	fread(tmp, sizeof(unsigned char), length, fd);
#ifdef DEBUG
	for (int i = 0; i < length; i++)
		printf("%c", tmp[i]);
	printf("\n");
#endif
	switch (type) {
		case 'a': /* design name:userid:synthesize tool version */
			fieldA=(tmp);
			prev_pos = 0;
			pos = fieldA.find(";");
			design_name = fieldA.substr(prev_pos, pos);
			printf("%d %d %s\n", prev_pos, pos, design_name.c_str());
			prev_pos = pos+1;

			pos = fieldA.find(";", prev_pos);
			userID = fieldA.substr(prev_pos, pos-prev_pos);
			printf("%d %d %s\n", prev_pos, pos, userID.c_str());
			prev_pos = pos+1;

			//pos = fieldA.find(";", prev_pos);
			toolVersion = fieldA.substr(prev_pos);
			printf("%d %d %s\n", prev_pos, pos, toolVersion.c_str());
			break;
		case 'b': /* FPGA model */
			part_name = (tmp);
			break;
		case 'c': /* buildDate */
			date = (tmp);
			break;
		case 'd': /* buildHour */
			hour = (tmp);
			break;
		case 'e': /* file size */
			file_length = 0;
			for (int i = 0; i < 4; i++) {
#ifdef DEBUG
				printf("%x %x\n", 0xff & tmp[i], file_length);
#endif
				file_length <<= 8;
				file_length |= 0xff & tmp[i];
			}
#ifdef DEBUG
			printf("    %x\n", file_length);
#endif

			break;
	}
	return length;

}
int BitParser::parse()
{
	unsigned char *tmp_data;
	FILE *fd = fopen(_filename.c_str(), "rb");
	if (fd == NULL) {
		cerr << "Error: failed to open " + _filename << endl;
		return -1;
	}

	short length;
	unsigned char type;
	display("parser\n\n");

	/* Field 1 : misc header */
	fread(&length, sizeof(short), 1, fd);
	length = ntohs(length);
	display("%d\n", length);
	fseek(fd, length, SEEK_CUR);
	fread(&length, sizeof(short), 1, fd);
	length = ntohs(length);
	display("%d\n", length);

	/* process all field */
	fread(&type, sizeof(unsigned char), 1, fd);
	display("Field 2 %c\n", type);
	parseField(type, fd);
	fread(&type, sizeof(unsigned char), 1, fd);
	display("Field 3 %c\n", type);
	parseField(type, fd);
	fread(&type, sizeof(unsigned char), 1, fd);
	display("Field 4 %c\n", type);
	parseField(type, fd);
	fread(&type, sizeof(unsigned char), 1, fd);
	display("Field 5 %c\n", type);
	parseField(type, fd);
	fread(&type, sizeof(unsigned char), 1, fd);
	display("Field 6 %c\n", type);
	parseField(type, fd);

	display("results\n\n");

	cout << "fieldA      : " << fieldA << endl;
	cout << "            : " << design_name << ";" << userID << ";" << toolVersion << endl;
	cout << "part name   : " << part_name << endl;
	cout << "date        : " << date << endl;
	cout << "hour        : " << hour << endl;
	cout << "file length : " << file_length << endl;

	/* rest of the file is data to send */
	bit_data = (unsigned char *)malloc(sizeof(unsigned char) * file_length);
	if (bit_data == NULL) {
		cerr << "Error: data buffer malloc failed" << endl;
		return -1;
	}
	tmp_data = (unsigned char *)malloc(sizeof(unsigned char) * file_length);
	if (tmp_data == NULL) {
		cerr << "Error: data buffer malloc failed" << endl;
		return -1;
	}

	int pos = ftell(fd);
	cout << pos + file_length << endl;
	size_t ret = fread(tmp_data, sizeof(unsigned char), file_length, fd);
	if (ret != (size_t)file_length) {
		cerr << "Error: data read different to asked length" << endl;
		return -1;
	}

	for (int i = 0; i < file_length; i++) {
		bit_data[i] = reverseByte(tmp_data[i]);
	}

	fclose(fd);
	free(tmp_data);

	return 0;
}

unsigned char *BitParser::getData()
{
	return bit_data;
}

unsigned char BitParser::reverseByte(unsigned char src)
{
	unsigned char dst = 0;
	for (int i=0; i < 8; i++) {
		dst = (dst << 1) | (src & 0x01);
		src >>= 1;
	}
	return dst;
}
