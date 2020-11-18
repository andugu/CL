# Pràctica de Compiladors

A compiler of ASL (A Simple Language), an imperative high-level programming language with the basic data types (integer, boolean, character, float), usual control structures (if-then-else, while, functions, …), and arrays (one-dimension, basic type elements). For more details about the target language visit: https://www.cs.upc.edu/~padro/CL/practica/asl.html

Build with the power of ANTLR-4 and C++.

To compile:
`cd asl && make antlr && make`

To clean up:
`make pristine`

Use /examples/.asl as input and compare the output with the corresponding /examples/.err.
Feel free to also use our check-custom-examples.sh script or check-examples.sh to validate.
