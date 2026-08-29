# spiky.plt — the Spiky kernel and its derivative, symmetric about d = 0.
# Not used by the solver: kept for comparison against Wendland C2, whose
# derivative vanishes at d = 0 where this one is at its extremum.
#   gnuplot -persist spiky.plt              - interactive window
#   gnuplot -e "out='spiky.png'" spiky.plt  - write a PNG
#   gnuplot -e "out='spiky.pdf'" spiky.plt  - write a vector PDF

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

h = 3.995         # KERNEL_RADIUS

# W(d) = 10/(pi h^5) (h-|d|)^3;  even, peaks at d = 0 with a cusp
spiky(d) = (abs(d) < h) ? 10.0 * (h - abs(d))**3 / (pi * h**5) : 0.0

# dW/dd = -30 (h-|d|)^2 / (pi h^5);  odd -> *sgn(d). Extremum at d = 0, unlike
# Wendland C2, which is what makes this kernel resist particle clumping.
spiky_deriv(d) = (abs(d) < h) ? -30.0 * (h - abs(d))**2 / (pi * h**5) * sgn(d) : 0.0

set title  "Spiky kernel (h = 3.995)"
set xlabel "signed distance d"
set ylabel "value"
set grid
set zeroaxis lt -1
set samples 400
set key top right

# Line style: solid = the curve this figure is about; dashed and dotted
# mark successive derivatives of it.
plot [-h:h] \
     spiky(x)       title "W (density)"            with lines lw 2       lc rgb "blue", \
     spiky_deriv(x) title "dW/dd (pressure slope)" with lines lw 2 dt 2  lc rgb "red"

if (exists("out")) { unset output }
