/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <map>
#include <cctype>

#include <cutils/properties.h>

#include "perfprofdcore.h"
#include "perfprofdutils.h"
#include "perf_data_converter.h"

//
// Perf profiling daemon -- collects system-wide profiles using
//
//       /system/bin/perf record -a
//
// and encodes them so that they can be uploaded by a separate service.
//

//......................................................................

//
// Output file from 'perf record'. The linux 'perf' tool by default
// creates a file with this name.
//
#define PERF_OUTPUT "perf.data"

//
// This enum holds the results of the "should we profile" configuration check.
//
typedef enum {

  // All systems go for profile collection.
  DO_COLLECT_PROFILE,

  // The destination directory selected in the conf file doesn't exist. Most
  // likely this is due to a missing or out-of-date version of the uploading
  // service in GMS core.
  DONT_PROFILE_MISSING_DESTINATION_DIR,

  // Destination directory does not contain the semaphore file that
  // the perf profile uploading service creates when it determines
  // that the user has opted "in" for usage data collection. No
  // semaphore -> no user approval -> no profiling.
  DONT_PROFILE_MISSING_SEMAPHORE,

  // No perf executable present
  DONT_PROFILE_MISSING_PERF_EXECUTABLE,

  // We're running in the emulator, perf won't be able to do much
  DONT_PROFILE_RUNNING_IN_EMULATOR

} CKPROFILE_RESULT;

//
// Are we running in the emulator? If so, stub out profile collection
// Starts as uninitialized (-1), then set to 1 or 0 at init time.
//
static int running_in_emulator = -1;

//
// Is this a debug build ('userdebug' or 'eng')?
// Starts as uninitialized (-1), then set to 1 or 0 at init time.
//
static int is_debug_build = -1;

//
// Random number generator seed (set at startup time).
//
static unsigned short random_seed[3];

//
// Config file path. May be overridden with -c command line option
//
static const char *config_file_path = "/system/etc/perfprofd.conf";

//
// Set by SIGHUP signal handler
//
volatile unsigned please_reread_config_file = 0;

//
// This table describes the config file syntax in terms of key/value pairs.
// Values come in two flavors: strings, or unsigned integers. In the latter
// case the reader sets allowable minimum/maximum for the setting.
//
class ConfigReader {

 public:
  ConfigReader();
  ~ConfigReader();

  // Ask for the current setting of a config item
  unsigned getUnsignedValue(const char *key);
  std::string getStringValue(const char *key);

  // read the specified config file, applying any settings it contains
  void readFile(const char *configFilePath);

 private:
  void addUnsignedEntry(const char *key,
                        unsigned default_value,
                        unsigned min_value,
                        unsigned max_value);
  void addStringEntry(const char *key, const char *default_value);
  void addDefaultEntries();
  void parseLine(const char *key, const char *value, unsigned linecount);

  typedef struct { unsigned minv, maxv; } values;
  std::map<std::string, values> u_info;
  std::map<std::string, unsigned> u_entries;
  std::map<std::string, std::string> s_entries;
  bool trace_config_read;
};

ConfigReader::ConfigReader()
    : trace_config_read(false)
{
  addDefaultEntries();
}

ConfigReader::~ConfigReader()
{
}

//
// Populate the reader with the set of allowable entries
//
void ConfigReader::addDefaultEntries()
{
  // Average number of seconds between perf profile collections (if
  // set to 100, then over time we want to see a perf profile
  // collected every 100 seconds). The actual time within the interval
  // for the collection is chosen randomly.
  addUnsignedEntry("collection_interval", 901, 100, UINT32_MAX);

  // Use the specified fixed seed for random number generation (unit
  // testing)
  addUnsignedEntry("use_fixed_seed", 0, 0, UINT32_MAX);

  // For testing purposes, number of times to iterate through main
  // loop.  Value of zero indicates that we should loop forever.
  addUnsignedEntry("main_loop_iterations", 0, 0, UINT32_MAX);

  // Destination directory (where to write profiles). This location
  // chosen since it is accessible to the uploader service.
  addStringEntry("destination_directory",
                 "/data/data/com.google.android.gms/files");

  // Path to 'perf' executable.
  addStringEntry("perf_path", "/system/bin/perf");

  // Desired sampling period (passed to perf -c option). Small
  // sampling periods can perturb the collected profiles, so enforce
  // min/max.
  addUnsignedEntry("sampling_period", 500000, 5000, UINT32_MAX);

  // Length of time to collect samples (number of seconds for 'perf
  // record -a' run).
  addUnsignedEntry("sample_duration", 3, 2, 600);

  // If this parameter is non-zero it will cause perfprofd to
  // exit immediately if the build type is not userdebug or eng.
  // Currently defaults to 1 (true).
  addUnsignedEntry("only_debug_build", 1, 0, 1);

  // If set to 1, pass the -g option when invoking perf (requests
  // stack traces as opposed to flat profile).
  addUnsignedEntry("stack_profile", 0, 0, 1);

  // For unit testing only: if set to 1, emit info messages on config
  // file parsing.
  addUnsignedEntry("trace_config_read", 0, 0, 1);

  // For unit testing only: avoid deleting existing perf.data file
  // prior to invoking 'perf'
  addUnsignedEntry("noclean", 0, 0, 1);
}

void ConfigReader::addUnsignedEntry(const char *key,
                                    unsigned default_value,
                                    unsigned min_value,
                                    unsigned max_value)
{
  std::string ks(key);
  if (u_entries.find(ks) != u_entries.end() ||
      s_entries.find(ks) != s_entries.end()) {
    W_ALOGE("internal error -- duplicate entry for key %s", key);
    exit(9);
  }
  values vals;
  vals.minv = min_value;
  vals.maxv = max_value;
  u_info[ks] = vals;
  u_entries[ks] = default_value;
}

void ConfigReader::addStringEntry(const char *key, const char *default_value)
{
  std::string ks(key);
  if (u_entries.find(ks) != u_entries.end() ||
      s_entries.find(ks) != s_entries.end()) {
    W_ALOGE("internal error -- duplicate entry for key %s", key);
    exit(9);
  }
  if (! default_value) {
    W_ALOGE("internal error -- bad default value for key %s", key);
    exit(9);
  }
  s_entries[ks] = std::string(default_value);
}

unsigned ConfigReader::getUnsignedValue(const char *key)
{
  std::string ks(key);
  auto it = u_entries.find(ks);
  assert(it != u_entries.end());
  return it->second;
}

std::string ConfigReader::getStringValue(const char *key)
{
  std::string ks(key);
  auto it = s_entries.find(ks);
  assert(it != s_entries.end());
  return it->second;
}

//
// Parse a key=value pair read from the config file. This will issue
// warnings or errors to the system logs if the line can't be
// interpreted properly.
//
void ConfigReader::parseLine(const char *key,
                             const char *value,
                             unsigned linecount)
{
  assert(key);
  assert(value);

  auto uit = u_entries.find(key);
  if (uit != u_entries.end()) {
    unsigned uvalue = 0;
    if (! isdigit(value[0]) || sscanf(value, "%u", &uvalue) != 1) {
      W_ALOGW("line %d: malformed unsigned value (ignored)", linecount);
    } else {
      values vals;
      auto iit = u_info.find(key);
      assert(iit != u_info.end());
      vals = iit->second;
      if (uvalue < vals.minv || uvalue > vals.maxv) {
        W_ALOGW("line %d: specified value %u for '%s' "
                "outside permitted range [%u %u] (ignored)",
                linecount, uvalue, key, vals.minv, vals.maxv);
      } else {
        if (trace_config_read) {
          W_ALOGI("option %s set to %u", key, uvalue);
        }
        uit->second = uvalue;
      }
    }
    trace_config_read = (getUnsignedValue("trace_config_read") != 0);
    return;
  }

  auto sit = s_entries.find(key);
  if (sit != s_entries.end()) {
    if (trace_config_read) {
      W_ALOGI("option %s set to %s", key, value);
    }
    sit->second = std::string(value);
    return;
  }

  W_ALOGW("line %d: unknown option '%s' ignored", linecount, key);
}

static bool isblank(const std::string &line)
{
  for (std::string::const_iterator it = line.begin(); it != line.end(); ++it)
  {
    if (! isspace(*it)) {
      return false;
    }
  }
  return true;
}

void ConfigReader::readFile(const char *configFilePath)
{
  FILE *fp = fopen(configFilePath, "r");
  if (!fp) {
    W_ALOGE("unable to open configuration file %s", config_file_path);
    return;
  }

  char *linebuf = NULL;
  size_t line_length = 0;
  for (unsigned linecount = 1;
       getline(&linebuf, &line_length, fp) != -1;
       ++linecount) {
    char *eq = 0;
    char *key, *value;

    // comment line?
    if (linebuf[0] == '#') {
      continue;
    }

    // blank line?
    if (isblank(linebuf)) {
      continue;
    }

    // look for X=Y assignment
    eq = strchr(linebuf, '=');
    if (!eq) {
      W_ALOGW("line %d: line malformed (no '=' found)", linecount);
      continue;
    }

    *eq = '\0';
    key = linebuf;
    value = eq+1;
    char *ln = strrchr(value, '\n');
    if (ln) { *ln = '\0'; }

    parseLine(key, value, linecount);
  }
  free(linebuf);
  fclose(fp);
}

//
// Parse command line args. Currently you can supply "-c P" to set
// the path of the config file to P.
//
static void parse_args(int argc, char** argv)
{
  int ac;

  for (ac = 1; ac < argc; ++ac) {
    if (!strcmp(argv[ac], "-c")) {
      if (ac >= argc-1) {
        W_ALOGE("malformed command line: -c option requires argument)");
        continue;
      }
      config_file_path = strdup(argv[ac+1]);
      W_ALOGI("config file path set to %s", config_file_path);
      ++ac;
    } else {
      W_ALOGE("malformed command line: unknown option or arg %s)", argv[ac]);
      continue;
    }
  }
}

//
// Convert a CKPROFILE_RESULT to a string
//
const char *ckprofile_result_to_string(CKPROFILE_RESULT result)
{
  switch(result) {
    case DO_COLLECT_PROFILE:
      return "DO_COLLECT_PROFILE";
    case DONT_PROFILE_MISSING_DESTINATION_DIR:
      return "missing destination directory";
    case DONT_PROFILE_MISSING_SEMAPHORE:
      return "missing semaphore file";
    case DONT_PROFILE_MISSING_PERF_EXECUTABLE:
      return "missing 'perf' executable";
    case DONT_PROFILE_RUNNING_IN_EMULATOR:
      return "running in emulator";
    default: return "unknown";
  }
  return "notreached";
}

//
// Convert a PROFILE_RESULT to a string
//
const char *profile_result_to_string(PROFILE_RESULT result)
{
  switch(result) {
    case OK_PROFILE_COLLECTION:
      return "profile collection succeeded";
    case ERR_FORK_FAILED:
      return "fork() system call failed";
    case ERR_PERF_RECORD_FAILED:
      return "perf record returned bad exit status";
    case ERR_PERF_ENCODE_FAILED:
      return "failure encoding perf.data to protobuf";
    case ERR_OPEN_ENCODED_FILE_FAILED:
      return "failed to open encoded perf file";
    case ERR_WRITE_ENCODED_FILE_FAILED:
      return "write to encoded perf file failed";
    default: return "unknown";
  }
  return "notreached";
}

//
// The daemon does a read of the main config file on startup, however
// if the destination directory also contains a configf file, then we
// read parameters from that as well. This provides a mechanism for
// changing/controlling the behavior of the daemon via the settings
// established in the uploader service (which may be easier to update
// than the daemon).
//
static void read_aux_config(ConfigReader &config)
{
  std::string destConfig(config.getStringValue("destination_directory"));
  destConfig += "/perfprofd.conf";
  FILE *fp = fopen(destConfig.c_str(), "r");
  if (fp) {
    fclose(fp);
    bool trace_config_read =
        (config.getUnsignedValue("trace_config_read") != 0);
    if (trace_config_read) {
      W_ALOGI("reading auxiliary config file %s", destConfig.c_str());
    }
    config.readFile(destConfig.c_str());
  }
}

//
// Check to see whether we should perform a profile collection
//
static CKPROFILE_RESULT check_profiling_enabled(ConfigReader &config)
{
  //
  // Profile collection in the emulator doesn't make sense
  //
  assert(running_in_emulator != -1);
  if (running_in_emulator) {
    return DONT_PROFILE_RUNNING_IN_EMULATOR;
  }

  //
  // Check for the existence of the destination directory
  //
  std::string destdir = config.getStringValue("destination_directory");
  DIR* dir = opendir(destdir.c_str());
  if (!dir) {
    W_ALOGW("unable to open destination directory %s: (%s)",
            destdir.c_str(), strerror(errno));
    return DONT_PROFILE_MISSING_DESTINATION_DIR;
  }

  // Reread aux config file -- it may have changed
  read_aux_config(config);

  // Check for existence of perf executable
  std::string pp = config.getStringValue("perf_path");
  FILE *pfp = fopen(pp.c_str(), "re");
  if (!pfp) {
    W_ALOGW("unable to open %s", pp.c_str());
    closedir(dir);
    return DONT_PROFILE_MISSING_PERF_EXECUTABLE;
  }
  fclose(pfp);

  // Check for existence of semaphore file
  unsigned found = 0;
  struct dirent* e;
  while ((e = readdir(dir)) != 0) {
    if (!strcmp(e->d_name, SEMAPHORE_FILENAME)) {
      found = 1;
      break;
    }
  }
  closedir(dir);
  if (!found) {
    return DONT_PROFILE_MISSING_SEMAPHORE;
  }

  //
  // We are good to go
  //
  return DO_COLLECT_PROFILE;
}

inline char* string_as_array(std::string* str) {
  return str->empty() ? NULL : &*str->begin();
}

PROFILE_RESULT encode_to_proto(const std::string &data_file_path,
                               const std::string &encoded_file_path)
{
  //
  // Open and read perf.data file
  //
  const wireless_android_play_playlog::AndroidPerfProfile &encodedProfile =
      wireless_android_logging_awp::RawPerfDataToAndroidPerfProfile(data_file_path);

  fprintf(stderr, "data file path is %s\n", data_file_path.c_str());
  fprintf(stderr, "encoded file path is %s\n", encoded_file_path.c_str());

  //
  // Issue error if no samples
  //
  if (encodedProfile.programs().size() == 0) {
    return ERR_PERF_ENCODE_FAILED;
  }


  //
  // Serialize protobuf to array
  //
  int size = encodedProfile.ByteSize();
  std::string data;
  data.resize(size);
  ::google::protobuf::uint8* dtarget =
        reinterpret_cast<::google::protobuf::uint8*>(string_as_array(&data));
  encodedProfile.SerializeWithCachedSizesToArray(dtarget);

  //
  // Open file and write encoded data to it
  //
  FILE *fp = fopen(encoded_file_path.c_str(), "w");
  if (!fp) {
    return ERR_OPEN_ENCODED_FILE_FAILED;
  }
  size_t fsiz = size;
  if (fwrite(dtarget, fsiz, 1, fp) != 1) {
    fclose(fp);
    return ERR_WRITE_ENCODED_FILE_FAILED;
  }
  fclose(fp);

  return OK_PROFILE_COLLECTION;
}

//
// Collect a perf profile. Steps for this operation are:
// - kick off 'perf record'
// - read perf.data, convert to protocol buf
//
static PROFILE_RESULT collect_profile(ConfigReader &config)
{
  //
  // Form perf.data file name, perf error output file name
  //
  std::string destdir = config.getStringValue("destination_directory");
  std::string data_file_path(destdir);
  data_file_path += "/";
  data_file_path += PERF_OUTPUT;
  std::string perf_stderr_path(destdir);
  perf_stderr_path += "/perferr.txt";

  //
  // Remove any existing perf.data file -- if we don't do this, perf
  // will rename the old file and we'll have extra cruft lying around.
  // NB: allow this to be turned off for unit testing purposes, so
  // that we can use canned perf.data files.
  //
  unsigned noclean = config.getUnsignedValue("noclean");
  if (!noclean) {
    unlink(data_file_path.c_str());
  }

  //
  // NB: it would probably be better to use explicit fork/exec with an
  // alarm timeout in case of funny business. For now, call system().
  //
  std::string perf_path = config.getStringValue("perf_path");
  unsigned duration = config.getUnsignedValue("sample_duration");
  unsigned period = config.getUnsignedValue("sampling_period");
  const char *gopt = (config.getUnsignedValue("stack_profile") != 0 ? "-g" : "");
  char cmd[8192];
  snprintf(cmd, 8192, "%s record %s -c %u -o %s -a -- sleep %d 1> %s 2>&1 ",
           perf_path.c_str(), gopt, period, data_file_path.c_str(), duration,
           perf_stderr_path.c_str());
  int rc = system(cmd);
  if (rc == -1) {
    return ERR_FORK_FAILED;
  }
  if (rc != 0) {
    return ERR_PERF_RECORD_FAILED;
  }

  //
  // Read the resulting perf.data file, encode into protocol buffer, then write
  // the result to a *.pdb file
  //
  std::string encoded_file_path(data_file_path);
  encoded_file_path += ".encoded";
  return encode_to_proto(data_file_path, encoded_file_path);
}

//
// SIGHUP handler. Sets a flag to indicate that we should reread the
// config file
//
static void sig_hup(int /* signum */)
{
  please_reread_config_file = 1;
}

//
// Assuming that we want to collect a profile every N seconds,
// randomly partition N into two sub-intervals.
//
static void determine_before_after(unsigned &sleep_before_collect,
                                   unsigned &sleep_after_collect,
                                   unsigned collection_interval)
{
  double frac = erand48(random_seed);
  sleep_before_collect = (unsigned) (((double)collection_interval) * frac);
  assert(sleep_before_collect <= collection_interval);
  sleep_after_collect = collection_interval - sleep_before_collect;
}

//
// Set random number generator seed
//
static void set_seed(ConfigReader &config)
{
  unsigned seed = 0;
  unsigned use_fixed_seed = config.getUnsignedValue("use_fixed_seed");
  if (use_fixed_seed) {
    //
    // Use fixed user-specified seed
    //
    seed = use_fixed_seed;
  } else {
    //
    // Randomized seed
    //
    seed = arc4random();
  }
  W_ALOGI("random seed set to %u", seed);
  random_seed[0] = seed & 0xffff;
  random_seed[1] = (seed >> 16);
  random_seed[2] = (random_seed[0] ^ random_seed[1]);
}

//
// Initialization
//
static void init(ConfigReader &config)
{
  config.readFile(config_file_path);
  set_seed(config);

  char propBuf[PROPERTY_VALUE_MAX];
  propBuf[0] = '\0';
  property_get("ro.kernel.qemu", propBuf, "");
  running_in_emulator = (propBuf[0] == '1');
  property_get("ro.debuggable", propBuf, "");
  is_debug_build = (propBuf[0] == '1');

  signal(SIGHUP, sig_hup);
}

//
// Main routine:
// 1. parse cmd line args
// 2. read config file
// 3. loop: {
//       sleep for a while
//       perform a profile collection
//    }
//
int perfprofd_main(int argc, char** argv)
{
  ConfigReader config;

  W_ALOGI("starting Android Wide Profiling daemon");

  parse_args(argc, argv);
  init(config);
  read_aux_config(config);

  // Early exit if we're not supposed to run on this build flavor
  if (is_debug_build != 1 &&
      config.getUnsignedValue("only_debug_build") == 1) {
    W_ALOGI("early exit due to inappropriate build type");
    return 0;
  }

  unsigned iterations = 0;
  while(!config.getUnsignedValue("main_loop_iterations") ||
        iterations < config.getUnsignedValue("main_loop_iterations")) {

    // Figure out where in the collection interval we're going to actually
    // run perf
    unsigned sleep_before_collect = 0;
    unsigned sleep_after_collect = 0;
    determine_before_after(sleep_before_collect, sleep_after_collect,
                           config.getUnsignedValue("collection_interval"));
    perfprofd_sleep(sleep_before_collect);

    // Reread config file if someone sent a SIGHUP
    if (please_reread_config_file) {
      config.readFile(config_file_path);
      please_reread_config_file = 0;
    }

    // Check for profiling enabled...
    CKPROFILE_RESULT ckresult = check_profiling_enabled(config);
    if (ckresult != DO_COLLECT_PROFILE) {
      W_ALOGI("profile collection skipped (%s)",
              ckprofile_result_to_string(ckresult));
    } else {
      // Kick off the profiling run...
      W_ALOGI("initiating profile collection");
      PROFILE_RESULT result = collect_profile(config);
      if (result != OK_PROFILE_COLLECTION) {
        W_ALOGI("profile collection failed (%s)",
                profile_result_to_string(result));
      } else {
        W_ALOGI("profile collection complete");
      }
    }
    perfprofd_sleep(sleep_after_collect);
    iterations += 1;
  }

  W_ALOGI("finishing Android Wide Profiling daemon");
  return 0;
}
