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
 *       Filename:  plasma_stats.hpp
 *
 *    Description:  Data structures to store count data
 *
 *        Created:  Thu May 31 06:47:48 2012 +0200
 *
 *         Author:  Jonas Maaskola (JM), jonas@maaskola.de
 *
 * =====================================================================================
 */

#ifndef  PLASMA_STATS_HPP
#define  PLASMA_STATS_HPP

#include <unordered_map>
#include <map>
#include <string>
#include "../matrix.hpp"
#include "../verbosity.hpp"

namespace Seeding {
  typedef std::unordered_map<std::string, count_vector_t> hash_map_t;
  typedef std::unordered_map<std::string, double> score_map_t;
  typedef std::multimap<double, std::string, std::greater<double>> rev_map_t;
}

struct FisherExactTestResults {
  double p_value;
  double log_p_value;
};


enum class Alternative {
  TwoSided,
  Less,
  Greater
};

FisherExactTestResults fisher_exact_test(const matrix_t &X_,
    double hypothesizedOddsRatio = 1,
    Alternative alternative = Alternative::TwoSided,
    bool confInt = true,
    double confLevel = 0.95,
    Verbosity verbosity=Verbosity::verbose);

#endif   /* ----- #ifndef PLASMA_STATS_HPP ----- */

