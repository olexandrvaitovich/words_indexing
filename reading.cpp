#include <archive.h>
#include <archive_entry.h>
#include <fstream>
#include <sstream>
#include <boost/locale.hpp>
#include <mutex>
#include "reading.h"
#include "ParallelQueue.h"

void read_config(std::string filename, std::vector<std::string> &configurations){
    std::ifstream h(filename, std::ios_base::out);
    auto ss = std::ostringstream{};
    ss << h.rdbuf();
    std::istringstream iss(ss.str());
    std::vector<std::string> text((std::istream_iterator<std::string>(iss)), std::istream_iterator<std::string>());
    configurations = text;
}

std::string read_txt(std::string filename){
    std::vector<std::string> temp_words_list;
    std::ifstream in(filename, std::ios_base::out);
    auto ss = std::ostringstream{};
    ss << in.rdbuf();
    return ss.str();
}
void read_archive(std::string filename, ParallelQueue<std::string> &texts){
    struct archive *a;
    struct archive_entry *entry;
    int r;
    long size;
    long buffsize = 10240;
    char buff[buffsize];
    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    r = archive_read_open_filename(a, filename.c_str(), buffsize);
    if (r != ARCHIVE_OK)
        exit(1);
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (archive_entry_size(entry) > 0) {
            std::stringstream ss;
            size = archive_read_data(a, buff, buffsize);
            while (size > 0) {
                ss << boost::locale::fold_case(boost::locale::normalize(std::string(buff)));
                memset(buff, 0, 10240);
                size = archive_read_data(a, buff, buffsize);
            }
            texts.enque(std::move(ss.str()));
        }
    }
    r = archive_read_free(a);
    if (r != ARCHIVE_OK)
        exit(1);
}