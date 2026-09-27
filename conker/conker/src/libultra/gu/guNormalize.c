// No includes: gu.h makes sqrtf an intrinsic (an inline sqrt.s), while this game's
// guNormalize calls the sqrtf function.
float sqrtf(float);

// libultra's guNormalize: scales the vector (*x, *y, *z) to length 1, in place.
void guNormalize(float *x, float *y, float *z) {
    float m;

    m = 1 / sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    *x *= m;
    *y *= m;
    *z *= m;
}
