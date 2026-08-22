# Dam break surge front, dimensionless.
#   gnuplot dam_break.plt                      - interactive
#   gnuplot -e "out='dam_break.png'" dam_break.plt  - write a PNG
# Plots the archived cohesion-50 run (dam_break_coh50_run1.csv).
#
# Columns: 1=sim_time  2=front_x  3=Z  4=T
#   Z = (x_front - x_0)/a     front position in column widths
#   T = t*sqrt(2g/a)          time scaled by the column's free-fall time
# Data paths below are relative to the working directory, so run this from
# experiments/ (e.g. gnuplot ../media/plots/dam_break.plt).
set datafile separator ','

if (exists("out")) {
    set terminal pngcairo noenhanced size 1200,800 font 'sans,13'
    set output out
}

# Keep in sync with defs.hpp
# NOTE: not DAM_ASPECT (2.5) - create_column clamps rows to fit the domain,
# so the column that actually spawned is 46 x 111 -> n = 117.0/48.49 = 2.41.
# Using the requested aspect instead of the realised one biases Ritter by 4%.
n = 2.41

set xlabel 'T  =  t sqrt(2g/a)'
set ylabel 'Z  =  (x_front - x_0) / a'
set key left top
set grid
set xrange [0:3]
set yrange [0:5]
set title 'Dam break: surge front position'

# Ritter (1892) shallow-water dam break: the front of an inviscid column of
# height h0 = n*a advances at 2*sqrt(g*h0), which in these variables is the
# straight line Z = sqrt(2n)*T. It is the frictionless upper bound - a real
# column (and this simulation) should run BELOW it, because bed friction and
# the finite reservoir both retard the front. If your curve sits above this
# line, the front criterion is picking up spray rather than the bulk.
# The +1 matters: Z is measured from the LEFT WALL, so at t=0 the front sits at
# the column's right face, Z=1. Ritter gives the displacement from the dam, not
# the absolute position - forgetting the offset shifts the whole curve down by
# one column width and makes the simulation look like it outruns theory.
ritter(T) = 1.0 + sqrt(2.0*n)*T

# ---------------------------------------------------------------------------
# TO ADD: the experimental reference. Martin & Moyce (1952) tabulate front
# position vs time for the planar case; Koshizuka & Oka (1996) give a second
# dataset. Digitise the curve for YOUR aspect ratio into martin_moyce.csv as
# two columns (T,Z), then uncomment the third plot line below. Do not compare
# against a curve for a different n - the aspect ratio changes the answer.
# ---------------------------------------------------------------------------

plot 'dam_break_coh50_run1.csv' skip 1 using 4:3 with lines lw 2 lc rgb '#1a5490' title 'this simulation', \
     ritter(x) with lines dt 2 lw 2 lc rgb '#c0392b' title 'Ritter (inviscid, frictionless bound)'
#    'martin_moyce.csv' using 1:2 with points pt 7 ps 1.2 lc rgb '#000000' title 'Martin & Moyce (1952)'

if (!exists("out")) { pause -1 }
