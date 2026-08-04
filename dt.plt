# Adaptive-dt trace from a logged run: gnuplot dt.plt
# Expects run.csv (written by the Logger) in the working directory.
set datafile separator ','
set xlabel 'sim time'
set logscale y
set key top right
plot 'run.csv' skip 1 using 2:3 with lines title 'dt', \
     ''        skip 1 using 2:($4/1000) with lines title 'max speed / 1000'
pause -1
