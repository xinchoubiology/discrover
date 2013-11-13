/*
 * =====================================================================================
 *
 *       Filename:  options.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  31.05.2012 06:47:48
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Jonas Maaskola <jonas@maaskola.de>
 *   Organization:  
 *
 * =====================================================================================
 */

#include <iostream>
#include <boost/algorithm/string.hpp>
#include "options.hpp"

using namespace std;


namespace Seeding {

  Options::Options() :
    paths(),
    motif_specifications({}),
    objectives(),
    algorithm(Algorithm::Plasma),
    plasma(Plasma()),
    fire(FIRE()),
    n_threads(1),
    revcomp(false),
    strict(false),
    pseudo_count(1),
    n_seq(0),
    word_stats(false),
    measure_runtime(false),
    n_motifs(1),
    occurrence_filter(OccurrenceFilter::RemoveSequences),
    keep_all(false),
    verbosity(Verbosity::info),
    dump_viterbi(false),
    no_enrichment_filter(false),
    candidate_selection(CandidateSelection::TopN)
    { };

  Options::Plasma::Plasma() :
    max_candidates(100),
    degeneracies(),
    rel_degeneracy(1),
    per_degeneracy(false)
  { };

  Options::FIRE::FIRE() :
    nucleotides_5prime(1),
    nucleotides_3prime(1),
    nr_rand_tests(10),
    redundancy_threshold(5.0)
  { };

  std::istream &operator>>(std::istream &in, CandidateSelection &cand_sel) {
    string token;
    in >> token;
    boost::algorithm::to_lower(token);
    if(token == "topn")
      cand_sel = CandidateSelection::TopN;
    else if(token == "rand" or token == "randtest" or token == "randomizationtest")
      cand_sel = CandidateSelection::RandomizationTest;
    else {
      cout << "Couldn't parse candidate selection type '" << token << "'." << endl;
      exit(-1);
    }
    return(in);
  }
  std::ostream &operator<<(std::ostream &os, const CandidateSelection &cand_sel) {
    switch(cand_sel) {
      case CandidateSelection::TopN:
        os << "TopN";
        break;
      case CandidateSelection::RandomizationTest:
        os << "RandomizationTest";
        break;
    }
    return(os);
  }

  istream &operator>>(istream &in, OccurrenceFilter &filter) {
    string token;
    in >> token;
    if(token == "remove")
      filter = OccurrenceFilter::RemoveSequences;
    else if(token == "mask")
      filter = OccurrenceFilter::MaskOccurrences;
    else {
      cout << "Couldn't parse occurrence filter type '" << token << "'." << endl;
      exit(-1);
    }
    return(in);
  }

  ostream &operator<<(ostream &os, const OccurrenceFilter &filter) {
    switch(filter) {
      case OccurrenceFilter::RemoveSequences:
        os << "remove";
        break;
      case OccurrenceFilter::MaskOccurrences:
        os << "mask";
        break;
    }
    return(os);
  }

  Algorithm parse_algorithm(const string &token_) {
    string token(token_);
    boost::algorithm::to_lower(token);
    if(token == "plasma")
      return(Algorithm::Plasma);
    else if(token == "fire")
      return(Algorithm::FIRE);
    else if(token == "mcmc")
      return(Algorithm::MCMC);
    else if(token == "all")
      return(Algorithm::Plasma | Algorithm::FIRE | Algorithm::MCMC);
    else {
      cout << "Seeding algorithm '" << token_ << "' unknown." << endl
        << "Please use one of 'plasma', 'fire', 'mcmc', or 'all'." << endl
        << "It is also possible to use multiple algorithms by separating them by comma." << endl;
      exit(-1);
    }
  }

  istream &operator>>(istream &in, Algorithm &algorithm) {
    string algorithms;
    in >> algorithms;
    bool first = true;
    size_t pos;
    do {
      pos = algorithms.find(",");
      Algorithm algo = parse_algorithm(algorithms.substr(0, pos));
      if(first) {
        first = false;
        algorithm = algo;
      } else
        algorithm = algorithm | algo;
      algorithms = algorithms.substr(pos+1);
    } while(pos != string::npos);
    Algorithm algo = parse_algorithm(algorithms);
    if(first)
      algorithm = algo;
    else
      algorithm = algorithm | algo;
    return(in);
  }
  ostream &operator<<(ostream &os, const Algorithm &algorithm) {
    bool first = true;
    if((algorithm & Algorithm::Plasma) == Algorithm::Plasma) {
      os << "plasma";
      first = false;
    }
    if((algorithm & Algorithm::FIRE) == Algorithm::FIRE) {
      os << (first ? "" : ",") << "fire";
      first = false;
    }
    if((algorithm & Algorithm::MCMC) == Algorithm::MCMC)
      os << (first ? "" : ",") << "mcmc";
    return(os);
  }

  ostream &operator<<(ostream &out, const Objectives &objectives) {
    bool first = true;
    for(auto &objective: objectives) {
      if(first)
        first = false;
      else
        out << " ";
      out << Specification::to_string(objective);
    }
    return(out);
  }
}

