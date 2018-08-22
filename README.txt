datastat - Simple command-line data statistics
           Copyright 2011-2019 by Tommaso Cucinotta
           firstname dot lastname at gmail dot com
======================================================================
datastat is an open-source command-line tool that allows one to
compute simple statistics over columns of numeric data in text files
by aggregating rows on the basis of the values of specified key
columns.

The simplest usage of the tool is to compute the average of all
columns in a file with many rows. For example:

  datastat myfile.dat

will produce on the standard output a single row, containing as many
columns as in the input file, and with each value being the average of
all the values in the corresponding column. If one wanted also the
standard deviation, then

  datastat --dev myfile.dat

would provide an output file where, for each input file column, there
are two output values, one with the average and the following one with
the standard deviation of all the values in that column.

A more complex usage is when you need to aggregate data in the input
file based on some key columns. For example, the input file contains 3
columns with the first two columns being configuration options for some
experiment, and the third column being the actual output of the
experimentation. The file may have many such rows, with repeated
entries per configuration. The user would like to compute the average
values aggregated depending on the first configuration parameter:

  datastat -k 1-2 myfile.dat

This will produce an output with multiple rows, one for each value
pairs within the first two columns of the input file, and for each row
one can find the average of each configuration on the third column.

Other statistics that can be easily computed over all the values
within a column, or all the values within each key value set, include
the standard deviation, the minimum, the maximum and the elements
count.

datastat has been purposedly kept at a minimum of functionality. Its
power resides in pipe-ing it with other common UNIX tools for
column-based numeric table processing, including grep, sed, cut,
paste, awk and sort. Ultimately, it may be extremely useful to use
datastat in combination with said tools in a GNUplot plotting script.


License of use
======================================================================
datastat is provided under the GPLv3 license. See LICENSE.txt for
details.
