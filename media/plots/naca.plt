# naca.plt — the NACA 0018 section the wing scene uses, at zero and at 8 degrees.
# Matches wing::section() in include/wing.hpp.
#   gnuplot -persist naca.plt              - interactive window
#   gnuplot -e "out='naca.png'" naca.plt   - write a PNG
#   gnuplot -e "out='naca.pdf'" naca.plt   - write a vector PDF

# Terminal follows the extension of out=, so the same script serves a PNG for
# the README and a vector PDF for the thesis.
if (exists("out")) {
    if (strstrt(out, ".pdf") > 0) {
        set terminal pdfcairo size 6.0,3.0 font 'sans,11'
    } else {
        set terminal pngcairo size 900,450 font 'sans,11'
    }
    set output out
}

c   = 90.0        # WING_CHORD_FRAC * (DOMAIN_MAX - DOMAIN_MIN)
th  = 0.18        # WING_THICKNESS_RATIO
aoa = 8.0         # panel default
h   = 9.04        # KERNEL_RADIUS at n_x = 44

# NACA 00xx half-thickness, closed at the trailing edge.
half(u) = 5.0*th*c*(0.2969*sqrt(u) - 0.1260*u - 0.3516*u**2 \
                    + 0.2843*u**3 - 0.1036*u**4)

# Cosine spacing in the parameter, as in the shader-side generator.
u(s)  = 0.5*(1.0 - cos(pi*s))
a     = aoa*pi/180.0
px    = 0.25*c                       # rotation is about the quarter chord

rx(x, y) =  (x - px)*cos(a) + y*sin(a)
ry(x, y) = -(x - px)*sin(a) + y*cos(a)

set title "NACA 0018: хорда 54, ъгъл на атака 8°"
set xlabel "x"
set ylabel "y"
set size ratio -1     # equal axes, or the section reads thinner than it is
set grid
set samples 400
set key top right

set xrange [-22:48]
set yrange [-13:13]

# The kernel radius to the same scale: the section is 1.07 h thick at n_x = 44,
# which is why the trailing edge is not resolved.
set arrow from -19,9 to -19+h,9 heads size 1.2,90 lc rgb "grey30"
set label "h" at -19+h/2,11 center tc rgb "grey30"

set parametric
set trange [0:1]

plot rx(c*u(t),  half(u(t))), ry(c*u(t),  half(u(t))) \
         title "при 8°" with lines lw 2 lc rgb "blue", \
     rx(c*u(t), -half(u(t))), ry(c*u(t), -half(u(t))) \
         notitle with lines lw 2 lc rgb "blue", \
     c*u(t) - px,  half(u(t)) title "при 0°" with lines lw 1 dt 2 lc rgb "grey50", \
     c*u(t) - px, -half(u(t)) notitle with lines lw 1 dt 2 lc rgb "grey50"

if (exists("out")) { unset output }
