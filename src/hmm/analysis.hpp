/* =====================================================================================
 * Copyright (c) 2011, Jonas Maaskola
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
 *       Filename:  analysis.hpp
 *
 *    Description:  Header for the top-level analysis routines
 *
 *        Created:  Thu Aug 4 22:12:31 2011 +0200
 *
 *         Author:  Jonas Maaskola <jonas@maaskola.de>
 *
 * =====================================================================================
 */

#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <random>
#include "hmm.hpp"

void perform_analysis(Options::HMM &options, std::mt19937 &rng);

namespace Exception {
namespace Analysis {
struct NotASymlink : public std::runtime_error {
  NotASymlink(const std::string &path);
};
struct MeasureNotForMultiple : public std::runtime_error {
  MeasureNotForMultiple(Measures::Continuous::Measure measure);
};
}
}

#endif
