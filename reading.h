#ifndef UNTITLED1_READING_H
#define UNTITLED1_READING_H

#include <string>
#include <vector>
#include "ParallelQueue.h"

void read_config(std::string filename, std::vector<std::string> &configurations);
std::string read_txt(std::string filename);
void read_archive(std::string filename, ParallelQueue<std::string> &texts_list);


#endif //UNTITLED1_READING_H
