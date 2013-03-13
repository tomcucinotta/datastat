/*
 * datastat - Easy command-line data statistics
 *
 * Author: Tommaso Cucinotta
 * Copyright 2011-2013
 * 
 * License: GPLv3, see LICENSE.txt file for details
 */

#include <stdio.h>
#include <readline/readline.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <vector>
#include <map>
#include <string>

#define LOG_DEBUG 0

using namespace std;

FILE *fin = stdin;
const char *delim = " \t";
long key_fields = 0;
bool show_avg = true;
bool show_dev = false;
bool show_min = false;
bool show_max = false;
bool show_cnt = false;
bool show_sum = false;

#if LOG_DEBUG
#define log_noln(fmt, args...) do {	\
    printf(fmt, ##args);	\
  } while (0)
#else
#define log_noln(fmt, args...)
#endif

#define log(fmt, args...) do {	\
    log_noln(fmt "\n", ##args);	\
  } while (0)

#define chk_exit(cond,msg, args...) do {			\
    if (!cond) {						\
      fprintf(stderr, "Error: " msg "\n", ##args);		\
      exit(-1);							\
    }								\
  } while (0)

// f goes from zero
static bool is_key_field(int f) {
  return (key_fields & (1 << f)) != 0;
}

static void log_values(vector<double> const & values) {
  vector<double>::const_iterator vit;
  for (vit = values.begin(); vit != values.end(); ++vit) {
    if (vit - values.begin() > 0)
      log_noln(" ");
    log_noln("%g", *vit);
  }
  log("");
}

struct record {
  vector<double> v_sum;
  vector<double> v_sqr;
  vector<double> v_min;
  vector<double> v_max;
  unsigned long num;
  record() : v_sum(), v_sqr(), v_min(), v_max(), num(0) {  }
};

static long parse_fields(char *s) {
  long fields = 0;
  char *tok = strtok(s, ",");
  chk_exit(tok != 0, "Wrong syntax for fields argument");
  while (tok != NULL) {
    int i1, i2;
    if (sscanf(tok, "%d-%d", &i1, &i2) == 2) {
      for (int i = i1 - 1; i < i2; ++i) {
	fields |= (1 << i);
      }
    } else if (sscanf(tok, "%d", &i1) == 1) {
      fields |= (1 << (i1 - 1));
    } else {
      chk_exit(false, "Wrong syntax for fields argument");
    }
    tok = strtok(NULL, ",");
  }
  return fields;
}

void usage() {
  printf("Usage: datastat [options] [filename]\n");
  printf("  Options:\n");
  printf("    -h|--help ....... This help message\n");
  printf("    --no-avg ........ Suppress average in output\n");
  printf("    --dev ........... Show standard deviation in output\n");
  printf("    --min ........... Show minimum in output\n");
  printf("    --max ........... Show maximum in output\n");
}

/* Utility macro */
#define printf_sep(msg, args...) do { 		\
	printf("%s" msg, sep, ##args);		\
	sep = " ";				\
      } while (0)

int main(int argc, char *argv[]) {
  --argc;  ++argv;
  while (argc > 0) {
    if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
      usage();
      exit(0);
    } else if (strcmp(*argv, "-k") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option -k requires an argument");
      key_fields = parse_fields(*argv);
      log("Parsed key_fields: %ld", key_fields);
    } else if (strcmp(*argv, "--no-avg") == 0) {
      show_avg = false;
    } else if (strcmp(*argv, "--dev") == 0) {
      show_dev = true;
    } else if (strcmp(*argv, "--min") == 0) {
      show_min = true;
    } else if (strcmp(*argv, "--max") == 0) {
      show_max = true;
    } else if (strcmp(*argv, "--cnt") == 0) {
      show_cnt = true;
    } else if (strcmp(*argv, "--sum") == 0) {
      show_sum = true;
    } else {
      fin = fopen(*argv, "r");
      chk_exit(fin != 0, "File not found: %s", *argv);
    }

    --argc;  ++argv;
  }

  map<vector<string>, record> accum_map;
  record accum;
  long num = 0;
  while (!feof(fin)) {
    char *line = NULL;
    size_t line_size = 0;
    ssize_t rv = getline(&line, &line_size, fin);
    if (rv < 0 || line == NULL)
      break;
    if (line[0] == '#')
      continue;
    if (line[strlen(line)-1] == '\n')
	line[strlen(line)-1] = '\0';
    log("Read line: %s", line);

    vector<string> values;
    char *tok = strtok(line, delim);
    while (tok != NULL) {
      log("parsed: %s", tok);
      values.push_back(string(tok));
      tok = strtok(NULL, delim);
    }
    free(line);

    if (key_fields == 0) {
      for (int i = 0; i < (int)values.size(); ++i) {
	const char *s = values[i].c_str();
	double d;
	sscanf(s, "%lf", &d);
	if (i >= (int)accum.v_sum.size()) {
	  accum.v_sum.push_back(d);
	  accum.v_sqr.push_back(d*d);
	  accum.v_min.push_back(d);
	  accum.v_max.push_back(d);
	} else {
	  accum.v_sum[i] += d;
	  accum.v_sqr[i] += d*d;
	  accum.v_min[i] = min(accum.v_min[i], d);
	  accum.v_max[i] = max(accum.v_max[i], d);
	}
	++accum.num;
      }
    } else {
      vector<string> key;
      for (int i = 0; i < (int)values.size(); ++i) {
	if (is_key_field(i)) {
	  key.push_back(values[i]);
	  log("Pushed %s as key field", values[i].c_str());
	}
      }
      record &r = accum_map[key];
      log("v_sum[] size: %lu", r.v_sum.size());
      log_noln("Retrieved values: ");
      log_values(r.v_sum);
      int non_key_id = 0;
      for (int i = 0; i < (int)values.size(); ++i) {
	if (!is_key_field(i)) {
	  const char *s = values[i].c_str();
	  log("values[%d]=%s", i, s);
	  double d;
	  chk_exit(sscanf(s, "%lf", &d) == 1, "Couldn't parse number");
	  log("non_key_id=%d, r.v_sum.size()=%lu", non_key_id, r.v_sum.size());
	  if (non_key_id >= (int)r.v_sum.size()) {
	    r.v_sum.push_back(d);
	    log("Pushed back: %g", d);
	    r.v_sqr.push_back(d*d);
	    r.v_min.push_back(d);
	    r.v_max.push_back(d);
	  } else {
	    log("v_sum[non_key_id]=%g, d=%g", r.v_sum[non_key_id], d);
	    r.v_sum[non_key_id] += d;
	    log("New v_sum[non_key_id]=%g", r.v_sum[non_key_id]);
	    r.v_sqr[non_key_id] += d*d;
	    r.v_min[non_key_id] = min(r.v_min[non_key_id], d);
	    r.v_max[non_key_id] = max(r.v_max[non_key_id], d);
	  }
	  ++non_key_id;
	}
      }
      ++r.num;
    }
    ++num;
  }

  if (key_fields == 0) {
    const char *sep = "";
    for (int i = 0; i < (int)accum.v_sum.size(); ++i) {
      double avg = accum.v_sum[i] / accum.num;
      if (show_avg) {
	printf_sep("%g", avg);
      }
      if (show_dev) {
	printf_sep("%g", sqrt(accum.v_sqr[i]/accum.num - avg*avg));
      }
      if (show_min) {
	printf_sep("%g", accum.v_min[i]);
      }
      if (show_max) {
	printf_sep("%g", accum.v_max[i]);
      }
      if (show_sum) {
	printf_sep("%g", accum.v_sum[i]);
      }
    }
    if (show_cnt) {
      printf_sep("%lu", accum.num);
    }
    printf("\n");
  } else {
    map<vector<string>, record>::const_iterator it;
    for (it = accum_map.begin(); it != accum_map.end(); ++it) {
      vector<string> const & key = it->first;
      record const & r = it->second;
      log_noln("v_sum: ");  log_values(r.v_sum);
      log_noln("v_sqr: ");  log_values(r.v_sqr);
      log_noln("v_min: ");  log_values(r.v_min);
      log_noln("v_max: ");  log_values(r.v_max);
      int key_id = 0;
      int non_key_id = 0;
      const char *sep = "";
      for (int i = 0; i < int(key.size() + r.v_sum.size()); ++i) {
	if (is_key_field(i)) {
	  printf_sep("%s", key[key_id].c_str());
	  ++key_id;
	} else {
	  double avg = r.v_sum[non_key_id] / r.num;
	  if (show_avg) {
	    printf_sep("%g", avg);
	  }
	  if (show_dev) {
	    printf_sep("%g", sqrt(r.v_sqr[non_key_id]/r.num - avg*avg));
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

	  ++non_key_id;
	}
      }
      if (show_cnt) {
	printf_sep("%lu", r.num);
      }
      printf("\n");
    }
  }
}
