# Adaptive-dt trace from a logged run.
#   gnuplot dt.plt                  - interactive window
#   gnuplot -e "out='dt.png'" dt.plt - write a PNG instead
# Expects run.csv (written by the Logger) in the working directory.
#
# run.csv columns: 1=step  2=sim_time  3=dt  4=max_speed  5=max_accel
#
# Plotting max_speed directly was misleading: over a 477s run the velocity CFL
# condition never once bound dt (it sits ~4.6x above the ceiling at the median,
# and max_speed would have to exceed 107 to bind), while the acceleration
# condition bound it in 5.5% of frames. So plot the two CANDIDATE timesteps the
# conditions propose - the lower curve is the active constraint, and dt tracks
# it whenever it dips below the ceiling.
set datafile separator ','

if (exists("out")) {
    set terminal pngcairo size 1400,760 font 'sans,11'
    set output out
}

set xlabel 'sim time (s)'
set ylabel 'timestep (s), log scale'
set logscale y
set yrange [0.005:0.2]
set key outside right top
set grid ytics
set title 'Adaptive dt and the two CFL conditions competing to limit it'

# Mirrored from include/defs.hpp - keep in sync
h    = 4.0    # KERNEL_RADIUS
lam  = 0.4    # CFL_LAMBDA
lamf = 0.25   # CFL_LAMBDA_FORCE
DT   = 0.015  # params.dt ceiling

# The two conditions from src/main.cc:163-170. The 1e-6 guards and the fallback
# to DT mirror the C++ exactly: below that threshold main.cc leaves cfl_dt at
# the ceiling rather than dividing, so the curves cannot disagree with the solver.
vel_dt(v)   = (v > 1e-6 ? lam  * h / v         : DT)
accel_dt(a) = (a > 1e-6 ? lamf * sqrt(h / a)   : DT)

plot 'run.csv' skip 1 using 2:(vel_dt($4)) \
         with lines lw 1 lc rgb '#c0392b' title 'velocity-CFL candidate  0.4h/v', \
     ''        skip 1 using 2:(accel_dt($5)) \
         with lines lw 1 lc rgb '#2980b9' title 'accel-CFL candidate  0.25*sqrt(h/a)', \
     ''        skip 1 using 2:3 \
         with lines lw 2 lc rgb '#111111' title 'dt actually used', \
     DT with lines dt 2 lw 2 lc rgb '#27ae60' title 'params.dt ceiling (0.015)'

if (!exists("out")) { pause -1 }
