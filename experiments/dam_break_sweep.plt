# Cohesion sweep: surge front vs cohesion strength, all else identical.
#   gnuplot dam_break_sweep.plt
#   gnuplot -e "out='dam_break_sweep.png'" dam_break_sweep.plt
#
# Expects dam_break_coh<V>.csv for each swept value, written automatically by
# DamBreakLogger when you press R with that cohesion set in the panel.
#
# The question this answers: cohesion at 50 puts the capillary length (14-43
# units) at the same scale as the column width (48.5), so surface tension is
# holding the column up against gravity. If that is what causes the flat start
# in the front trace, lowering cohesion should shorten the plateau. If the
# plateau survives at cohesion 0, the cause is viscosity or the pressure
# solver, not cohesion.
set datafile separator ','

if (exists("out")) {
    set terminal pngcairo noenhanced size 1300,850 font 'sans,13'
    set output out
}

n = 2.41    # realised column aspect (46 x 111), not DAM_ASPECT - see dam_break.plt
ritter(T) = 1.0 + sqrt(2.0*n)*T

set xlabel 'T  =  t sqrt(2g/a)'
set ylabel 'Z  =  (x_front - x_0) / a'
set key left top spacing 1.2
set grid
set xrange [0:3.5]
set yrange [0.8:5]
set title 'Dam break surge front vs cohesion strength'

# Add or remove values here to match the files you actually produced. gnuplot
# aborts if one is missing, so keep this list in sync with what you ran.
values  = "0 5 20 50"
colours = "#2c7fb8 #41ab5d #d95f0e #c0392b"

plot for [i=1:words(values)] \
        "dam_break_coh".word(values,i).".csv" skip 1 using 4:3 \
        with lines lw 2 lc rgb word(colours,i) \
        title "cohesion ".word(values,i), \
     ritter(x) with lines dt 2 lw 2 lc rgb '#000000' title 'Ritter (inviscid bound)'

if (!exists("out")) { pause -1 }
