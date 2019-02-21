/*
 * datastat - Easy command-line data statistics
 *
 * Author: Tommaso Cucinotta
 * Copyright 2011-2019
 *
 * Quartiles patch contributed by Alan Barton
 * 
 * License: GPLv3, see LICENSE.txt file for details
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <algorithm> // for sort
#include <vector>
#include <map>
#include <set>
#include <string>

using namespace std;

FILE *fin = stdin;
const char *use_delim = " ,\t";
std::set<int> key_fields;
bool show_avg = true;
bool show_dev = false;
bool show_1qt = false;
bool show_2qt = false;
bool show_3qt = false;
bool show_min = false;
bool show_max = false;
bool show_cnt = false;
bool show_sum = false;
bool show_header = true;
bool use_nan = false;
const char *use_sep = " ";

#if LOG_DEBUG
#define log_noln(fmt, args...) do {	\
    printf(fmt, ##args);	\
  } while (0)
#else
#define log_noln(fmt, args...) do {  } while(0)
#endif

#define log(fmt, args...) do {	\
    log_noln(fmt "\n", ##args);	\
  } while (0)

#define chk_exit(cond,msg, args...) do {			\
    if (!(cond)) {						\
      fprintf(stderr, "Error: " msg "\n", ##args);		\
      exit(-1);							\
    }								\
  } while (0)

// f goes from zero
static bool is_key_field(int f) {
  return key_fields.find(f) != key_fields.end();
}

static void log_values(vector<double> const & values) {
  vector<double>::const_iterator vit;
  log_noln("[");
  for (vit = values.begin(); vit != values.end(); ++vit) {
    if (vit - values.begin() > 0)
      log_noln(", ");
    log_noln("%g", *vit);
  }
  log("]");
}

struct record {
  vector<double> v_sum;
  vector<double> v_sqr;
  vector<double> v_min;
  vector<double> v_max;
  vector< vector<double> > v_val; // TODO
  unsigned long num;
  record() : v_sum(), v_sqr(), v_min(), v_max(), v_val(), num(0) {  }
};

static void parse_fields(std::set<int> &fields, char *s) {
  char *tok = strtok(s, ",");
  chk_exit(tok != 0, "Wrong syntax for fields argument");
  while (tok != NULL) {
    int i1, i2;
    if (sscanf(tok, "%d-%d", &i1, &i2) == 2) {
      chk_exit(i1 >= 1 && i2 >= 1, "-k expects natural integers >= 1");
      for (int i = i1 - 1; i < i2; ++i) {
	fields.insert(i);
      }
    } else if (sscanf(tok, "%d", &i1) == 1) {
      chk_exit(i1 >= 1, "-k expects natural integers >= 1");
      fields.insert(i1 - 1);
    } else {
      chk_exit(false, "Wrong syntax for fields argument");
    }
    tok = strtok(NULL, ",");
  }
}

static void dump_fields(std::set<int> fields) {
  bool need_comma = false;
  for (int i = 0; !fields.empty(); ++i) {
    if (fields.find(i) != fields.end()) {
      if (need_comma)
	log_noln(", %d", i + 1);
      else
	log_noln("%d", i + 1);
      fields.erase(i);
      need_comma = true;
    }
  }
}

void usage() {
  printf("Source available from: git://git.code.sf.net/p/datastat/code\n");
  printf("Usage: datastat [options] [filename]\n");
  printf("  Options:\n");
  printf("    -h|--help ....... This help message\n");
  printf("    -k|--key cols ... Specify key columns ('-k 3' or '-k 3,5' or '-k 3-5,7' all work)\n");
  printf("    -na|--no-avg .... Suppress average\n");
  printf("    -nh|--no-header . Suppress header line\n");
  printf("    --dev ........... Show standard deviation\n");
  printf("    --1qt ........... Show first quartile (include median)\n");
  printf("    --2qt|--med ..... Show second quartile (i.e. median)\n");
  printf("    --3qt ........... Show third quartile (include median)\n");
  printf("    --min ........... Show minimum\n");
  printf("    --max ........... Show maximum\n");
  printf("    --sum ........... Show sum\n");
  printf("    --cnt ........... Show count of values\n");
  printf("    --use-nan ....... Tolerate non-numbers in input (samples IGNORED when computing stats)\n");
  printf("    --sep char ...... Use the specified separator character when formatting output (default ' ')\n");
  printf("    --delim chars ... Use the specified set of delimiters when parsing input (default ' ,\\t')\n");
}

/* Utility macro */
#define printf_sep(msg, args...) do { 		\
	printf("%s" msg, sep, ##args);		\
	sep = use_sep;				\
      } while (0)

vector<double> slice(const vector<double>& v, int start=0, int end=-1) {
   int oldlen = v.size();
   int newlen;
   if (end == -1 || end >=  oldlen || end < start) {
      newlen = oldlen - start;
   } else {
      newlen = end - start;
   }
   vector<double> nv(newlen);
   for (int i = 0; i < newlen; ++i) {
      nv[i] = v[start + i];
   }
   return nv;
}

/**
 ** See http://en.wikipedia.org/wiki/Quartile, in particular method 2.
 **   Use the median to divide the ordered data set into two
 **   halves. If the median is a datum (as opposed to being the mean
 **   of the middle two data), include the median in both halves.  The
 **   lower quartile value is the median of the lower half of the
 **   data. The upper quartile value is the median of the upper half
 **   of the data.
 **/
bool calculateMedian(vector<double> vals, double* median, int* medianPosLow, int* medianPosHigh) {
   bool isEvenNumberOfDataPoints = false;
   size_t size = vals.size();
   sort(vals.begin(), vals.end());
   if (size %2 == 0) {
      isEvenNumberOfDataPoints = true;
      // even number of data points, so median is average of two middle values.
      // it may be the case that the calculated value is an actual datum in our set
      *medianPosLow  = size/2 - 1;
      *medianPosHigh = size/2;
      *median = (vals[*medianPosLow] + vals[*medianPosHigh]) / 2;
   } else {
      // odd number of data points, so median is an actual datum
      *medianPosLow  = size/2;
      *medianPosHigh = size/2;
      *median = vals[*medianPosLow];
   }
   log("      medianPosLow (before): %d", *medianPosLow);
   log("      medianPosHigh(before): %d", *medianPosHigh);
   for (size_t ii = *medianPosLow - 1; ; ii--) {

      if (vals[ii] == *median) {
         *medianPosLow = ii;
      } else {
         break;
      }
   }
   for (size_t ii = *medianPosLow + 1; ii < size; ++ii) {
      if (vals[ii] == *median) {
         *medianPosHigh = ii;
      } else {
         break;
      }
   }
   log("      medianPosLow (before): %d", *medianPosLow);
   log("      medianPosHigh(before): %d", *medianPosHigh);
   return isEvenNumberOfDataPoints;
}

static double finite_or(double val, double orig1, double orig2) {
  log("finite_or: val=%g, orig1=%g, orig2=%g, isfinite(val)=%d", val, orig1, orig2, isfinite(val));
  if (isfinite(val))
    return val;
  else if (isfinite(orig1))
    return orig1;
  return orig2;
}

// TODO: we should count non-NaN per column, as they could be different!
//       now this is computing average & std-dev assuming number of values == number of rows
static void accumulate_on(record & accum, vector<string> & values) {
  log("      v_sum[] size: %lu", accum.v_sum.size());
  log_noln("      Retrieved values: ");
  log_values(accum.v_sum);
  int non_key_id = 0;
  for (int i = 0; i < (int)values.size(); ++i) {
    if (!is_key_field(i)) {
      const char *s = values[i].c_str();
      double d = NAN;
      chk_exit((sscanf(s, "%lf", &d) == 1 && isfinite(d)) || use_nan, "Couldn't parse number: %s!", s);
      log("      non_key_id=%d, accum.v_sum.size()=%lu", non_key_id, accum.v_sum.size());
      // d now contains the double value of the read string
      if (accum.num == 0) {
	// BEWARE: we might be pushing NaN here, it's needed to count the correct number of accumulators
	//         if NaNs happen on the first line
	if (show_sum || show_avg || show_dev)
	  accum.v_sum.push_back(d);
	if (show_dev)
	  accum.v_sqr.push_back(d*d);
	if (show_min)
	  accum.v_min.push_back(d);
	if (show_max)
	  accum.v_max.push_back(d);
	// with use_nan, this is needed to correctly count # of non-NaN values in each columns
	if (use_nan || show_1qt || show_2qt || show_3qt) {
	  accum.v_val.push_back(vector<double>());
	  if (isfinite(d)) {
	    log("pushing back: %g", d);
	    accum.v_val.back().push_back(d);
	  }
	}
      } else {
	if (show_sum || show_avg || show_dev) {
	  double v = accum.v_sum[non_key_id] + d;
	  accum.v_sum[non_key_id] = finite_or(v, accum.v_sum[non_key_id], d);
	}
	if (show_dev) {
	  double v = accum.v_sqr[non_key_id] + d*d;
	  accum.v_sqr[non_key_id] = finite_or(v, accum.v_sqr[non_key_id], d*d);
	}
	if (show_min) {
	  double v = min(accum.v_min[non_key_id], d);
	  accum.v_min[non_key_id] = finite_or(v, accum.v_min[non_key_id], d);
	}
	if (show_max) {
	  double v = max(accum.v_max[non_key_id], d);
	  accum.v_max[non_key_id] = finite_or(v, accum.v_max[non_key_id], d);
	}
	if (isfinite(d) && (use_nan || show_1qt || show_2qt || show_3qt))
	  accum.v_val[non_key_id].push_back(d);
      }
      ++non_key_id;
    }
  }
  ++accum.num;

  log_noln("      Finalized values: ");
  log_values(accum.v_sum);
}

static int get_num_cols(record const & r) {
  int sz = 0;
  if (show_sum || show_avg || show_dev)
    sz = r.v_sum.size();
  else if (show_max)
    sz = r.v_max.size();
  else if (show_min)
    sz = r.v_min.size();
  else if (use_nan || show_1qt || show_2qt || show_3qt)
    sz = r.v_val.size();
  return sz;
}

static void show(vector<string> const & key, record const & r) {
  log_noln("   v_sum: ");  log_values(r.v_sum);
  log_noln("   v_sqr: ");  log_values(r.v_sqr);
  log_noln("   v_min: ");  log_values(r.v_min);
  log_noln("   v_max: ");  log_values(r.v_max);
  int key_id = 0;
  int non_key_id = 0;
  const char *sep = "";
  int sz = get_num_cols(r);
  for (int i = 0; i < int(key.size() + sz); ++i) {
    if (is_key_field(i)) {
      printf_sep("%s", key[key_id].c_str());
      ++key_id;
      continue;
    }
    double firstQuantile;
    double median;
    double thirdQuantile;
    unsigned long num = r.num;
    if (use_nan) {
      chk_exit(non_key_id < (int)r.v_val.size(), "Internal error!");
      log("non_key_id=%d, r.v_val.size()=%lu\n", non_key_id, r.v_val.size());
      num = r.v_val[non_key_id].size();
    }
    log("   num: %lu, r.num: %lu", num, r.num);
    if (show_avg) {
      double avg = r.v_sum[non_key_id] / num;
      printf_sep("%g", avg);
    }
    if (show_dev) {
      double avg = r.v_sum[non_key_id] / num;
      printf_sep("%g", sqrt(r.v_sqr[non_key_id] / num - avg*avg));
    }
    if (show_1qt || show_2qt || show_3qt) {
      int medianPosLow;
      int medianPosHigh;
      int firstQuantilePosLow;
      int firstQuantilePosHigh;
      int thirdQuantilePosLow;
      int thirdQuantilePosHigh;
      bool isEvenNumberOfDataPoints;
      vector<double> alldata = slice(r.v_val[non_key_id], 0);
      log_noln("   v_val         : "); log_values(alldata);
      // not sure exactly how to sort without copying data
      sort(alldata.begin(), alldata.end());
      log_noln("   v_val (sorted): "); log_values(alldata);
      log("      size: %ld", alldata.size());

      // 2nd quartile
      isEvenNumberOfDataPoints = calculateMedian(alldata, &median, &medianPosLow, &medianPosHigh);
      if (isEvenNumberOfDataPoints) {
	medianPosHigh--;
	medianPosLow++;
      }
      {// 1st quartile (including median... so use medianPosHigh)
	vector<double> first = slice(alldata, 0, medianPosHigh+1);
	calculateMedian(first, &firstQuantile, &firstQuantilePosLow, &firstQuantilePosHigh);
      }
      if (isEvenNumberOfDataPoints) {
	firstQuantilePosLow--;
	firstQuantilePosHigh++;
      }
      { // 3rd quartile (including median... so use medianPosLow)
	vector<double> third = slice(alldata, medianPosLow, alldata.size());
	calculateMedian(third, &thirdQuantile, &thirdQuantilePosLow, &thirdQuantilePosHigh);
      }
    }
    if (show_1qt) {
      printf_sep("%g", firstQuantile);
    }
    if (show_2qt) {
      printf_sep("%g", median);
    }
    if (show_3qt) {
      printf_sep("%g", thirdQuantile);
    }
    if (show_min) {
      printf_sep("%g", r.v_min[non_key_id]);
    }
    if (show_max) {
      printf_sep("%g", r.v_max[non_key_id]);
    }
    if (show_sum) {
      printf_sep("%g", r.v_sum[non_key_id]);
    }
    if (use_nan && show_cnt) {
      printf_sep("%lu", num);
    }

    ++non_key_id;
  }
  if (!use_nan && show_cnt) {
    printf_sep("%lu", r.num);
  }
  printf("\n");
}

int main(int argc, char *argv[]) {
  --argc;  ++argv;
  while (argc > 0) {
    if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
      usage();
      exit(0);
    } else if (strcmp(*argv, "-k") == 0 || strcmp(*argv, "--key") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option -k requires an argument");
      parse_fields(key_fields, *argv);
      log("Parsed key_fields: ");
      dump_fields(key_fields);
      log_noln("\n");
    } else if (strcmp(*argv, "-na") == 0 || strcmp(*argv, "--no-avg") == 0) {
      show_avg = false;
    } else if (strcmp(*argv, "-nh") == 0 || strcmp(*argv, "--no-header") == 0) {
      show_header = false;
    } else if (strcmp(*argv, "--dev") == 0) {
      show_dev = true;
    } else if (strcmp(*argv, "--1qt") == 0) {
      show_1qt = true;
    } else if (strcmp(*argv, "--2qt") == 0 || strcmp(*argv, "--med") == 0) {
      show_2qt = true;
    } else if (strcmp(*argv, "--3qt") == 0) {
      show_3qt = true;
    } else if (strcmp(*argv, "--min") == 0) {
      show_min = true;
    } else if (strcmp(*argv, "--max") == 0) {
      show_max = true;
    } else if (strcmp(*argv, "--sum") == 0) {
      show_sum = true;
    } else if (strcmp(*argv, "--cnt") == 0) {
      show_cnt = true;
    } else if (strcmp(*argv, "--use-nan") == 0) {
      use_nan = true;
    } else if (strcmp(*argv, "--sep") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option --sep requires a single character as argument");
      use_sep = *argv;
    } else if (strcmp(*argv, "--delim") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option --delim requires an argument");
      use_delim = *argv;
    } else {
      fin = fopen(*argv, "r");
      chk_exit(fin != 0, "File not found: %s", *argv);
    }

    --argc;  ++argv;
  }

  map<vector<string>, record> accum_map;
  record accum;
  long num = 0;
  log("Starting to read data");
  while (!feof(fin)) {
    char *line = NULL;
    size_t line_size = 0;
    ssize_t rv;
    log("   getline()");
    rv = getline(&line, &line_size, fin);
    if (rv < 0 || line == NULL || strlen(line) == 0)
      break;
    if (line[0] == '#')                     // Can have comments in the file
      continue;
    if (line[strlen(line)-1] == '\n')
	line[strlen(line)-1] = '\0';
    log("      Read line: %s", line);
    if (strlen(line) == 0)
      break;

    vector<string> values;                  // vector of token values for current line
    char *ptr = line;
    char *tok = strsep(&ptr, use_delim);
    while (tok != NULL) {
      log("      parsed: %s", tok);
      values.push_back(string(tok));
      tok = strsep(&ptr, use_delim);
    }
    free(line);

    if (key_fields.empty()) {                            // there are no key fields, so we use all data
      accumulate_on(accum, values);
    } else {
      vector<string> key;
      for (int i = 0; i < (int)values.size(); ++i) {
	if (is_key_field(i)) {
	  key.push_back(values[i]);
	  log("      Pushed %s as key field (i=%d)", values[i].c_str(), i);
	}
      }
      record &r = accum_map[key];
      accumulate_on(r, values);
    }
    ++num;
  }

  log("end of reading and processing file... now output final info");
  if (show_header)  {
    const char *sep = "";
    int num_cols;
    if (key_fields.empty())
      num_cols = get_num_cols(accum);
    else {
      auto it = accum_map.begin();
      num_cols = it->first.size() + get_num_cols(it->second);
    }
    printf("#");
    for (int i = 0; i < num_cols; ++i) {
      if (is_key_field(i)) {
	printf_sep("key%d", i+1);
	continue;
      }
      if (show_avg) {
	printf_sep("avg%d", i+1);
      }
      if (show_dev) {
	printf_sep("dev%d", i+1);
      }
      if (show_1qt) {
	printf_sep("1qt%d", i+1);
      }
      if (show_2qt) {
	printf_sep("2qt%d", i+1);
      }
      if (show_3qt) {
	printf_sep("3qt%d", i+1);
      }
      if (show_min) {
	printf_sep("min%d", i+1);
      }
      if (show_max) {
	printf_sep("max%d", i+1);
      }
      if (show_sum) {
	printf_sep("sum%d", i+1);
      }
      if (use_nan && show_cnt) {
	printf_sep("cnt%d", i+1);
      }
    }
    if (!use_nan && show_cnt) {
      printf_sep("cnt");
    }
    printf("\n");
  }

  if (key_fields.empty()) {
    // use an empty key in this case
    vector<string> key;
    show(key, accum);
  } else {
    map<vector<string>, record>::const_iterator it;
    // iterate over each "key"
    for (it = accum_map.begin(); it != accum_map.end(); ++it) {
      vector<string> const & key = it->first;
      record const & r = it->second;
      show(key, r);
    }
  }
  fclose(fin);
}
