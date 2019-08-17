// SFV - Print or check Simple File Verification (SFV) checksums
// Copyright (c) 2019 Slek

#define PROGRAM_NAME "sfvtool"
#define PROGRAM_DESC "Print or check Simple File Verification (SFV) checksums."
#define PROGRAM_VERS "1.0"
#define COPY_INFO "Copyright (c) 2019 Slek"

#define FLAGS_CASES                                                                                \
    FLAG_CASE(check, 'c', "read SFVs from FILEs and check them")                                   \
    FLAG_CASE(ignore_missing, -1, "don't fail or report status for missing files (with check)")    \
    FLAG_CASE(quiet, -1, "don't print OK for each successfully verified file (with check)")        \
    FLAG_CASE(status, -1, "don't output anything, status code shows success (with check)")         \
    FLAG_CASE(strict, -1, "exit non-zero for improperly formatted checksum lines (with check)")    \
    FLAG_CASE(warn, 'w', "warn about improperly formatted checksum lines (with check)")            

#include <ios>
#include <iostream>
#include <ostream>

#include <algorithm>
#include <string>
#include <unordered_set>

#include <boost/filesystem.hpp>

#include "flags.hpp"
#include "sfv.hpp"

int main(int argc, char* argv[]) {

  if (flags::HelpRequired(argc, argv)) {
    flags::ShowHelp();
    return 0; // SUCCESS
  }

  if (flags::VersionRequested(argc, argv)) {
    flags::ShowVersion();
    return 0; // SUCCESS;
  }

  int result = flags::ParseFlags(&argc, &argv, true);
  if (result > 0) {
    std::cerr << "unrecognized option '" << argv[result] << "'" << std::endl;
    std::cerr << "Try '" << PROGRAM_NAME << " --help' for more information" << std::endl;
    return 1; // FAILURE
  }

  if (argc <= 1) {
    flags::ShowHelp();
    return 0; // SUCCESS
  }

  if (FLAGS_check) {
    sfv::check_summary summary;
    for (int i = 1; i < argc; ++i) {
      try {
        sfv::SFVFile sfv(argv[i]);
        summary.format_errors += sfv.getIgnoredLines().size();

        if (FLAGS_warn) {
          for (std::uint32_t line : sfv.getIgnoredLines())
            std::cerr << argv[i] << ':' << line << ": improperly formatted SFV line" << std::endl;
        }

        for (const std::pair<std::string, std::uint32_t>& entry : sfv.getData()) {
          try {
            if (sfv::computeCRC32(entry.first) == entry.second) {
              if (!FLAGS_quiet && !FLAGS_status)
                std::cout << entry.first << ": OK" << std::endl;
            } else {
              summary.check_errors++;
              if (!FLAGS_status)
                std::cout << entry.first << ": FAILED" << std::endl;
            }
          } catch (const sfv::not_found& e) {
            if (!FLAGS_ignore_missing) {
              summary.read_errors++;
              if (!FLAGS_status) {
                //std::cerr << entry.first << ": " << e.what() << std::endl;
                std::cout << entry.first << ": FAILED open or read" << std::endl;
              }
            }
          } catch (const sfv::io_error& e) {
            summary.read_errors++;
            if (!FLAGS_status) {
              //std::cerr << entry.first << ": " << e.what() << std::endl;
              std::cout << entry.first << ": FAILED open or read" << std::endl;
            }
          }
        }
      } catch (const std::runtime_error& e) {
        std::cerr << argv[i] << ": " << e.what() << std::endl;
      } catch(...) {
        std::cerr << argv[i] << ": unexpected error" << std::endl;
      }
    }

    if (!FLAGS_status)
      std::cerr << summary;

    if (FLAGS_status && (summary.check_errors > 0))
      return 1; // FAILURE

    if (FLAGS_status && !FLAGS_ignore_missing && (summary.read_errors > 0))
      return 1; // FAILURE

    if (FLAGS_strict && (summary.format_errors > 0))
      return 1; // FAILURE

    return 0; // SUCCESS;
  } else {
    sfv::summary summary;
    std::vector<sfv::SFVData> data;
    data.reserve(argc);

    std::unordered_set<std::string> files;
    for (int i = 1; i < argc; ++i) {
      try {
        boost::filesystem::path filepath(argv[i]);
        if (!boost::filesystem::is_regular_file(filepath)) {
          std::cerr << argv[i] << ": file not found" << std::endl;
          continue;
        }

        std::string filename = filepath.filename().string();
        if (files.find(filename) != files.cend()) {
          summary.duplicated_files++;
          std::cerr << argv[i] << ": filename already exists" << std::endl;
          continue;
        }
        files.insert(filename);

        data.emplace_back(sfv::SFVData(argv[i]));
      } catch (const sfv::not_found& e) {
        std::cerr << argv[i] << ": " << e.what() << std::endl;
      } catch (const sfv::io_error& e) {
        std::cerr << argv[i] << ": " << e.what() << std::endl;
      } catch(...) {
        std::cerr << argv[i] << ": unexpected error" << std::endl;
      }
    }

    if (data.empty()) return 0;

    std::sort(data.begin(), data.end());

    std::cout << sfv::COMMENT << ' ' << sfv::HEADER
              << ' ' << sfv::getTimestamp() << sfv::LINE_SEPARATOR
              << sfv::COMMENT << sfv::LINE_SEPARATOR;

    unsigned int n_max_size = 0;
    for (const sfv::SFVData& sfv_entry : data) {
      unsigned int n = sfv_entry.getFormattedSize().size();
      if (n_max_size < n)
        n_max_size = n;
    }

    for (const sfv::SFVData& sfv_entry : data) {
      std::string num_bytes = sfv_entry.getFormattedSize();

      std::cout << sfv::COMMENT << ' '
                << std::string(n_max_size - num_bytes.size(), ' ') << num_bytes << ' '
                << sfv_entry.getFormattedTimestamp() << ' '
                << sfv_entry.getName() << sfv::LINE_SEPARATOR;
    }

    std::cout << sfv::COMMENT << sfv::LINE_SEPARATOR;

    for (const sfv::SFVData& sfv_entry : data) {
      std::cout << sfv_entry.getName() << sfv::SEPARATOR
                << sfv_entry.getFormattedChecksum() << sfv::LINE_SEPARATOR;
    }

    std::cerr << summary;

    return 0; // SUCCESS;
  }
}