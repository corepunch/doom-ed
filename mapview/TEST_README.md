# Map Rendering Tests

This directory contains test files for the map rendering algorithms.

## Test Files

### Triangulation Tests
- `triangulate_test.c` - Main test file with 11 comprehensive test cases
- `triangulate_standalone.c` - Standalone version of triangulate.c that can be compiled without full project dependencies
- `map_test.h` - Minimal header file with type definitions needed for testing

### BSP Traversal Tests
- `bsp_test.c` - Tests for BSP tree traversal and point-on-side determination

## Building and Running Tests

### Triangulation Tests

To compile and run the triangulation tests:

```bash
cd mapview
gcc -DTEST_MODE -o triangulate_test triangulate_test.c triangulate_standalone.c -I. -lm
./triangulate_test
```

### BSP Tests

To compile and run the BSP tests:

```bash
cd mapview
gcc -o bsp_test bsp_test.c -I. -lm
./bsp_test
```

## Test Coverage

### Triangulation Test Suite

The triangulation test suite includes:

1. Basic shapes (triangle, square, pentagon)
2. Concave polygons (L-shape, complex shapes)
3. Different winding orders (CCW and CW)
4. Edge cases (tiny triangles, large polygons, invalid input)
5. Complex shapes (star polygon)
6. Validation tests (area preservation)

### BSP Test Suite

The BSP test suite includes:

1. Point-on-side tests with vertical partitions
2. Point-on-side tests with horizontal partitions
3. Point-on-side tests with diagonal partitions
4. Simple BSP tree traversal
5. Consistency checks

## Expected Output

### Triangulation Tests

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

### BSP Tests

When all tests pass, you should see:

```
=== Running BSP Traversal Tests ===

Test 1: Point on side (vertical partition)... PASSED
Test 2: Point on side (horizontal partition)... PASSED
Test 3: Point on side (diagonal partition)... PASSED
Test 4: Simple BSP tree traversal... PASSED
Test 5: Consistency check... PASSED

=== All Tests Passed! ===
```

## Clean Up

To remove compiled test binaries:

```bash
rm -f triangulate_test bsp_test *.o
```

