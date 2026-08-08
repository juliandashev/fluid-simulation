# Matched-pair comparison: artificial viscosity 3.14 vs 0, all else identical.
#   gnuplot viscosity_compare.plt                          - interactive
#   gnuplot -e "out='viscosity_comparison.png'" viscosity_compare.plt
set datafile separator ','

if (exists("out")) {
    set terminal pngcairo noenhanced size 1500,1250 font 'sans,13'
    set output out
}


h=4.0; lam=0.4; lamf=0.25; DT=0.015; DTMIN=0.001
A_BIND = lamf*lamf*h/(DT*DT)    # 1111
V_BIND = lam*h/DT               # 107

C0 = '#c0392b'   # viscosity 0
C1 = '#1a5490'   # viscosity 3.14

set multiplot layout 3,1 \
  title "Artificial viscosity is the only bulk dissipation: removing it destabilises the solver immediately" font 'sans,15'
set lmargin 13
set rmargin 30
set xrange [0:42]
set grid ytics lc rgb '#dddddd'
set key outside right top spacing 1.25 samplen 2

# ---- panel 1: peak acceleration -----------------------------------------
set tmargin 2
set bmargin 0
set format x ''
set xlabel ''
set ylabel "peak |a|  (units/s2)" offset -1,0
set logscale y
set yrange [100:20000]
set ytics (300,1000,3000,10000)
set format y '%g'
set label 1 "NaN sentinel fired (t=8.2 s, 32.4 s)\nclipped from view at 2e4" at 22, 14000 tc rgb C0 font 'sans,10'

plot 'run_visc0.csv'   skip 1 u 2:5 w l lw 1 lc rgb '#eebbb4' t 'visc 0, per sample', \
     'run_visc3.14.csv' skip 1 u 2:5 w l lw 1 lc rgb '#b8cbdd' t 'visc 3.14, per sample', \
     'run_visc0_smoothed.csv'    skip 1 u 1:2 w l lw 3 lc rgb C0 t 'visc 0, rolling median', \
     'run_visc3.14_smoothed.csv'  skip 1 u 1:2 w l lw 3 lc rgb C1 t 'visc 3.14, rolling median', \
     A_BIND w l dt 2 lw 2 lc rgb '#000000' t sprintf('accel-CFL binds above %.0f', A_BIND)
unset label 1

# ---- panel 2: peak speed ------------------------------------------------
set tmargin 0
set ylabel "peak speed  (units/s)" offset -1,0
unset logscale y
set yrange [0:200]
set ytics (50,100,150)
unset title

plot 'run_visc0.csv'   skip 1 u 2:4 w l lw 1 lc rgb '#eebbb4' notitle, \
     'run_visc3.14.csv' skip 1 u 2:4 w l lw 1 lc rgb '#b8cbdd' notitle, \
     'run_visc0_smoothed.csv'    skip 1 u 1:3 w l lw 3 lc rgb C0 t 'visc 0', \
     'run_visc3.14_smoothed.csv'  skip 1 u 1:3 w l lw 3 lc rgb C1 t 'visc 3.14', \
     V_BIND w l dt 2 lw 2 lc rgb '#000000' t sprintf('velocity-CFL binds above %.0f', V_BIND)

# ---- panel 3: the adaptive timestep -------------------------------------
set bmargin 4
set format x '%g'
set xlabel 'sim time (s)'
set ylabel "dt used (s), log scale" offset -1,0
set logscale y
set yrange [0.0008:0.02]
set ytics (0.001,0.002,0.005,0.01,0.015)
set format y '%g'

plot 'run_visc0.csv'   skip 1 u 2:3 w l lw 1.5 lc rgb C0 t 'visc 0', \
     'run_visc3.14.csv' skip 1 u 2:3 w l lw 1.5 lc rgb C1 t 'visc 3.14', \
     DT    w l dt 2 lw 2 lc rgb '#27ae60' t 'ceiling 0.015', \
     DTMIN w l dt 4 lw 2 lc rgb '#8e44ad' t 'DT_MIN floor 0.001'

unset multiplot
