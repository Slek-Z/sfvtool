// SFV data structures and useful functions
// Copyright (c) 2019 Slek

#ifndef SFV_HPP_
#define SFV_HPP_

#include <cstdint>
#include <string>
#include <time.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <ios>
#include <ostream>
#include <regex>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/crc.hpp>
#include <boost/filesystem.hpp>

namespace sfv {

const char COMMENT = ';';
const char SEPARATOR = ' ';
const std::string LINE_SEPARATOR = "\r\n";

const std::string HEADER = "Generated by SFVTool v1.0 (github.com/slek-z/sfvtool)";

const std::streamsize BUFFER_SIZE = 4096;

class not_found : public std::exception {
public:

  not_found()
    : message_("sfv::not_found") { }

  not_found(const char* message)
    : message_(message) { }

  inline const char * what () const noexcept {
    return message_.c_str();
  }

private:

  std::string message_;
};

class io_error : public std::exception {
public:

  io_error()
    : message_("sfv::io_error") { }

  io_error(const char* message)
    : message_(message) { }

  inline const char * what () const noexcept {
    return message_.c_str();
  }

private:

  std::string message_;
};

struct summary {
  std::uint32_t duplicated_files;

  summary()
    : duplicated_files(0) { }
};

struct check_summary {
  std::uint32_t format_errors, read_errors, check_errors;

  check_summary()
    : format_errors(0), read_errors(0), check_errors(0) { }
};

inline bool getline(std::ifstream& input, std::string& output, const std::string& delim) {
  output.clear();

  if (!input.good()) return false;
  if (delim.empty()) return false;

  std::string buffer;
  buffer.reserve(delim.size());

  std::ifstream::char_type ch;
  std::string::size_type idx = 0;
  while (idx < delim.size()) {
    if ((ch = input.get()) == std::ifstream::traits_type::eof()) {
      output.append(buffer);
      return true;
    }
    if (delim.at(idx) == ch) {
      buffer.push_back(ch);
      idx++;
    } else {
      if (!buffer.empty()) {
        output.append(buffer);
        buffer.clear();
      }
      output.push_back(ch);
    }
  }

  return true;
}

inline std::string getTimestamp() {
  std::time_t now = std::time(NULL);
  std::tm * ptm = std::localtime(&now);
  char buffer[32];
  // Format: on 2009-06-13 at 20:20:00
  std::strftime(buffer, 32, "on %Y-%m-%d at %H:%M:%S", ptm);
  return buffer;
}

inline std::uint32_t computeCRC32(const std::string& file) {
  boost::crc_32_type result;
  std::ifstream ifs(file, std::ios_base::in | std::ios_base::binary);
  if (ifs.is_open()) {
    char buffer[BUFFER_SIZE];
    do {
      ifs.read(buffer, BUFFER_SIZE);
      result.process_bytes(buffer, ifs.gcount());
    } while (ifs.good());
  } else
    throw not_found("file not found");

  if (ifs.fail() && !ifs.eof())
    throw io_error("couldn't read file");

  return result.checksum();
}

class SFVData {
public:

  explicit SFVData(const std::string& file) {
    boost::filesystem::path filepath(file);
    if (!boost::filesystem::is_regular_file(filepath))
      throw not_found("file not found");

    name_ = filepath.filename().string();
    size_ = boost::filesystem::file_size(filepath);
    last_write_ = boost::filesystem::last_write_time(filepath);
    crc32_ = computeCRC32(file);
  }

  inline std::string getName() const { return name_; }

  inline std::uint32_t getChecksum() const { return crc32_; }
  inline std::string getFormattedChecksum() const {
    char buffer[9];
    std::snprintf(buffer, 9, "%08X", crc32_);
    return std::string(buffer, 8);
  }

  inline std::uint64_t getSize() const { return size_; }
  inline std::string getFormattedSize() const { return std::to_string(size_); }

  inline std::time_t getTimestamp() const { return last_write_; }
  inline std::string getFormattedTimestamp() const {
    std::tm * ptm = std::localtime(&last_write_);
    char buffer[20];
    // Format: 20:20.00 2009-06-15
    std::strftime(buffer, 20, "%H:%M.%S %Y-%m-%d", ptm);
    return std::string(buffer, 19);
  }

  inline bool operator <(const SFVData& lhs) const {
    return (name_.compare(lhs.name_) < 0);
  }

private:

  std::string name_;
  std::uint64_t size_;
  std::time_t last_write_;
  std::uint32_t crc32_;
};

class SFVFile {
public:

  explicit SFVFile(const std::string& file) {
    std::string regex_def = std::string("^([0-9a-fA-F]{8})");
    regex_def += SEPARATOR;
    regex_def += std::string("(.+$)");

    rev_sfv_regex_ = std::regex(regex_def, std::regex::extended);

    if (!boost::filesystem::is_regular_file(file))
      throw std::runtime_error("file not found");

    std::string line;
    line.reserve(256);

    std::uint32_t ctr = 0;
    std::ifstream input(file);
    while (getline(input, line, LINE_SEPARATOR)) {
      ctr++;
      if (line.empty()) continue;
      if (line.front() == ';') continue;

      std::reverse(line.begin(), line.end());
      std::smatch matches;
      if (!std::regex_match(line, matches, rev_sfv_regex_) ||
          matches.size() != 3) {
        ignored_lines_.push_back(ctr);
        continue;
      }

      std::string checksum = matches[1].str();
      std::reverse(checksum.begin(), checksum.end());

      std::string filename = matches[2].str();
      std::reverse(filename.begin(), filename.end());

      try {
        data_.emplace_back(std::make_pair(filename, static_cast<std::uint32_t>(std::stoul(checksum, nullptr, 16))));
      } catch (const std::invalid_argument&) {
        ignored_lines_.push_back(ctr);
        continue;
      }
    }

    if (input.fail() && !input.eof())
      throw std::runtime_error("couldn't read file");
  }

  inline const std::vector<std::uint32_t>& getIgnoredLines() const { return ignored_lines_; }

  inline const std::vector<std::pair<std::string, std::uint32_t>>& getData() const {
    return data_;
  }

private:

  std::regex rev_sfv_regex_;
  std::vector<std::uint32_t> ignored_lines_;
  std::vector<std::pair<std::string, std::uint32_t>> data_;

};

} // namesapce sfv

inline std::ostream& operator <<(std::ostream& stream, const sfv::summary& summary) {
  if (summary.duplicated_files > 1)
    stream << "WARNING: " << summary.duplicated_files << " files ignored" << std::endl;
  else if (summary.duplicated_files == 1)
    stream << "WARNING: 1 file ignored" << std::endl;

  return stream;
}

inline std::ostream& operator <<(std::ostream& stream, const sfv::check_summary& summary) {
  if (summary.format_errors > 1)
    stream << "WARNING: " << summary.format_errors << " lines are improperly formatted" << std::endl;
  else if (summary.format_errors == 1)
    stream << "WARNING: 1 line is improperly formatted" << std::endl;

  if (summary.read_errors > 1)
    stream << "WARNING: " << summary.read_errors << " listed files could not be read" << std::endl;
  else if (summary.read_errors == 1)
    stream << "WARNING: 1 listed file could not be read" << std::endl;

  if (summary.check_errors > 1)
    stream << "WARNING: " << summary.check_errors << " computed checksums did NOT match" << std::endl;
  else if (summary.check_errors == 1)
    stream << "WARNING: 1 computed checksum did NOT match" << std::endl;

  return stream;
}

#endif //SFV_HPP_