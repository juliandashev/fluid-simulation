# viscosity_kernel.plt — the Laplacian kernel the Muller viscosity term uses.
# Matches viscosity_kernel_laplacian() in shaders/force.comp.
#   gnuplot -persist viscosity_kernel.plt              - interactive window
#   gnuplot -e "out='viscosity_kernel.png'" viscosity_kernel.plt  - write a PNG
#   gnuplot -e "out='viscosity_kernel.pdf'" viscosity_kernel.plt  - write a vector PDF

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

# lap W(d) = 40 (h - |d|) / (pi h^5);  even, a linear ramp to zero at |d| = h
visc_lap(d) = (abs(d) < h) ? 40.0 * (h - abs(d)) / (pi * h**5) : 0.0

set title  "Muller Laplacian viscosity kernel (h = 3.995)"
set xlabel "signed distance d"
set ylabel "value"
set grid
set zeroaxis lt -1
set samples 400
set key top right

# Line style: solid = kernel, dashed = first derivative, dotted = Laplacian.
plot [-h*1.1:h*1.1] \
     visc_lap(x) title "lap W (velocity smoothing weight)" with lines lw 2 dt 3 lc rgb "purple"

if (exists("out")) { unset output }
