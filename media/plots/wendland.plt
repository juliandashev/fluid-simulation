# wendland.plt — the Wendland C2 kernel and its derivative, symmetric about d = 0.
# Matches smoothing_kernel() and smoothing_kernel_derivative() in shaders/common.glsl.
#   gnuplot -persist wendland.plt              - interactive window
#   gnuplot -e "out='wendland.png'" wendland.plt  - write a PNG
#   gnuplot -e "out='wendland.pdf'" wendland.plt  - write a vector PDF

# Terminal follows the extension of out=, so the same script serves a PNG for
# the README and a vector PDF for the thesis.
if (exists("out")) {
    if (strstrt(out, ".pdf") > 0) {
        # narrow=1 for a figure sat three-across; the font stays readable
        # because the canvas shrinks with it instead of being scaled down.
        if (exists("narrow")) {
            set terminal pdfcairo size 2.6,2.3 font 'sans,8'
        } else {
            set terminal pdfcairo size 6.0,3.7 font 'sans,11'
        }
    } else {
        set terminal pngcairo size 900,560 font 'sans,11'
    }
    set output out
}

h = 3.995         # KERNEL_RADIUS = KERNEL_RATIO * PARTICLE_SPACING

# W(d) = 7/(pi h^2) * (1-s)^4 (4s+1),  s = |d|/h;  even, peaks at d = 0
wendland(d) = (abs(d) < h) ? \
    7.0 / (pi * h**2) * (1.0 - abs(d)/h)**4 * (4.0*abs(d)/h + 1.0) : 0.0

# dW/dd = -140 s (1-s)^3 / (pi h^3);  odd -> *sgn(d). Zero at d = 0, extremum at s = 1/4.
wendland_deriv(d) = (abs(d) < h) ? \
    -140.0 * (abs(d)/h) * (1.0 - abs(d)/h)**3 / (pi * h**3) * sgn(d) : 0.0

set title  "Wendland C2 kernel (h = 3.995)"
set xlabel "signed distance d"
set ylabel "value"
set grid
set zeroaxis lt -1
set samples 400
set key top right

# Line style: solid = the curve this figure is about; dashed and dotted
# mark successive derivatives of it.
plot [-h:h] \
     wendland(x)       title "W (density)"            with lines lw 2 lc rgb "blue", \
     wendland_deriv(x) title "dW/dd (pressure slope)" with lines lw 2 dt 2 lc rgb "red"

if (exists("out")) { unset output }
