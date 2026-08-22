# spiky.plt — visualize the spiky kernel(s) and derivative, symmetric about d = 0.
# Matches spiky_kernel_derivative() in src/simulation.hpp.
# Run with:  gnuplot -persist spiky.plt

h = 16.0          # KERNEL_RADIUS

# Kernels depend only on |d|, so wrap the argument in abs() to see the symmetric shape.

# 2D-normalized spiky value:   W(d) = (h - |d|)^3 * 10 / (pi h^5)
spiky(d)       = (abs(d) < h) ?  (h - abs(d))**3 * 10.0 / (pi * h**5) : 0.0

# 2D spiky derivative dW/dd (the force "slope"); odd function -> *sign(d):
#   -30 (h - |d|)^2 / (pi h^5)
spiky_deriv(d) = (abs(d) < h) ? -30.0 * (h - abs(d))**2 / (pi * h**5) * sgn(d) : 0.0

# Müller 2003 *3D* spiky value:  W(d) = 15/(pi h^6) * (h - |d|)^3   (note the 15)
spiky_3d(d)       = (abs(d) < h) ?  15.0 * (h - abs(d))**3 / (pi * h**6) : 0.0

# Müller 3D spiky derivative dW/dd = -45/(pi h^6) * (h - |d|)^2 ; odd -> *sgn(d)
spiky_3d_deriv(d) = (abs(d) < h) ? -45.0 * (h - abs(d))**2 / (pi * h**6) * sgn(d) : 0.0

set title  "Spiky kernels (symmetric about d = 0, h = 16)"
set xlabel "signed distance d"
set ylabel "value"
set grid
set zeroaxis lt -1            # draw x and y axes so the sign/symmetry is obvious
set samples 400
set key top right

plot [-h*1.1:h*1.1] \
     spiky(x)       title "W_spiky 2D"            with lines lw 2 lc rgb "blue",  \
     spiky_deriv(x) title "dW/dd 2D (odd)"        with lines lw 2 lc rgb "red",   \
     spiky_3d(x)       title "W_spiky 3D (Muller)"     with lines lw 2 lc rgb "green",  \
     spiky_3d_deriv(x) title "dW/dd 3D (Muller, odd)"  with lines lw 2 lc rgb "orange"
