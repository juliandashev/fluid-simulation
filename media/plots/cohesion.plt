# cohesion.plt — the Akinci pairwise cohesion weight, symmetric about d = 0.
# Matches cohesion_kernel() in shaders/force.comp.
#   gnuplot -persist cohesion.plt              - interactive window
#   gnuplot -e "out='cohesion.png'" cohesion.plt  - write a PNG
#   gnuplot -e "out='cohesion.pdf'" cohesion.plt  - write a vector PDF

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

# C(d) = q^2 (1-q)^2,  q = |d|/h;  even, peaks at q = 1/2 and vanishes at both ends
cohesion(d) = (abs(d) < h) ? (abs(d)/h)**2 * (1.0 - abs(d)/h)**2 : 0.0

set title  "Akinci pairwise cohesion kernel (h = 3.995)"
set xlabel "signed distance d"
set ylabel "weight"
set grid
set zeroaxis lt -1
set samples 400
set key top right
set arrow from -h/2,0 to -h/2,0.0625 nohead dt 2 lc rgb "gray"
set arrow from  h/2,0 to  h/2,0.0625 nohead dt 2 lc rgb "gray"

# Line style: solid = kernel, dashed = first derivative, dotted = Laplacian.
plot [-h*1.1:h*1.1] \
     cohesion(x) title "C (peaks at h/2, zero at contact)" with lines lw 2 lc rgb "dark-green"

if (exists("out")) { unset output }
