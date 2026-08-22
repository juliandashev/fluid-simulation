# Generic parameter sweep: surge front vs any one swept parameter.
#
#   gnuplot -e "files='dam_break_coh0_visc0_ten20.csv dam_break_coh0_visc3.14_ten20.csv'; \
#               labels='visc0 visc3.14'; name='Viscosity'" sweep.plt
#   ... add "; out='viscosity_sweep.png'" to write a PNG instead of a window.
#
# dam_break_sweep.plt stays as it is - it reproduces the archived cohesion
# figure from the archived four-column files, and pinning a published figure to
# its exact inputs is worth more than folding it into something general. This
# script is for the sweeps that come next, whose filenames now carry all three
# force parameters (dam_break_coh<c>_visc<v>_ten<t>.csv) and so cannot be built
# from a bare list of values the way the cohesion names could.
#
# `labels` is one whitespace-separated word per file, in the same order. It
# defaults to the filenames, which are full of underscores, and underscores read
# as subscripts in enhanced mode - hence `set key noenhanced`, which applies
# whichever terminal the interactive branch happens to pick.
# Data paths below are relative to the working directory, so run this from
# experiments/ (e.g. gnuplot ../media/plots/sweep.plt).
set datafile separator ','
set key noenhanced

if (!exists("files")) {
    print "usage: gnuplot -e \"files='a.csv b.csv'; labels='a b'\" sweep.plt"
    exit
}
if (!exists("labels")) { labels = files }
if (!exists("name"))   { name   = "parameter" }

if (exists("out")) {
    set terminal pngcairo noenhanced size 1300,850 font 'sans,13'
    set output out
}

# Realised column aspect (46 x 111 particles, a = 48.49), not DAM_ASPECT - see
# dam_break.plt for why using the requested 2.5 biases Ritter by 4%.
n = 2.41
ritter(T) = 1.0 + sqrt(2.0*n)*T

# The far wall. The column spawns against the left wall and the domain is only
# 3.4 column widths wider than it, so the front runs out of tank at Z = 4.35 and
# every sample after that is pile-up rather than collapse. Drawn, because a
# curve flattening here has been stopped by the geometry, not by the physics,
# and the two look identical if the line is not on the plot.
Z_WALL = 4.3476

colours = "#2c7fb8 #41ab5d #d95f0e #c0392b #7b3294 #000000"

set xlabel 'T  =  t sqrt(2g/a)'
set ylabel 'Z  =  (x_front - x_0) / a'
# Bottom right: the curves sweep from lower-left to upper-right and leave that
# corner empty, while top-left is where the wall annotation has to sit.
set key right bottom spacing 1.2
set grid
set xrange [0:3.5]
set yrange [0.8:4.6]
set title 'Dam break surge front vs '.name

set label 1 "far wall - front stops here for geometric reasons" \
    at 0.15, Z_WALL+0.09 tc rgb '#888888' font 'sans,10'

plot for [i=1:words(files)] \
        word(files,i) skip 1 using 4:3 \
        with lines lw 2 lc rgb word(colours, 1+(i-1)%words(colours)) \
        title word(labels,i), \
     ritter(x) with lines dt 2 lw 2 lc rgb '#000000' title 'Ritter (inviscid bound)', \
     Z_WALL with lines dt 3 lw 1.5 lc rgb '#888888' notitle

if (!exists("out")) { pause -1 }
