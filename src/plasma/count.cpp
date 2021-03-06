/*
 * =====================================================================================
 *
 *       Filename:  count.cpp
 *
 *    Description:
 *
 *        Created:  31.05.2012 06:47:48
 *         Author:  Jonas Maaskola <jonas@maaskola.de>
 *
 * =====================================================================================
 */

#include "../aux.hpp"
#include "code.hpp"
#include "data.hpp"
#include "../timer.hpp"
#include "count.hpp"

using namespace std;

namespace Seeding {

hash_map_t get_word_counts(const Collection &collection, size_t length,
                           const Options &options) {
  Timer t;

  size_t n_samples = 0;
  for (auto &contrast : collection)
    n_samples += contrast.sets.size();

  if (options.verbosity >= Verbosity::debug)
    cout << "Getting word counts for " << n_samples << " samples." << endl;

  hash_map_t counts;
  count_vector_t default_stats(n_samples);
  for (auto &x : default_stats)
    x = 0;

  size_t idx = 0;
  for (auto &contrast : collection)
    for (auto &dataset : contrast)
      add_counts(dataset, length, counts, idx++, default_stats, options);

  if (options.measure_runtime)
    cerr << "Got word counts of length " + to_string(length) + " in "
            + time_to_pretty_string(t.tock()) << endl;

  return counts;
}

size_t count_motif(const string &seq, const string &motif,
                   const Options &options) {
  size_t cnt = 0;
  auto qiter = begin(motif);
  auto qend = end(motif);
  auto riter = begin(seq);
  auto rend = end(seq);
  while ((riter = search(riter, rend, qiter, qend, iupac_included)) != rend) {
    cnt++;
    if (not options.word_stats)
      return 1;
    riter++;
  }
  if (options.revcomp and (options.word_stats or cnt == 0)) {
    string rev_comp_motif = reverse_complement(motif);
    qiter = begin(rev_comp_motif);
    qend = end(rev_comp_motif);
    riter = begin(seq);
    while ((riter = search(riter, rend, qiter, qend, iupac_included)) != rend) {
      cnt++;
      if (not options.word_stats)
        return 1;
      riter++;
    }
  }
  return cnt;
}

count_vector_t count_motif(const Collection &collection, const string &motif,
                           const Options &options) {
  size_t n_samples = 0;
  for (auto &contrast : collection)
    n_samples += contrast.sets.size();
  count_vector_t stats(n_samples);
  for (auto &x : stats)
    x = 0;

  size_t idx = 0;
  for (auto &contrast : collection)
    for (auto &dataset : contrast) {
      for (auto &seq : dataset)
        stats[idx] += count_motif(seq.sequence, motif, options);
      idx++;
    }
  return stats;
}

void print_counts(const hash_map_t &counts) {
  for (auto &iter : counts) {
    cout << decode(iter.first);
    for (auto &x : iter.second)
      cout << "\t" << x;
    cout << endl;
  }
}

void add_counts(const string &seq_, size_t length, hash_map_t &counts,
                size_t idx, const count_vector_t &default_stats,
                const Options &options) {
  if (options.verbosity >= Verbosity::debug)
    cout << "Adding counts for sequence " << seq_ << endl;
  if (seq_.size() < length)
    return;

  vector<hash_map_t::key_type> words;

  seq_type seq = encode(seq_);

  auto b1 = begin(seq);
  auto b2 = b1;
  std::advance(b2, length);
  const auto e = end(seq);

  do {
    seq_type s(b1, b2);
    if (find_if(b1, b2, degenerate_nucleotide) == b2) {
      if (options.revcomp) {
        auto rc = iupac_reverse_complement(s);
        if (options.word_stats) {
          words.push_back(s);
          words.push_back(rc);
        } else {
          if (lexicographical_compare(begin(s), end(s), begin(rc), end(rc)))
            words.push_back(rc);
          else
            words.push_back(s);
        }
      } else
        words.push_back(s);
    }
    ++b1;
  } while (b2++ != e);

  if (not options.word_stats) {
    sort(begin(words), end(words));
    words.resize(unique(begin(words), end(words)) - begin(words));
  }
  for (auto &w : words) {
    auto iter = counts.find(w);
    if (iter == end(counts)) {
      auto inserted = counts.insert({w, default_stats});
      iter = inserted.first;
    }
    iter->second[idx]++;
  }
}

void add_counts(const Set &dataset, size_t len, hash_map_t &counts, size_t idx,
                const count_vector_t &default_stats, const Options &options) {
  for (auto &seq : dataset)
    add_counts(seq.sequence, len, counts, idx, default_stats, options);
}
}
