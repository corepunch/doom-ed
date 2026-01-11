# Triangulation Tests

This directory contains test files for the polygon triangulation algorithm.

## Test Files

- `triangulate_test.c` - Main test file with 11 comprehensive test cases
- `triangulate_standalone.c` - Standalone version of triangulate.c that can be compiled without full project dependencies
- `map_test.h` - Minimal header file with type definitions needed for testing

## Building and Running Tests

To compile and run the triangulation tests:

```bash
cd mapview
gcc -DTEST_MODE -o triangulate_test triangulate_test.c triangulate_standalone.c -I. -lm
./triangulate_test
```

## Test Coverage

The test suite includes:

1. Basic shapes (triangle, square, pentagon)
2. Concave polygons (L-shape, complex shapes)
3. Different winding orders (CCW and CW)
4. Edge cases (tiny triangles, large polygons, invalid input)
5. Complex shapes (star polygon)
6. Validation tests (area preservation)

## Expected Output

When all tests pass, you should see:

```
=== Running Triangulation Tests ===

Test 1: Simple triangle... PASSED
Test 2: Simple square... PASSED
Test 3: Pentagon... PASSED
Test 4: Concave L-shape... PASSED
Test 5: Complex concave polygon... PASSED
Test 6: Clockwise winding... PASSED
Test 7: Tiny triangle... PASSED
Test 8: Large polygon (16 vertices)... PASSED
Test 9: Invalid input (< 3 vertices)... PASSED
Test 10: Star-shaped polygon... PASSED
Test 11: Area preservation... PASSED

=== All Tests Passed! ===
```

## Clean Up

To remove compiled test binaries:

```bash
rm -f triangulate_test *.o
```
