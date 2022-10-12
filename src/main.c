#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    printf("sf_internal_fragmentation: %f\n", sf_internal_fragmentation()); //0.0

    printf("sf_peak_utilization: %f\n", sf_peak_utilization()); //0.0

    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("sf_internal_fragmentation: %f\n", sf_internal_fragmentation()); //0.25

    printf("sf_peak_utilization: %f\n", sf_peak_utilization()); //0.0078125

    printf("%f\n", *ptr);

    sf_free(ptr);

    printf("sf_internal_fragmentation: %f\n", sf_internal_fragmentation()); //0.0

    printf("sf_peak_utilization: %f\n", sf_peak_utilization()); //0.0078125


    return EXIT_SUCCESS;
}
