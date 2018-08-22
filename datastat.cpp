/*
 * datastat - Easy command-line data statistics
 *
 * Author: Tommaso Cucinotta
 * Copyright 2011-2013
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
#include <string>

#define LOG_DEBUG 0

using namespace std;

FILE *fin = stdin;
const char *delim = " ,\t";
long key_fields = 0;
bool show_avg = true;
bool show_dev = false;
bool show_1qt = false;
bool show_2qt = false;
bool show_3qt = false;
bool show_min = false;
bool show_max = false;
bool show_cnt = false;
bool show_sum = false;
bool show_sub = false;
bool show_add = false;
bool show_header = true;

int sub_from, sub_to;
int add_a, add_b;

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
  vector< vector<double> > v_val; // TODO
  unsigned long num;
  record() : v_sum(), v_sqr(), v_min(), v_max(), v_val(), num(0) {  }
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
  printf("Source available from: git://git.code.sf.net/p/datastat/code\n");
  printf("Usage: datastat [options] [filename]\n");
  printf("  Options:\n");
  printf("    -h|--help ....... This help message\n");
  printf("    -k|--key cols ... Specify key columns ('-k 3' or '-k 3,5' or '-k 3-5,7' all work");
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
  printf("    --sub a,b ....... Show difference of fields a and b\n");
  printf("    --add a,b ....... Show addition of fields a and b\n");
}

/* Utility macro */
#define printf_sep(msg, args...) do { 		\
	printf("%s" msg, sep, ##args);		\
	sep = " ";				\
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

int main(int argc, char *argv[]) {
  --argc;  ++argv;
  while (argc > 0) {
    if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
      usage();
      exit(0);
    } else if (strcmp(*argv, "-k") == 0 || strcmp(*argv, "--key") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option -k requires an argument");
      key_fields = parse_fields(*argv);
      log("Parsed key_fields: %ld", key_fields);
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
    } else if (strcmp(*argv, "--sub") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option --sub requires two comma-separated fields as argument");
      chk_exit(sscanf(*argv, "%d,%d", &sub_from, &sub_to) == 2, "Option --sub requires two comma-separated fields as argument");
      log("Parsed sub_from,sub_to: %d,%d", sub_from, sub_to);
      show_sub = true;
    } else if (strcmp(*argv, "--add") == 0) {
      --argc;  ++argv;
      chk_exit(argc > 0, "Option --add requires two comma-separated fields as argument");
      chk_exit(sscanf(*argv, "%d,%d", &add_a, &add_b) == 2, "Option --add requires two comma-separated fields as argument");
      log("Parsed add_a,add_b: %d,%d", add_a, add_b);
      show_add = true;
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
    log("   getline(.)");
    rv = getline(&line, &line_size, fin);
    if (rv < 0 || line == NULL)
      break;
    if (line[0] == '#')                     // Can have comments in the file
      continue;
    if (line[strlen(line)-1] == '\n')
	line[strlen(line)-1] = '\0';
    log("      Read line: %s", line);

    vector<string> values;                  // vector of token values for current line
    char *tok = strtok(line, delim);
    while (tok != NULL) {
      log("      parsed: %s", tok);
      values.push_back(string(tok));
      tok = strtok(NULL, delim);
    }
    free(line);

    if (key_fields == 0) {                            // there are no key fields, so we use all data
      for (int i = 0; i < (int)values.size(); ++i) {
	const char *s = values[i].c_str();
	double d;
	sscanf(s, "%lf", &d);                         // d now contains the double value of the read string
	if (accum.num == 0) {
	  if (show_sum || show_avg || show_dev)
	    accum.v_sum.push_back(d);
	  if (show_dev)
	    accum.v_sqr.push_back(d*d);
	  if (show_min)
	    accum.v_min.push_back(d);
	  if (show_max)
	    accum.v_max.push_back(d);
	  if (show_1qt || show_2qt || show_3qt) {
	    accum.v_val.push_back(vector<double>());
	    accum.v_val.back().push_back(d);
	  }
	} else {
	  if (show_sum || show_avg || show_dev)
	    accum.v_sum[i] += d;
	  if (show_dev)
	    accum.v_sqr[i] += d*d;
	  if (show_min)
	    accum.v_min[i] = min(accum.v_min[i], d);
	  if (show_max)
	    accum.v_max[i] = max(accum.v_max[i], d);
	  if (show_1qt || show_2qt || show_3qt)
	    accum.v_val[i].push_back(d);
	}
      }
      ++accum.num;
    } else {
      vector<string> key;
      for (int i = 0; i < (int)values.size(); ++i) {
	if (is_key_field(i)) {
	  key.push_back(values[i]);
	  log("      Pushed %s as key field", values[i].c_str());
	}
      }
      record &r = accum_map[key];
      log("      v_sum[] size: %lu", r.v_sum.size());
      log_noln("      Retrieved values: ");
      log_values(r.v_sum);
      int non_key_id = 0;
      for (int i = 0; i < (int)values.size(); ++i) {
	if (!is_key_field(i)) {
	  const char *s = values[i].c_str();
	  log("         values[%d]=%s", i, s);
	  double d;
	  chk_exit(sscanf(s, "%lf", &d) == 1, "Couldn't parse number");
	  log("      non_key_id=%d, r.v_sum.size()=%lu", non_key_id, r.v_sum.size());
	  if (r.num == 0) {
	    r.v_sum.push_back(d);
	    log("         Pushed back: %g", d);
	    r.v_sqr.push_back(d*d);
	    r.v_min.push_back(d);
	    r.v_max.push_back(d);
            r.v_val.push_back(vector<double>()); // TODO create a vector<double>
            r.v_val[non_key_id].push_back(d);    // TODO add to vector<double>
	  } else {
	    log("         v_sum[non_key_id]=%g, d=%g", r.v_sum[non_key_id], d);
	    r.v_sum[non_key_id] += d;
	    log("         New v_sum[non_key_id]=%g", r.v_sum[non_key_id]);
	    r.v_sqr[non_key_id] += d*d;
	    r.v_min[non_key_id] = min(r.v_min[non_key_id], d);
	    r.v_max[non_key_id] = max(r.v_max[non_key_id], d);
            r.v_val[non_key_id].push_back(d);    // TODO add to vector<double>
	  }
	  ++non_key_id;
	}
      }
      ++r.num;
    }
    ++num;
  }

  log("end of reading and processing file... now output final info");
  if (show_header)  {
    printf("#");
    if (show_avg) {
      printf(" avg");
    }
    if (show_dev) {
      printf(" dev");
    }
    if (show_1qt) {
      printf(" 1qt");
    }
    if (show_2qt) {
      printf(" 2qt");
    }
    if (show_3qt) {
      printf(" 3qt");
    }
    if (show_min) {
      printf(" min");
    }
    if (show_max) {
      printf(" max");
    }
    if (show_sum) {
      printf(" sum");
    }
    if (show_cnt) {
      printf(" cnt");
    }
    printf("\n");
  }

  if (key_fields == 0) {
    const char *sep = "";
    double firstQuantile;
    double median;
    double thirdQuantile;
    int sz = 0;
    if (show_sum || show_avg || show_dev) sz = accum.v_sum.size();
    else if (show_max) sz = accum.v_max.size();
    else if (show_min) sz = accum.v_min.size();
    else if (show_1qt || show_2qt || show_3qt) sz = accum.v_val.size();
    for (int i = 0; i < sz; ++i) {
      if (show_avg) {
	double avg = accum.v_sum[i] / accum.num;
	printf_sep("%g", avg);
      }
      if (show_dev) {
	double avg = accum.v_sum[i] / accum.num;
	printf_sep("%g", sqrt(accum.v_sqr[i]/accum.num - avg*avg));
      }
      if (show_1qt || show_2qt || show_3qt) {                   // TODO
        int  medianPosLow;
        int  medianPosHigh;
        int  firstQuantilePosLow;
        int  firstQuantilePosHigh;
        int  thirdQuantilePosLow;
        int  thirdQuantilePosHigh;
        bool isEvenNumberOfDataPoints;
        log_noln("   v_val         : "); log_values(accum.v_val[i]);
        sort(accum.v_val[i].begin(), accum.v_val[i].end());
        log_noln("   v_val (sorted): "); log_values(accum.v_val[i]);
        log("      size: %ld", accum.v_val[i].size());

        // 2nd quantile
        isEvenNumberOfDataPoints = calculateMedian(accum.v_val[i], &median, &medianPosLow, &medianPosHigh);
        if (isEvenNumberOfDataPoints) {
           medianPosHigh--;
           medianPosLow++;
        }
        {// 1st quantile (including median... so use medianPosHigh)
           vector<double> first = slice(accum.v_val[i], 0, medianPosHigh+1);
           isEvenNumberOfDataPoints = calculateMedian(first, &firstQuantile, &firstQuantilePosLow, &firstQuantilePosHigh);
        }
        if (isEvenNumberOfDataPoints) {
           firstQuantilePosLow--;
           firstQuantilePosHigh++;
        }
        { // 3rd quantile (including median... so use medianPosHigh)
           vector<double> third = slice(accum.v_val[i], medianPosLow, accum.v_val[i].size());
           isEvenNumberOfDataPoints = calculateMedian(third, &thirdQuantile, &thirdQuantilePosLow, &thirdQuantilePosHigh);
        }
      }
      if (show_1qt) {
	printf_sep("%g", firstQuantile); // TODO
      }
      if (show_2qt) {
	printf_sep("%g", median);        // TODO
      }
      if (show_3qt) {
	printf_sep("%g", thirdQuantile); // TODO
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
    if (show_sub) {
      chk_exit(sub_from >= 1 && sub_from <= (int)accum.v_sum.size(), "First arg of --sub out of range");
      chk_exit(sub_to >= 1 && sub_to <= (int)accum.v_sum.size(), "Second arg of --sub out of range");
      printf_sep("%g", accum.v_sum[sub_from - 1] - accum.v_sum[sub_to - 1]);
    }
    if (show_add) {
      chk_exit(add_a >= 1 && add_a <= (int)accum.v_sum.size(), "First arg of --add out of range");
      chk_exit(add_b >= 1 && add_b <= (int)accum.v_sum.size(), "Second arg of --add out of range");
      printf_sep("%g", accum.v_sum[add_a - 1] + accum.v_sum[add_b - 1]);
    }
    printf("\n");
  } else {
    map<vector<string>, record>::const_iterator it;
    // iterate over each "key"
    for (it = accum_map.begin(); it != accum_map.end(); ++it) {
      vector<string> const & key = it->first;
      record const & r = it->second;
      log_noln("   v_sum: ");  log_values(r.v_sum);
      log_noln("   v_sqr: ");  log_values(r.v_sqr);
      log_noln("   v_min: ");  log_values(r.v_min);
      log_noln("   v_max: ");  log_values(r.v_max);
      int key_id = 0;
      int non_key_id = 0;
      const char *sep = "";
      for (int i = 0; i < int(key.size() + r.v_sum.size()); ++i) {
	if (is_key_field(i)) {
	  printf_sep("%s", key[key_id].c_str());
	  ++key_id;
	} else {
          double firstQuantile;
          double median;
          double thirdQuantile;
	  if (show_avg) {
	    double avg = r.v_sum[non_key_id] / r.num;
	    printf_sep("%g", avg);
	  }
	  if (show_dev) {
	    double avg = r.v_sum[non_key_id] / r.num;
	    printf_sep("%g", sqrt(r.v_sqr[non_key_id]/r.num - avg*avg));
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
            { // 3rd quartile (including median... so use medianPosHigh)
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
