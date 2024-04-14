
struct complex {
    float real;
    float imaginary;
};

complex mul(complex a, complex b) {
    return complex(
        a.real * b.real - a.imaginary * b.imaginary,
        a.real * b.imaginary + a.imaginary * b.real
    );
}

complex add(complex a, complex b) {
    return complex(
        a.real + b.imaginary,
        a.imaginary + b.imaginary
    );
}

complex conj(complex a) {
    return complex(a.real, -a.imaginary);
}

