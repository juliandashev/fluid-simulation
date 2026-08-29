# poly6.plt — the poly6 gradient and Laplacian the surface-tension color field uses.
# Matches poly6_gradient_coeff() and poly6_laplacian() in shaders/force.comp.
#   gnuplot -persist poly6.plt              - interactive window
#   gnuplot -e "out='poly6.png'" poly6.plt  - write a PNG
#   gnuplot -e "out='poly6.pdf'" poly6.plt  - write a vector PDF

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

# The shader returns a coefficient that multiplies the offset vector, so the
# gradient magnitude is coeff * |d|. Both are plotted: the coefficient is even,
# the resulting magnitude odd.
poly6_coeff(d) = (abs(d) < h) ? -24.0 * (h**2 - d**2)**2 / (pi * h**8) : 0.0
poly6_grad(d)  = (abs(d) < h) ? poly6_coeff(d) * d : 0.0

# Curvature term. Sign flips at |d| = h/sqrt(3), which is where tension turns
# from pulling inward to pushing outward - hence the second axis, it is an
# order of magnitude larger than the gradient.
poly6_lap(d) = (abs(d) < h) ? \
    -48.0 * (h**2 - d**2) * (h**2 - 3.0*d**2) / (pi * h**8) : 0.0

set title  "Poly6 gradient and Laplacian (h = 3.995)"
set xlabel "signed distance d"
set ylabel "gradient"
set y2label "Laplacian"
set ytics nomirror
set y2tics
set grid
set zeroaxis lt -1
set samples 400
set key bottom right

set arrow from  h/sqrt(3.0), graph 0 to  h/sqrt(3.0), graph 1 nohead dt 2 lc rgb "grey40"
set arrow from -h/sqrt(3.0), graph 0 to -h/sqrt(3.0), graph 1 nohead dt 2 lc rgb "grey40"

# Line style: solid = the curve this figure is about; dashed and dotted
# mark successive derivatives of it.
plot [-h:h] \
     poly6_coeff(x) title "coefficient (even)"       with lines lw 2 lc rgb "orange" axes x1y1, \
     poly6_grad(x)  title "gradient magnitude (odd)" with lines lw 2 dt 2 lc rgb "red"    axes x1y1, \
     poly6_lap(x)   title "Laplacian (curvature)"    with lines lw 2 dt 3 lc rgb "blue"   axes x1y2

if (exists("out")) { unset output }
