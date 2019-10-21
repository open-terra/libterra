add_library(terra STATIC
    src/grid/poisson_disc_sampler.cpp
    src/noise/open_simplex.cpp
    src/noise/summed_noise.cpp
    src/types/hash_grid.cpp
    src/delaunator.cpp
)