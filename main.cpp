#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <iterator>
#include <thread>
#include "time.h"
#include <vector>
#include <boost/locale.hpp>
#include <boost/locale/boundary.hpp>
#include <set>
#include <condition_variable>
#include "reading.h"
#include "ParallelQueue.h"
#include <boost/filesystem.hpp>
#include "boost/asio.hpp"
#include <boost/asio/thread_pool.hpp>
#include <boost/thread.hpp>
#include <boost/thread/detail/thread_group.hpp>
#include <boost/bind.hpp>
typedef std::map<std::string, int> map;


std::vector<std::string> preprocess(std::string text){
    std::vector<std::string> words_list;
    boost::locale::boundary::ssegment_index map(boost::locale::boundary::word,text.begin(),text.end());
    map.rule(boost::locale::boundary::word_any);
    for(auto it=map.begin(), e=map.end();it!=e;it++){
        words_list.emplace_back(*it);
    }
    for(auto &el:words_list){
        el = boost::locale::fold_case(boost::locale::normalize(el));
    }
    return words_list;
}
void get_filenames(ParallelQueue<std::string> &inx_queue, std::string &path, std::condition_variable &names_to_text_cv, std::mutex &mut) {
    boost::filesystem::recursive_directory_iterator i(path), end;
    while (i != end) {
        boost::filesystem::path current(*i);
        auto filetype = boost::locale::fold_case(boost::locale::normalize(std::string(current.string().end()-3, current.string().end())));
        if (not boost::filesystem::is_directory(boost::filesystem::status(current))) {
            if (filetype == "zip" || filetype == "txt") {
                {
                    std::lock_guard<std::mutex> loc(mut);
                    inx_queue.enque(current.string());
                }
                names_to_text_cv.notify_one();
            }
        }
        i++;
    }
    {
        std::lock_guard<std::mutex> loc(mut);
        inx_queue.enque("");
        names_to_text_cv.notify_one();
    }
}

void config_reader(std::string &config_name, std::string &indir,
                   std::string &out_a, std::string &out_n, int &inx_thr, int &merg_thr){
    std::string temp;
    std::ifstream config_stream(config_name);
    config_stream >> temp;
    indir = std::string(std::find(temp.begin(), temp.end(), '=') +2,temp.end() - 1);
    config_stream >> temp;
    out_a =  std::string(std::find(temp.begin(), temp.end(), '=') +2,temp.end() - 1);
    config_stream >> temp;
    out_n = std::string(std::find(temp.begin(), temp.end(), '=') +2,temp.end() - 1);
    config_stream >> temp;
    inx_thr = std::stoi(std::string(std::find(temp.begin(), temp.end(), '=') + 1,temp.end()));
    config_stream >> temp;
    merg_thr = std::stoi(std::string(std::find(temp.begin(), temp.end(), '=') + 1,temp.end()));
    config_stream.close();
}

void read_from_queue(ParallelQueue<std::string> &files_q, std::condition_variable &index_v, ParallelQueue<std::string> &strings_q, std::condition_variable &read_v, std::mutex &mut){
    while(true){
        std::unique_lock<std::mutex> loc(mut);
        index_v.wait(loc, [&files_q]{return !files_q.isEmpty();});
        if(!files_q.isEmpty()){
            auto filename = files_q.deque();
            if(filename == ""){
                files_q.enque(filename);
                index_v.notify_one();
                break;
            }
            if (std::string(filename.end()-3, filename.end())=="txt" || std::string(filename.end()-3, filename.end())=="TXT" ){
                strings_q.enque(read_txt(filename));
            }
            else{
                read_archive(filename, std::ref(strings_q));
            }
            loc.unlock();
            read_v.notify_one();
        }

    }
    {
        std::lock_guard<std::mutex> loc(mut);
        strings_q.enque("");
        read_v.notify_one();
    }
}

void counting(ParallelQueue<std::string> &strings_q, std::condition_variable &read_v, ParallelQueue<map> &counting_q, std::condition_variable &counting_v, std::mutex & mut){
    while(true){
        map temp_map;
        std::unique_lock<std::mutex> loc(mut);
        read_v.wait(loc, [&strings_q]{return !strings_q.isEmpty();});
        if(!strings_q.isEmpty()) {
            auto str = strings_q.deque();
            if(str==""){
                strings_q.enque(str);
                break;
            }
            auto words_list = preprocess(str);
            for (auto &w:words_list) {
                temp_map[w]++;
            }
            counting_q.enque(temp_map);
            counting_v.notify_one();

        }
        loc.unlock();
    }
    {
        std::lock_guard<std::mutex> loc(mut);
        map stop_map;
        counting_q.enque(stop_map);

    }
    counting_v.notify_one();

}

void merging(ParallelQueue<map> &counting_q, std::condition_variable &counting_v, std::mutex &mut){
    map res_map;
    while(true){
        std::unique_lock<std::mutex> loc(mut);
        map first;
        counting_v.wait(loc, [&counting_q]{return !counting_q.isEmpty();});
        if(!counting_q.isEmpty()){
            first = counting_q.deque();
            if(first.empty()){
                while(first.empty() and !counting_q.isEmpty()){
                    first = counting_q.deque();
                }
                if(first.empty()){
                    counting_q.enque(res_map);
                    counting_v.notify_one();
                    break;
                }
            }
            loc.unlock();
            for(auto itr=first.begin();itr!=first.end();itr++){
                res_map[itr->first]+=itr->second;
            }
        }
    }
}


int main(int argc, char* argv[]) {

    int inx_thr, merg_thr;
    std::string out_a, out_n, conffilename = (argc == 2) ? argv[1]: "../config.dat", indir;
    std::mutex mtx;
    //Checking whether conf file exists
    std::ifstream conf_stream(conffilename);
    if (!conf_stream.is_open()) {
        std::cerr << "No configuraion file found";
        return 1;
    }
    conf_stream.close();

    //Reaading config file
    try {
        config_reader(std::ref(conffilename), std::ref(indir),
                      std::ref(out_a), std::ref(out_n), std::ref(inx_thr), std::ref(merg_thr));
    } catch (std::exception &e) {
        std::cerr << "Couldn't read config file"<< e.what()<< std::endl;
        return 1;
    }

    //Opening checking whether output files are present
    std::fstream alph_stream(out_a), num_stream(out_n);
    if (!alph_stream.is_open() || !num_stream.is_open()) {
        std::cerr << "Coudn't reach the output files" << std::endl;
        return 1;
    }
    alph_stream.close();
    num_stream.close();

    boost::locale::generator gen;
    std::locale loc = gen("");
    std::locale::global(loc);
    std::wcout.imbue(loc);

    boost::asio::io_service ioservice;
    boost::asio::io_service::work work_(ioservice);
    boost::thread_group tg;
    for(int i=0;i<8;i++){
        tg.create_thread(boost::bind(&boost::asio::io_service::run, &ioservice));
    }


    std::condition_variable index_v;
    std::condition_variable read_v;
    std::condition_variable counting_v;

    ParallelQueue<std::string>path_q;
    ParallelQueue<std::string>text_q;
    ParallelQueue<map> counting_q;
    std::mutex index_mut;
    std::mutex read_mut;
    std::mutex counting_mut;
    std::mutex merging_mut;

    auto start = get_current_time_fenced();

    ioservice.post(boost::bind(get_filenames, std::ref(path_q), std::ref(indir), std::ref(index_v), std::ref(index_mut)));

    ioservice.post(boost::bind(read_from_queue, std::ref(path_q), std::ref(index_v), std::ref(text_q), std::ref(read_v), std::ref(read_mut)));

    ioservice.post(boost::bind(counting, std::ref(text_q), std::ref(read_v), std::ref(counting_q), std::ref(counting_v), std::ref(counting_mut)));

    ioservice.post(boost::bind(merging, std::ref(counting_q), std::ref(counting_v), std::ref(merging_mut)));
    std::cout<<""<<std::endl; //Please don`t touch, it is the  most importent part


    ioservice.stop();
    tg.join_all();

    auto finish = get_current_time_fenced();
    std::cout << to_us(finish-start)<<std::endl;


    map result = counting_q.deque();

    std::vector<std::pair<std::string, size_t>> pair_vector(result.size());
    std::copy(result.begin(), result.end(), pair_vector.begin());
    std::sort(pair_vector.begin(), pair_vector.end(),
              [](std::pair<std::string, size_t> &first, std::pair<std::string, size_t> &second) {
                  return first.first < second.first;
              });

    std::ofstream a_stream(out_a);
    for(auto &word:pair_vector){
        a_stream<<word.first<<" :  "<<word.second<<"\n";
    }
    a_stream.close();

    std::sort(pair_vector.begin(), pair_vector.end(),
              [](std::pair<std::string, size_t> &first, std::pair<std::string, size_t> &second) {
                  return first.second > second.second;
              });

    std::ofstream n_stream(out_n);
    for(auto &word:pair_vector){
        n_stream<<word.first<<" :  "<<word.second<<"\n";
    }
    n_stream.close();
    return 0;
}
