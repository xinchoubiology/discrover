/*
 * =====================================================================================
 *
 *       Filename:  align.cpp
 *
 *    Description:  Alignment routines based on suffix arrays
 *
 *        Created:  14.08.2012 22:23:44
 *         Author:  Jonas Maaskola <jonas@maaskola.de>
 *
 * =====================================================================================
 */

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/algorithm/string.hpp>
#include "align.hpp"
#include "fasta.hpp"

using namespace std;

bool iupac_included(char r, char q)
{
  q = tolower(q);
  r = tolower(r);
  if(q==r)
    return(true);
  switch(r) {
    case 'a':
      switch(q) {
        case 'w': case 'm': case 'r': case 'd': case 'h': case 'v': case 'n':
          return true;
        default:
          return false;
      }
    case 'c':
      switch(q) {
        case 's': case 'm': case 'y': case 'b': case 'h': case 'v': case 'n':
          return true;
        default:
          return false;
      }
    case 'g':
      switch(q) {
        case 's': case 'k': case 'r': case 'b': case 'd': case 'v': case 'n':
          return true;
        default:
          return false;
      }
    case 't':
      switch(q) {
        case 'u': case 'w': case 'k': case 'y': case 'b': case 'd': case 'h': case 'n':
          return true;
        default:
          return false;
      }
    case 'u':
      switch(q) {
        case 't': case 'w': case 'k': case 'y': case 'b': case 'd': case 'h': case 'n':
          return true;
        default:
          return false;
      }
    default:
      return(false);
  }
}

string iupac_reverse_complement(const string &s) {
  string t;
  for(string::const_reverse_iterator iter = s.rbegin(); iter != s.rend(); iter++)
    switch(*iter) {
      case 'a':
        t += "t";
        break;
      case 'c':
        t += "g";
        break;
      case 'g':
        t += "c";
        break;
      case 't':
      case 'u':
        t += "a";
        break;
      case 'n':
        t += "n";
        break;
      case 'A':
        t += "T";
        break;
      case 'C':
        t += "G";
        break;
      case 'G':
        t += "C";
        break;
      case 'T':
      case 'U':
        t += "A";
        break;
      case 'N':
        t += "N";
        break;
      case 'b':
        t += 'v';
        break;
      case 'v':
        t += 'b';
        break;
      case 'd':
        t += 'h';
        break;
      case 'h':
        t += 'd';
        break;
      case 's':
        t += 's';
        break;
      case 'w':
        t += 'w';
        break;
      case 'm':
        t += 'k';
        break;
      case 'k':
        t += 'm';
        break;
      case 'r':
        t += 'y';
        break;
      case 'y':
        t += 'r';
        break;
      case 'B':
        t += 'V';
        break;
      case 'V':
        t += 'B';
        break;
      case 'D':
        t += 'H';
        break;
      case 'H':
        t += 'D';
        break;
      case 'S':
        t += 'S';
        break;
      case 'W':
        t += 'W';
        break;
      case 'M':
        t += 'K';
        break;
      case 'K':
        t += 'M';
        break;
      case 'R':
        t += 'Y';
        break;
      case 'Y':
        t += 'R';
        break;
      default:
        throw("We have a problem reversing that string!");
    }
  return(t);
}

string read_fasta_with_boundaries(const vector<string> &paths, vector<size_t> &pos2seq, vector<size_t> &seq2set, size_t n_seq) {
  string s = "";
  size_t set_idx = 0;
  for(auto &path: paths) {
    if(not boost::filesystem::exists(path)) {
      cout << "Error opening file: " << path << " does not exist." << endl;
      exit(-1);
    }

    bool use_gzip = path.substr(path.size() - 3, 3) == ".gz";
    bool use_bzip2 = path.substr(path.size() - 4, 4) == ".bz2";
    ios_base::openmode flags = ios_base::in;
    if(use_gzip or use_bzip2)
      flags |= ios_base::binary;

    ifstream file(path, flags);
    boost::iostreams::filtering_stream<boost::iostreams::input> in;
    if(use_gzip)
      in.push(boost::iostreams::gzip_decompressor());
    if(use_bzip2)
      in.push(boost::iostreams::bzip2_decompressor());
    in.push(file);

    size_t idx = 0;
    auto parsing = [&](Fasta::Entry &&entry) {
      boost::algorithm::to_lower(entry.sequence);
      s += entry.sequence + "$";
      for(size_t i = 0; i < entry.sequence.size() + 1; i++)
        pos2seq.push_back(idx);
      seq2set.push_back(set_idx);
      idx++;
      return(n_seq == 0 or idx < n_seq);
    };
    auto parser = Fasta::make_parser(parsing);
    in >> parser;
    set_idx++;
  }
  return(s);
}

string collapse_contrast(const Seeding::Contrast &contrast, vector<size_t> &pos2seq, vector<size_t> &seq2set) {
  string s;
  size_t set_idx = 0;
  size_t seq_idx = 0;
  for(auto &dataset: contrast) {
    for(auto &seq: dataset) {
      s += seq.sequence + "$";
      for(size_t i = 0; i < seq.sequence.size() + 1; i++)
        pos2seq.push_back(seq_idx);
      seq2set.push_back(set_idx);
      seq_idx++;
    }
    set_idx++;
  }
  return(s);
}

string collapse_collection(const Seeding::Collection &collection, vector<size_t> &pos2seq, vector<size_t> &seq2set, vector<size_t> &set2contrast) {
  string s;
  size_t seq_idx = 0;
  size_t set_idx = 0;
  size_t contrast_idx = 0;
  for(auto &contrast: collection) {
    for(auto &dataset: contrast) {
      for(auto &seq: dataset) {
        s += seq.sequence + "$";
        for(size_t i = 0; i < seq.sequence.size() + 1; i++)
          pos2seq.push_back(seq_idx);
        seq2set.push_back(set_idx);
        seq_idx++;
      }
      set2contrast.push_back(contrast_idx);
      set_idx++;
    }
    contrast_idx++;
  }
  return(s);
}

