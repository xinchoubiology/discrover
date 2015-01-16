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
 *       Filename:  main.cpp
 *
 *    Description:  Executable for the HMM package
 *
 *        Created:  Thu Aug 4 22:12:31 2011 +0200
 *
 *         Author:  Jonas Maaskola <jonas@maaskola.de>
 *
 * =====================================================================================
 */

#include <sys/resource.h>  // for getrusage
#include <iostream>
#include <fstream>
#include <omp.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "../random_distributions.hpp"
#include "../plasma/cli.hpp"
#include "../logo/cli.hpp"
#include "analysis.hpp"
#include "../aux.hpp"
#include "../terminal.hpp"
#include "../random_seed.hpp"
#include "../mcmc/montecarlo.hpp"
#include "../timer.hpp"
#include <git_config.hpp>

using namespace std;

string gen_usage_string(const string &program_name) {
  string usage = "This program implements hidden Markov models for probabilistic sequence analysis, "
    "in particular for the purpose of discovering unknown binding site patterns in nucleic acid sequences. "
    "It may be used to train models using sequence data, evaluate models on sequence data. "
    "Several learning objectives are implemented. "
    "Please refer to the description for the --score option below.\n"
   "\n"
    "Among these objectives there are two non-discriminative measures: "
    "Viterbi learning and maximum likelihood learning using the Baum-Welch method. "
    "The other measures are discriminative and use gradient-based learning.\n"
    "The discriminative measures require the specification of multiple data sets. "
    "\n"
//    "Except for mutual information, the discriminative methods require paired signal and control "
//    "data sets.\n"
    "Here is an example call to train a motif seeded with the IUPAC string 'tgtanata' on "
    "sequences in files signal.fa and control.fa. By default, it will train in hybrid mode, using "
    "mutual information for the emission probabilities of the motifs, and likelihood maximization "
    "for everything else (background emission and all transition probabilities). Output will be "
    "written to files whose file name is prefixed by 'label':\n"
    "\n"
    "" + program_name + " signal.fa control.fa -m tgtanata -o label\n"
    "\n"
    "Similarly, to automatically find a motif of length 8 one can use the following:\n"
    "\n"
    "" + program_name + " signal.fa control.fa -m 8 -o label\n"
    "\n"
    "When the option --output is not given, a unique output prefix will be automatically generated.\n"
    "\n"
    "It is possible to use just a single FASTA file for discriminative sequence analysis. "
    "In this case a control set of sequences will be generated by shuffling the signal sequences:\n"
    "\n"
    "" + program_name + " signal.fa -m 8\n"
    "\n"
     "Note that names may be given to the motifs to annotate in which of the samples they are "
    "respectively expected to be enriched, as the following example demonstrates.\n"
    "\n"
    "" + program_name + " A:sample1.fa B:sample2.fa -m A:8 -m B:6\n"
    "\n"
    "Here, A and B are arbitrary labels given to motifs of length 8 and 6 that are to be enriched in sample1 and sample2, respectively.\n"
    "\n"
    "Options may also be specified in a configuration file, by using --config. "
    "Options given as arguments on the command line take precedence over configuration file entries. "
    "For options accepted multiple times, parameters from command line and configuration file are joined.\n"
    "\n"
    "Various output files are generated, including the resulting parameters, summary information, motif occurrence tables, and the Viterbi parse. "
#if CAIRO_FOUND
    "Also, sequence logos of the found motifs are automatically generated. "
#endif
    "See the description of the --output option below for details.";
  return usage;
}

Measures::Discrete::Measure measure2iupac_objective(const Measure measure) {
  if (Measures::is_generative(measure))
    return Measures::Discrete::Measure::SignalFrequency;
  switch (measure) {
    case Measure::DeltaFrequency:
      return Measures::Discrete::Measure::DeltaFrequency;
    case Measure::MatthewsCorrelationCoefficient:
      return Measures::Discrete::Measure::MatthewsCorrelationCoefficient;
    default:
      return Measures::Discrete::Measure::MutualInformation;
  }
}

void fixup_seeding_options(Options::HMM &options) {
  // TODO REACTIVATE if(set_objective)
  // TODO REACTIVATE    options.seeding.objective =
  // {measure2iupac_objective(options.measure);
  // options.seeding.objective = Seeding::Objective::corrected_logp_gtest;
  if (options.seeding.objectives.size() == 1
      and begin(options.seeding.objectives)->measure
          == Measures::Discrete::Measure::SignalFrequency)
    options.seeding.plasma.rel_degeneracy = 0.2;
  options.seeding.paths = options.paths;
  options.seeding.n_threads = options.n_threads;
  options.seeding.n_seq = options.n_seq;
  options.seeding.weighting = options.weighting;
  // options.seeding.verbosity = Verbosity::info;
  options.seeding.verbosity = options.verbosity;
  options.seeding.revcomp = options.revcomp;
  options.seeding.pseudo_count = options.contingency_pseudo_count;
  options.seeding.measure_runtime = options.timing_information;
  options.seeding.label = options.label;
#if CAIRO_FOUND
  options.logo.revcomp = options.revcomp;
  options.seeding.logo = options.logo;
  options.seeding.logo.pdf_logo = false;
  options.seeding.logo.png_logo = false;
#endif
  options.seeding.mcmc.max_iter = options.termination.max_iter;
  options.seeding.mcmc.temperature = options.sampling.temperature;
  options.seeding.mcmc.n_parallel = options.sampling.n_parallel;
  options.seeding.mcmc.random_salt = options.random_salt;
}

int main(int argc, const char **argv) {
  const string default_error_msg
      = "Please inspect the command line help with -h or --help.";
  Timer timer;

  namespace po = boost::program_options;

  Options::HMM options;
  options.n_threads = omp_get_num_procs();
  options.exec_info
      = ExecutionInformation(argv[0], GIT_DESCRIPTION, GIT_BRANCH, argc, argv);
  options.class_model = false;
  options.random_salt = generate_rng_seed();

  string config_path;

  string background_initialization;

  static const size_t MIN_COLS = 60;
  static const size_t MAX_COLS = 80;
  size_t cols = get_terminal_width();
  if (cols < MIN_COLS)
    cols = MIN_COLS;
  if (cols > MAX_COLS)
    cols = MAX_COLS;

  // Declare the supported options.
  po::options_description generic_options("Generic options", cols);
  po::options_description basic_options("Basic options, required", cols);
  po::options_description basic_options_optional("Basic options", cols);
  po::options_description init_options("Initialization options", cols);
  po::options_description eval_options("Evaluation options", cols);
  po::options_description advanced_options("Advanced options", cols);
  po::options_description multi_motif_options("Multiple motif mode options", cols);
  po::options_description mmie_options("MMIE options", cols);
  po::options_description linesearching_options("Line searching options", cols);
  po::options_description sampling_options("Gibbs sampling options", cols);
  po::options_description termination_options("Termination options", cols);
  po::options_description hidden_options("Hidden options", cols);

  po::options_description seeding_options = gen_plasma_options_description(
      options.seeding, "",
      "Seeding options for IUPAC regular expression finding", cols, false,
      false);

#if CAIRO_FOUND
  po::options_description logo_options
      = gen_logo_options_description(options.logo, Logo::CLI::HMM, cols);
#endif

  generic_options.add_options()
    ("config", po::value<string>(&config_path), "Read options from a configuration file. ")
    ("help,h", "Produce help message. Combine with -v or -V for additional commands.")
    ("version", "Print out the version. Also show git SHA1 with -v.")
    ("verbose,v", "Be verbose about the progress.")
    ("noisy,V", "Be very verbose about the progress.")
    ;

  basic_options.add_options()
    ("fasta,f", po::value<vector<Specification::Set>>(&options.paths)->required(),
     "FASTA file definition. "
     "May be given multiple times. "
     "Syntax as follows:\n"
     "\n"
     "[NAMES:[CONTRAST:]]PATH\n"
     "\n"
     "NAMES    \tcomma-separated list of names of motifs enriched in this file\n"
     "CONTRAST \tname of a contrast this file belongs to\n"
     "PATH     \tpath of FASTA file with sequences\n"
     "\n"
     "NAMES and CONTRAST are optional.\n"
     "\n"
     "FASTA files can be grouped into contrasts on which the --score definitions (see below) can act.\n"
     "\n"
     "If the path contains at least one colon, please prepend with colons to disambiguate.\n"
     "\n"
     "Note: usage of -f / --fasta is optional; all free arguments are taken to be paths of FASTA files."
     "\n"
     // TODO note usage of the 'control' motif name
     )
    ("motif,m", po::value<vector<Specification::Motif>>(&options.motif_specifications),
     "Motif definition. "
     "May be given multiple times. "
     "Syntax as follows:\n"
     "\n"
     "[NAME:[INSERT:]]MOTIFSPEC\n"
     "\n"
     "NAME      \tname of the motif\n"
     "INSERT    \tpositions after which insertions are allowed\n"
     "MOTIFSPEC \tmotif specification; see below\n"
     "\n"
     "NAME and INSERT are optional.\n"
     "\n"
     "The INSERT string is a comma-separated list of positions after which to add an insertion state. "
     "1-indexed; 1 ≤ position < motif length must hold for each position.\n"
     "\n"
     "MOTIFSPEC can be given in multiple ways:\n"
     "1. \tUsing the IUPAC code for nucleic acids.\n"
     "2. \tA length specification. "
     "Motifs of the indicated lengths are sought. "
     "A length specification is a comma separated list of length ranges, "
     "where a length range is either a single length, or an expression of the form 'x-y' to indicate lengths x up to y. "
     "A length specification also allows to specify a multiplicity separated by an 'x'. "
     "Thus examples are '8' for just length 8, '5-7' for lengths 5, 6, and 7, '5-8x2' for two motifs of lengths 5 to 8, '5-8,10x3' for three motifs of lengths 5, 6, 7, 8, and 10.\n"
     "3. \tBy specifying the path to a file with a PWM. "
     "If the path contains at least one colon, please prepend colons to disambiguate.")
   ;

  Training::Objective default_objective;
  default_objective.measure = Measure::MutualInformation;

  basic_options_optional.add_options()
    ("score", po::value<Training::Objectives>(&options.objectives)->default_value(Training::Objectives(1,default_objective), "mi"),
     "Score definition. "
     "May be given multiple times. "
     "Syntax as follows:\n"
     "\n"
     "[MOTIF:[CONTRASTS:]]MEASURE\n"
     "\n"
     "MOTIF     \tname of the motif to optimize\n"
     "CONTRASTS \tcontrast specification\n"
     "MEASURE   \tsignificance measure to use\n"
     "See below for details.\n"
     "\n"
     "MOTIF and CONTRASTS are optional.\n"
     "\n"
     "If no motif name is given, all motifs are optimized with this score specification. "
     "It is an error when more than one score specification is given for any motif.\n"
     "\n"
     "The contrast specification is a set of contrast names prefixed by '+' or '-' to indicate whether the score on the contrast is to count positively or negatively. "
     "The first contrast name, if lacking a '+' or '-', is taken to count positively.\n"
     "\n"
     "The following significance measures are available:\n"
     "none    \tDo not perform learning\n"
     "bw      \tLikelihood using Baum-Welch\n"
     "viterbi \tViterbi learning\n"
     "mi      \tMutual information of condition and motif occurrence (MICO)\n"
     "ri      \tRank mutual information\n"
     "mmie    \tMaximum mutual information estimation (MMIE), a.k.a. posterior classification probability\n"
     "mcc     \tMatthews correlation coefficient\n"
     "dlogl   \tLog-likelihood difference, like DME, see doi: 10.1073/pnas.0406123102\n"
     "dfreq   \tDifference of frequency of sequences with motif occurrences, similar to DIPS and DECOD, see doi: 10.1093/bioinformatics/btl227 and 10.1093/bioinformatics/btr412"
     "\n")
    ("revcomp,r", po::bool_switch(&options.revcomp), "Respect motif occurrences on the reverse complementary strand. Useful for DNA sequence motif analysis. Default is to consider only occurrence on the forward strand.")
    ;

  advanced_options.add_options()
    ("output,o", po::value<string>(&options.label), (
      "Output file names are generated from this label. If not given, the output label will be '" + options.exec_info.program_name + "_XXX' where XXX is a string to make the label unique. The output files comprise:\n"
      ".hmm     \tParameter of the trained HMM. May be loaded later with --learn.\n"
      ".summary \tSummary information with number of occurrences of the motifs in each sample, and various generative and discriminative statistics.\n"
      ".viterbi \tFASTA sequences annotated with the Viterbi path, and sequence level statistics.\n"
      ".table   \tCoordinates and sequences of matches to the motifs in all sequences.\n"
      "Note that, depending on the argument of --compress, the latter two files may be compressed, and require decompression for inspection.\n"
      "Also, sequence logos of the found motifs are generated with file names based on this output label."
     ).c_str())
    ("threads", po::value<size_t>(&options.n_threads), "Number of threads. If not given, as many are used as there are CPU cores on this machine.")
    ("time", po::bool_switch(&options.timing_information), "Output information about how long certain parts take to execute.")
    ("cv", po::value<size_t>(&options.cross_validation_iterations)->default_value(0), "Number of cross validation iterations to do.")
    ("cv_freq", po::value<double>(&options.cross_validation_freq)->default_value(0.9, "0.9"), "Fraction of data samples for training in cross validation.")
    ("nseq", po::value<size_t>(&options.n_seq)->default_value(0), "Use only the first N sequences of each file. Use 0 to indicate all sequences.")
    ("iter", po::value<size_t>(&options.termination.max_iter)->default_value(1000), "Maximal number of iterations to perform in training. A value of 0 means no limit, and that the training is only terminated by the tolerance.")
    ("salt", po::value<unsigned int>(&options.random_salt), "Seed for the pseudo random number generator (used e.g. for sequence shuffle generation and MCMC sampling). Set this to get reproducible results.")
    ("weight", po::bool_switch(&options.weighting), "When combining objective functions across multiple contrasts, combine values by weighting with the number of sequences per contrasts.")
    ;

  init_options.add_options()
    ("load,l", po::value<vector<string>>(&options.load_paths), "Load HMM parameters from a .hmm file produced by an earlier run. Can be specified multiple times; then the first parameter file will be loaded, and motifs of the following parameter files are added.")
    ("alpha", po::value<double>(&options.alpha)->default_value(0.03, "0.03"), "Probability of alternative nucleotides. The nucleotides not included in the IUPAC character will have this probability.")
    ("lambda", po::value<double>(&options.lambda)->default_value(1), "Initial value for prior with which a motif is expected.")
    ("wiggle", po::value<size_t>(&options.wiggle)->default_value(0), "For automatically determined seeds, consider variants shifted up- and downstream by up to the specified number of positions.")
    ("extend", po::value<size_t>(&options.extend)->default_value(0), "Extend seeds by this many Ns up- and downstream before HMM training.")
    ("padl", po::value<size_t>(&options.left_padding)->default_value(0), "Add this many Ns upstream (to the left) of the seed.")
    ("padr", po::value<size_t>(&options.right_padding)->default_value(0), "Add this many Ns downstream (to the right) of the seed.")
    ;

  eval_options.add_options()
    ("posterior", po::bool_switch(&options.evaluate.print_posterior), "During evaluation also print out the motif posterior probability.")
    ("condmotif", po::bool_switch(&options.evaluate.conditional_motif_probability), "During evaluation compute for every position the conditional motif likelihood considering only the motif emissions.")
    ("nosummary", po::bool_switch(&options.evaluate.skip_summary), "Do not print summary information.")
    ("noviterbi", po::bool_switch(&options.evaluate.skip_viterbi_path), "Do not print the Viterbi path.")
    ("notable", po::bool_switch(&options.evaluate.skip_occurrence_table), "Do not print the occurrence table.")
    ("ric", po::bool_switch(&options.evaluate.perform_ric), "Perform a rank information coefficient analysis.")
    ;

  multi_motif_options.add_options()
    ("multiple", po::bool_switch(&options.multi_motif.accept_multiple), "Accept multiple motifs as long as the score increases. This can only be used with the objective function MICO.")
    ("relearn", po::value<Options::MultiMotif::Relearning>(&options.multi_motif.relearning)->default_value(Options::MultiMotif::Relearning::Full, "full"), "When accepting multiple motifs, whether and how to re-learn the model after a new motif is added. Choices: 'none', 'reest', 'full'.")
    ("resratio", po::value<double>(&options.multi_motif.residual_ratio)->default_value(5.0), "Cutoff to discard new motifs in multi motif mode. The cutoff is applied on the ratio of conditional mutual information of the new motif and the conditions given the previous motifs. Must be non-negative. High values discard more motifs, and lead to less redundant motifs.")
    ;

  mmie_options.add_options()
    ("classp", po::value<double>(&options.class_prior)->default_value(0.5),
     "Initial class prior.")
    ("motifp1", po::value<double>(&options.conditional_motif_prior1)->default_value(0.6, "0.6"),
     "Initial conditional motif prior for the signal class.")
    ("motifp2", po::value<double>(&options.conditional_motif_prior2)->default_value(0.03, "0.03"),
     "Initial conditional motif prior for the control class.")
    ("noclassp", po::bool_switch(&options.dont_learn_class_prior),
     "Don't learn the class prior.")
    ("nomotifp", po::bool_switch(&options.dont_learn_conditional_motif_prior),
     "Don't learn the conditional motif prior.")
    ;


  linesearching_options.add_options()
    ("LSmu", po::value<double>(&options.line_search.mu)->default_value(0.1, "0.1"), "The parameter µ for the Moré-Thuente line search algorithm.")
    ("LSeta", po::value<double>(&options.line_search.eta)->default_value(0.5, "0.5"), "The parameter η for the Moré-Thuente line search algorithm.")
    ("LSdelta", po::value<double>(&options.line_search.delta)->default_value(0.66, "0.66"), "The parameter delta for the Moré-Thuente line search algorithm.")
    ("LSnum", po::value<size_t>(&options.line_search.max_steps)->default_value(10, "10"), "How many gradient and function evaluation to perform maximally per line search.")
    ;

  termination_options.add_options()
    ("gamma", po::value<double>(&options.termination.gamma_tolerance)->default_value(1e-4, "1e-4"), "Tolerance for the reestimation type learning methods. Training stops when the L1 norm of the parameter change between iterations is less than this value.")
    ("delta", po::value<double>(&options.termination.delta_tolerance)->default_value(1e-4, "1e-4"), "Relative score difference criterion tolerance for training algorithm termination: stops iterations when (f - f') / f < delta, where f' is the objective value of the past iteration, and f is the objective value of the current iteration.")
    ("epsilon", po::value<double>(&options.termination.epsilon_tolerance)->default_value(0), "Gradient norm criterion tolerance for training algorithm termination: stops when ||g|| < epsilon * max(1, ||g||), where ||.|| denotes the Euclidean (L2) norm.")
    ("past", po::value<size_t>(&options.termination.past)->default_value(1), "Distance for delta-based convergence test. This parameter determines the distance, in iterations, to compute the rate of decrease of the objective function.")
    ;

  sampling_options.add_options()
    ("sampling", po::bool_switch(&options.sampling.do_sampling), "Perform Gibbs sampling for parameter inference instead of re-estimation or gradient learning.")
    ("temp", po::value<double>(&options.sampling.temperature)->default_value(1e-3), "When performing Gibbs sampling use this temperature. The temperatures of parallel chains is decreasing by factors of two.")
//    ("anneal", po::value<double>(&options.sampling.anneal_factor)->default_value(0.98,"0.98"), "When performing Gibbs sampling multiply the temperature by this term in each iteration.")
    ("smin", po::value<int>(&options.sampling.min_size), "Minimal length for Gibbs sampling. When unspecified defaults to initial motif length.")
    ("smax", po::value<int>(&options.sampling.max_size), "Maximal length for Gibbs sampling. When unspecified defaults to initial motif length.")
    ("nindel", po::value<size_t>(&options.sampling.n_indels)->default_value(5), "Maximal number of positions that may be added or removed at a time. Adding and removing of happens at and from the ends of the motif.")
    ("nshift", po::value<size_t>(&options.sampling.n_shift)->default_value(5), "Maximal number of positions that the motif may be shifted by.")
    ("partemp", po::value<size_t>(&options.sampling.n_parallel)->default_value(6), "Number of chains in parallel tempering.")
    ;

  hidden_options.add_options()
    ("nosave", po::bool_switch(&options.dont_save_shuffle_sequences), "Do not save generated shuffle sequences.")
    ("bglearn", po::value<Training::Method>(&options.bg_learning)->default_value(Training::Method::Reestimation, "em"), "How to learn the background. Available are 'fixed', 'em', 'gradient', where the 'em' uses re-estimation to maximize the likelihood contribution of the background parameters, while 'gradient' uses the discriminative objective function.")
    ("pscnt", po::value<double>(&options.contingency_pseudo_count)->default_value(1.0, "1"), "The pseudo count to be added to the contingency tables in the discriminative algorithms.")
    ("pscntE", po::value<double>(&options.emission_pseudo_count)->default_value(1.0, "1"), "The pseudo count to be added to the expected emission probabilities before normalization in the Baum-Welch algorithm.")
    ("pscntT", po::value<double>(&options.transition_pseudo_count)->default_value(0.0, "0"), "The pseudo count to be added to the expected transition probabilities before normalization in the Baum-Welch algorithm.")
    ("compress", po::value<Options::Compression>(&options.output_compression)->default_value(Options::Compression::gzip, "gz"), "Compression method for larger output files. Available are: 'none', 'gz' or 'gzip', 'bz2' or 'bzip2'.") // TODO make the code conditional on the presence of zlib
    ("miseeding", po::bool_switch(&options.use_mi_to_seed), "Disregard automatic seeding choice and use MICO for seeding.")
    ("absthresh", po::bool_switch(&options.termination.absolute_improvement), "Whether improvement should be gauged by absolute value. Default is relative to the current score.")
    ("intermediate", po::bool_switch(&options.store_intermediate), "Write out intermediate parameters during training.")
    ("limitlogp", po::bool_switch(&options.limit_logp), "Do not report corrected log-P values greater 0 but report 0 in this case.")
    ("longnames", po::bool_switch(&options.long_names), "Form longer output file names that contain some information about the parameters.")
    ;

  advanced_options
    .add(multi_motif_options)
    .add(mmie_options)
    .add(sampling_options);

  hidden_options
    .add(linesearching_options)
    .add(eval_options)
    .add(termination_options);

  po::options_description simple_options;
  simple_options
    .add(basic_options)
    .add(basic_options_optional);

  po::options_description common_options;
  common_options
    .add(advanced_options)
    .add(seeding_options)
    .add(init_options);
#if CAIRO_FOUND
  common_options
    .add(logo_options);
#endif

  po::options_description visible_options;
  visible_options
    .add(generic_options)
    .add(simple_options);

  po::options_description cmdline_options;
  cmdline_options
    .add(generic_options)
    .add(simple_options)
    .add(common_options)
    .add(hidden_options);

  po::options_description config_file_options;
  config_file_options
    .add(simple_options)
    .add(common_options)
    .add(hidden_options);


  po::positional_options_description pos;
  pos.add("fasta", -1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(cmdline_options)
                  .positional(pos)
                  .run(),
              vm);
  } catch (po::unknown_option &e) {
    cout << "Error while parsing command line options:" << endl << "Option "
         << e.get_option_name() << " not known." << endl << default_error_msg
         << endl;
    return EXIT_FAILURE;
  } catch (po::ambiguous_option &e) {
    cout << "Error while parsing command line options:" << endl << "Option "
         << e.get_option_name() << " is ambiguous." << endl << default_error_msg
         << endl;
    return EXIT_FAILURE;
  } catch (po::multiple_values &e) {
    cout << "Error while parsing command line options:" << endl << "Option "
         << e.get_option_name() << " was specified multiple times." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::multiple_occurrences &e) {
    cout << "Error while parsing command line options:" << endl << "Option "
         << e.get_option_name() << " was specified multiple times." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::invalid_option_value &e) {
    cout << "Error while parsing command line options:" << endl
         << "The value specified for option " << e.get_option_name()
         << " has an invalid format." << endl << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::too_many_positional_options_error &e) {
    cout << "Error while parsing command line options:" << endl
         << "Too many positional options were specified." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::invalid_command_line_syntax &e) {
    cout << "Error while parsing command line options:" << endl
         << "Invalid command line syntax." << endl << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::invalid_command_line_style &e) {
    cout << "Error while parsing command line options:" << endl
         << "There is a programming error related to command line style."
         << endl << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::reading_file &e) {
    cout << "Error while parsing command line options:" << endl
         << "The config file can not be read." << endl << default_error_msg
         << endl;
    return EXIT_FAILURE;
  } catch (po::validation_error &e) {
    cout << "Error while parsing command line options:" << endl
         << "Validation of option " << e.get_option_name() << " failed." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (po::error &e) {
    cout << "Error while parsing command line options:" << endl
         << "No further information as to the nature of this error is "
            "available, please check your command line arguments." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  } catch (exception &e) {
    cout << "An error occurred while parsing command line options." << endl
         << e.what() << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  }

  options.verbosity = Verbosity::info;
  if (vm.count("verbose"))
    options.verbosity = Verbosity::verbose;
  if (vm.count("noisy"))
    options.verbosity = Verbosity::debug;

  if (vm.count("version") and not vm.count("help")) {
    cout << options.exec_info.program_name << " "
         << options.exec_info.hmm_version << " [" << GIT_BRANCH << " branch]"
         << endl;
    if (options.verbosity >= Verbosity::verbose)
      cout << GIT_SHA1 << endl;
    return EXIT_SUCCESS;
  }

  if (vm.count("help")) {
    cout << options.exec_info.program_name << " "
         << options.exec_info.hmm_version << endl;
    cout << "Copyright (C) 2011 Jonas Maaskola\n"
            "Provided under GNU General Public License Version 3 or later.\n"
            "See the file COPYING provided with this software for details of "
            "the license.\n" << endl;
    cout << limit_line_length(gen_usage_string(options.exec_info.program_name),
                              cols) << endl << endl;
    cout << visible_options << endl;
    switch (options.verbosity) {
      case Verbosity::nothing:
      case Verbosity::error:
      case Verbosity::info:
        cout << "Advanced and hidden options not shown. Use -hv or -hV to show "
                "them." << endl;
        break;
      case Verbosity::verbose:
        cout << common_options << endl;
        cout << "Hidden options not shown. Use -hV to show them." << endl;
        break;
      case Verbosity::debug:
      case Verbosity::everything:
        cout << common_options << endl;
        cout << hidden_options << endl;
        break;
    }
    return EXIT_SUCCESS;
  }

  try {
    po::notify(vm);
  } catch (po::required_option &e) {
    cout << "Error while parsing command line options:" << endl
         << "The required option " << e.get_option_name()
         << " was not specified." << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  }

  if (config_path != "") {
    ifstream ifs(config_path.c_str());
    if (!ifs) {
      cout << "Error: can not open config file: " << config_path << endl
           << default_error_msg << endl;
      return EXIT_FAILURE;
    } else {
      try {
        store(parse_config_file(ifs, config_file_options), vm);
      } catch (po::multiple_occurrences &e) {
        cout << "Error while parsing config file:" << endl << "Option "
             << e.get_option_name() << " was specified multiple times." << endl
             << default_error_msg << endl;
        return EXIT_FAILURE;
      } catch (po::unknown_option &e) {
        cout << "Error while parsing config file:" << endl << "Option "
             << e.get_option_name() << " not known." << endl
             << default_error_msg << endl;
        return EXIT_FAILURE;
      } catch (po::invalid_option_value &e) {
        cout << "Error while parsing config file:" << endl
             << "The value specified for option " << e.get_option_name()
             << " has an invalid format." << endl
             << default_error_msg << endl;
        return EXIT_FAILURE;
      }
      notify(vm);
    }
  }

  // generate an output path stem if the user did not specify one
  if (not vm.count("output")) {
    options.label = generate_random_label(options.exec_info.program_name, 0,
                                          options.verbosity);
    while (boost::filesystem::exists(options.label + ".hmm"))
      options.label = generate_random_label(options.exec_info.program_name, 5,
                                            options.verbosity);
    if (options.verbosity >= Verbosity::info)
      cout << "Using \"" << options.label
           << "\" as label to generate output file names." << endl;
  }

  // set the number of threads with OpenMP
  omp_set_num_threads(options.n_threads);

  // print information about specified motifs, paths, and objectives
  if (options.verbosity >= Verbosity::debug) {
    cout << "motif_specifications:";
    for (auto &x : options.motif_specifications)
      cout << " " << x;
    cout << endl;
    cout << "paths:";
    for (auto &x : options.paths)
      cout << " " << x;
    cout << endl;
    cout << "objectives:";
    for (auto &x : options.objectives)
      cout << " " << x;
    cout << endl;
  }

  try {
    // check and harmonize specified motifs, paths, and objectives
    Specification::harmonize(options.motif_specifications, options.paths,
                             options.objectives, false);
  } catch (runtime_error &e) {
    cout << e.what() << endl;
    return EXIT_FAILURE;
  }

  // print information about specified motifs, paths, and objectives
  if (options.verbosity >= Verbosity::debug) {
    cout << "motif_specifications:";
    for (auto &x : options.motif_specifications)
      cout << " " << x;
    cout << endl;
    cout << "paths:";
    for (auto &x : options.paths)
      cout << " " << x;
    cout << endl;
    cout << "objectives:";
    for (auto &x : options.objectives)
      cout << " " << x;
    cout << endl;
  }

  // ensure a positive number, if anything, is specified for sampling min_size
  if (vm.count("smin")) {
    if (options.sampling.min_size < 0) {
      cout << "Please note that the minimal length for MCMC sampling must be "
              "non-negative." << endl << default_error_msg << endl;
      return EXIT_FAILURE;
    }
  } else
    options.sampling.min_size = -1;

  // ensure a positive number, if anything, is specified for sampling max_size
  if (vm.count("smax")) {
    if (options.sampling.max_size < 0) {
      cout << "Please note that the maximal length for MCMC sampling must be "
              "non-negative." << endl << default_error_msg << endl;
      return EXIT_FAILURE;
    } else if (options.sampling.max_size < options.sampling.min_size) {
      cout << "Please note that the maximal length for MCMC sampling must be "
              "larger than the minimal size." << endl
              << default_error_msg << endl;
      return EXIT_FAILURE;
    }
  } else
    options.sampling.max_size = -1;

  // Initialize the plasma options
  fixup_seeding_options(options);

  if (options.termination.past == 0) {
    cout << "Error: the value of --past must be a number greater than 0."
         << endl;
  }

  if (options.termination.max_iter == 0 and options.sampling.do_sampling) {
    options.termination.max_iter = 1000;
    cout << "Note: did not specify the number of iterations to perform "
            "(--maxiter)." << endl << "We will now do "
         << options.termination.max_iter << " iterations." << endl;
  }

  bool any_named = false;
  for (auto &x : options.paths)
    if (not x.motifs.empty()) {
      any_named = true;
      break;
    }
  if (not any_named)
    for (auto &x : options.paths)
      for (auto &s : options.motif_specifications)
        x.motifs.insert(s.name);

  if (options.load_paths.empty() and options.motif_specifications.empty()) {
    cout << "Error: you must either specify at least one of:" << endl
         << "1. a path from which to load HMM parameter (--load)" << endl
         << "2. one or more motifs (--motif)" << endl
         << default_error_msg << endl;
    return EXIT_FAILURE;
  }

  if (not options.long_names) {
    if (not options.seeding.only_best) {
      cout << "Warning: you did not specify --best seed selection, but did not "
              "specify --longnames." << endl << "Adding option --longnames."
           << endl;
      options.long_names = true;
    } else if (options.wiggle > 0) {
      cout << "Warning: you specified wiggle variants, but did not specify "
              "--longnames." << endl << "Adding option --longnames." << endl;
      options.long_names = true;
    }
  }

  // Ensure that the residual MI ratio cutoff is non-negative
  if (options.multi_motif.residual_ratio < 0) {
    cout << "Warning: negative value provided for residual mutual information "
            "ratio cutoff. Using 0 as value." << endl;
    options.multi_motif.residual_ratio = 0;
  }

  // Ensure that multiple mode is only used with objective function MICO
  if (options.multi_motif.accept_multiple) {
    for (auto &obj : options.objectives)
      if (obj.measure != Measures::Continuous::Measure::MutualInformation) {
        cout << "Error: multiple motif mode can only be used with the "
                "objective function MICO." << endl;
        return EXIT_FAILURE;
      }
  }

  if (options.line_search.eta <= options.line_search.mu) {
    cout << "Error: the Moré-Thuente η parameter must be larger than the µ "
            "parameter." << endl;
    return EXIT_FAILURE;
  }

  // initialize RNG
  if (options.verbosity >= Verbosity::info)
    cout << "Initializing random number generator with salt "
         << options.random_salt << "." << endl;
  mt19937 rng;
  rng.seed(options.random_salt);

  Fasta::EntropySource::seed(RandomDistribution::Uniform(rng));
  MCMC::EntropySource::seed(RandomDistribution::Uniform(rng));

  // main routine
  try {
    perform_analysis(options, rng);
  } catch (exception &e) {
    cout << e.what() << endl;
    return EXIT_FAILURE;
  }

  if (options.verbosity >= Verbosity::info) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
      cout << "getrusage failed" << endl;
      return EXIT_FAILURE;
    }

    double utime = usage.ru_utime.tv_sec + 1e-6 * usage.ru_utime.tv_usec;
    double stime = usage.ru_stime.tv_sec + 1e-6 * usage.ru_stime.tv_usec;
    double total_time = utime + stime;
    double elapsed_time = timer.tock() * 1e-6;

    cerr << "User time = " << utime << " sec" << endl
         << "System time = " << stime << " sec" << endl
         << "CPU time = " << total_time << " sec" << endl
         << "Elapsed time = " << elapsed_time << " sec" << endl
         << 100 * total_time / elapsed_time << "\% CPU" << endl;
  }

  return EXIT_SUCCESS;
}
