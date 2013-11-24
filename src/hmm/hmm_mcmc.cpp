/* =====================================================================================
 * Copyright (c) 2012, Jonas Maaskola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * =====================================================================================
 *
 *       Filename:  hmm_mcmc.cpp
 *
 *    Description:  Routines to perform Gibbs sampling with HMMs
 *
 *        Created:  Thu Mar 29 21:51:56 2012 +0200
 *
 *         Author:  Jonas Maaskola (JM), jonas@maaskola.de
 *
 * =====================================================================================
 */

#include "hmm.hpp"
#include "../mcmc/mcmchmm.hpp"
#include <random>

std::uniform_int_distribution<size_t> r_binary(0,1);
std::uniform_real_distribution<double> r_unif(0, 1);

void HMM::modify_column(std::mt19937 &rng)
{
  std::uniform_int_distribution<size_t> r_state(first_state, last_state);
  size_t col = r_state(rng);

  std::uniform_int_distribution<size_t> r_nucl(0, alphabet_size - 1);
  size_t i = r_nucl(rng);
  size_t j = r_nucl(rng);
  while(i == j)
    j = r_nucl(rng);
  if(verbosity >= Verbosity::verbose)
    std::cout << "Modifying emissions " << i << " and " << j << " in column " << col << std::endl;
  double rel_amount = r_unif(rng);
  double amount = emission(col,i) * rel_amount;
  emission(col,i) -= amount;
  emission(col,j) += amount;
}

void HMM::modify_transition(std::mt19937 &rng, double eps)
{
  std::uniform_int_distribution<size_t> r_state(0, n_states - 1);
  size_t col = r_state(rng);
  size_t cnt = 0;
  for(size_t i = 0; i < n_states; i++)
    cnt += (transition(col,i) > 0 ? 1 : 0);
  while(cnt <= 1) {
    col = r_state(rng);
    cnt = 0;
    for(size_t i = 0; i < n_states; i++)
      cnt += (transition(col,i) > 0 ? 1 : 0);
  }
  std::uniform_int_distribution<size_t> r_cnt(0, cnt-1);
  std::vector<size_t> present;
  for(size_t i = 0; i < n_states; i++)
    if(transition(col,i) > 0)
      present.push_back(i);
  size_t i = r_cnt(rng);
  size_t j = r_cnt(rng);
  while(i == j)
    j = r_cnt(rng);
  if(verbosity >= Verbosity::verbose)
    std::cout << "Modifying transitions " << present[i] << " and " << present[j] << " in column " << col << std::endl;
  double rel_amount = r_unif(rng);
  double amount = transition(col,present[i]) * rel_amount;
  transition(col,present[i]) -= amount;
  transition(col,present[j]) += amount - eps;
  transition(col,present[i]) = std::max<double>(eps,transition(col,present[i]));
  double z = 0;
  for(size_t i = 0; i < n_states; i++)
    z += transition(col,i);
  for(size_t i = 0; i < n_states; i++)
    transition(col,i) /= z;
}

HMM HMM::random_variant(const hmm_options &options, std::mt19937 &rng) const
{
  HMM candidate(*this);
  unsigned n_cols = n_states - first_state;
  int n_ins = std::max<int>(0,std::min<int>(options.sampling.n_indels, options.sampling.max_size - n_cols));
  int n_del = std::max<int>(0,std::min<int>(options.sampling.n_indels, int(n_cols) - std::max<int>(0,options.sampling.min_size)));

  std::uniform_int_distribution<size_t> r_operation(0, 5);
  std::uniform_int_distribution<size_t> r_ins(1, n_ins);
  std::uniform_int_distribution<size_t> r_del(1, n_del);
  std::uniform_int_distribution<size_t> r_shift(1, options.sampling.n_shift);

  size_t operation = r_operation(rng);
  while((operation >= 5 and options.bg_learning == Training::Method::None) or
      (operation < 5 and options.objectives.empty()) or
      (operation == 2 and n_ins <= 0) or (operation == 3 and n_del <= 0) or (operation == 4 and options.sampling.n_shift == 0))
    operation = r_operation(rng);
  if(verbosity > Verbosity::info)
    std::cout << "operation =  " << operation << std::endl;
  switch(operation) {
    case 0: // modify a column
      candidate.modify_column(rng);
      break;
    case 1: // swap two columns
      candidate.swap_columns(rng);
      break;
    case 2: // add column
      candidate.add_columns(r_ins(rng), rng);
      break;
    case 3: // del column
      candidate.del_columns(r_del(rng), rng);
      break;
    case 4: // shift matrix
      {
        size_t n = r_shift(rng);
        candidate.del_columns(n, rng);
        candidate.add_columns(n, rng);
      }
      break;
    case 5: // modify transition
      candidate.modify_transition(rng);
      break;
    case 6: // modify IC
      break;
  }
  if(options.verbosity >= Verbosity::verbose)
    std::cout << "Generated: " << candidate << std::endl;
  return(candidate);
}
 

void HMM::swap_columns(std::mt19937 &rng)
{
  std::uniform_int_distribution<size_t> r_state(first_state, last_state);
  size_t i = r_state(rng);
  size_t j = r_state(rng);
  while(i == j)
    j = r_state(rng);
  if(verbosity >= Verbosity::verbose)
    std::cout << "Swapping columns " << i << " and " << j << std::endl;
  for(size_t k = 0; k < n_emissions; k++) {
    double temp = emission(i,k);
    emission(i,k) = emission(j,k);
    emission(j,k) = temp;
  }
}
 
void HMM::add_column(size_t n, const std::vector<double> &e)
{
  n_states += 1;
  last_state += 1;
  matrix_t new_emission(n_states, emission.size2());
  for(size_t i = 0; i < n; i++)
    for(size_t j = 0; j < new_emission.size2(); j++)
      new_emission(i,j) = emission(i,j);
  for(size_t j = 0; j < new_emission.size2(); j++)
    new_emission(n,j) = e[j];
  for(size_t i = n+1; i < n_states; i++)
    for(size_t j = 0; j < new_emission.size2(); j++)
      new_emission(i,j) = emission(i-1,j);

  matrix_t new_transition(n_states, n_states);
  for(size_t i = 0; i < n_states-1; i++)
    for(size_t j = 0; j < n_states-1; j++)
      new_transition(i,j) = transition(i,j);

  for(size_t j = 0; j < n_states-1; j++)
    new_transition(n_states-1,j) = transition(n_states-2,j);

  for(size_t j = 0; j < n_states; j++)
    new_transition(j,n_states-1) = 0;

  for(size_t j = 0; j < n_states; j++)
    new_transition(n_states-2, j) = 0;
  new_transition(n_states-2, n_states-1) = 1;

  emission = new_emission;
  transition = new_transition;

  group_ids.insert(group_ids.begin() + n, 1); // TODO make this compatible with the next motif_idx semantics
  order.insert(order.begin() + n, *order.rbegin());
}


void HMM::add_columns(size_t n, std::mt19937 &rng)
{
  size_t pos = r_binary(rng);
  if(verbosity >= Verbosity::verbose)
    std::cout << "Adding " << n << " columns at the " << (pos == 0 ? "beginning" : "end") << "." << std::endl;
  for(size_t j = 0; j < n; j++) {
    size_t n = n_emissions;
    std::vector<double> e(n+1, 0); // FIXME: why + 1 ?
    for(size_t i = 0; i < alphabet_size; i++)
      e[i] = r_unif(rng);

    if(pos == 0)
      add_column(first_state+j, e);
    else
      add_column(n_states, e);
  }
  finalize_initialization();
}

void HMM::del_column(size_t n)
{
  n_states -= 1;
  last_state -= 1;
  matrix_t new_emission(n_states, n_emissions);
  for(size_t i = 0; i < n; i++)
    for(size_t j = 0; j < n_emissions; j++)
      new_emission(i,j) = emission(i,j);
  for(size_t i = n; i < n_states; i++)
    for(size_t j = 0; j < n_emissions; j++)
      new_emission(i,j) = emission(i+1,j);

  // NOTE: the way of handling the transitions as written here is correct only for the SubHMM usage; not for MCMC
  matrix_t new_transition(n_states, n_states);
  for(size_t i = 0; i < n_states; i++)
    for(size_t j = 0; j < n_states; j++) {
      size_t k = i; size_t l = j;
      if(i >= n)
        k++;
      if(j >= n)
        l++;
      new_transition(i,j) = transition(k,l);
    }
//  for(size_t j = 0; j < n_states; j++)
//    new_transition(n_states-1,j) = transition(n_states,j);

  emission = new_emission;
  transition = new_transition;

  group_ids.erase(group_ids.begin()+n);
  order.erase(order.begin()+n);

  for(auto &group: groups) {
    auto iter = std::find(group.states.begin(), group.states.end(), n);
    if(iter != group.states.end()) {
      group.states.erase(iter);
      break; // states are assumed to be part of only one group
    }
  }

  // TODO: purge empty groups?
}

void HMM::del_columns(size_t n, std::mt19937 &rng)
{
  size_t i = r_binary(rng);
  if(verbosity >= Verbosity::verbose)
    std::cout << "Deleting " << n << " columns at the " << (i == 0 ? "beginning" : "end") << "." << std::endl;
  for(size_t j = 0; j < n; j++)
    if(i == 0)
      del_column(first_state);
    else
      del_column(n_states-1);
  finalize_initialization();
}

std::vector<std::list<std::pair<HMM, double>>> HMM::mcmc(const Data::Collection &data,
    const Training::Task &task,
    const hmm_options &options)
{
  double temperature = options.sampling.temperature;
  MCMC::Evaluator<HMM> eval(data, task);
  MCMC::Generator<HMM> gen(options, n_states-first_state);
  MCMC::MonteCarlo<HMM> mcmc(gen, eval, verbosity);
  std::vector<double> temperatures;
  std::vector<HMM> init;
  for(size_t i = 0; i < options.sampling.n_parallel; i++) {
    init.push_back(*this);
    temperatures.push_back(temperature);
    temperature /= 2;
  }
  return(mcmc.parallel_tempering(temperatures, init, options.termination.max_iter));
}

