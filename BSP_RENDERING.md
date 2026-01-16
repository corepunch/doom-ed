# BSP-Based Rendering - Removed

**Note: As of January 2026, BSP-based rendering has been removed from this project.**

The renderer now uses only portal-based rendering to traverse connected sectors through two-sided linedefs. BSP data (nodes, subsectors, segs) is no longer loaded from WAD files.

## Why BSP Was Removed

The project has transitioned to use portal-based rendering exclusively. This simplifies the codebase and removes the dependency on precomputed BSP data while still providing functional rendering through the portal traversal system.

## Portal-Based Rendering

The current rendering system uses `draw_portals()` in `floor.c` to recursively traverse connected sectors:

1. Start from the player's sector
2. Render the current sector's floors, ceilings, and walls
3. Check all two-sided linedefs in the sector
4. For each portal (two-sided linedef), check if it's visible using frustum culling
5. Recursively render connected sectors
6. Frame tracking prevents duplicate sector rendering

## Historical Context

This document is preserved for historical reference. The original BSP implementation is described below.

---

# Original BSP-Based Rendering Implementation (Historical)

## Overview
This document describes the BSP (Binary Space Partitioning) based rendering system implemented to fix the visibility issues in the DOOM level renderer. The implementation follows the approach used in the original Doom and GZDoom engines.

## Problem Statement
The original rendering code used a portal-based system that had issues with frame tracking, causing sectors to be incorrectly dropped or not rendered. The comment in the code indicated: "there are already added some pieces to filter out invisible sectors, but it's not working correctly, some sectors get dropped and so on. So currently it's just drawing everything."

## Solution
We implemented a proper BSP tree traversal system based on the Doom/GZDoom approach that:
1. Uses the BSP data already loaded from DOOM WAD files
2. Correctly determines which sectors are visible from the player's viewpoint
3. Efficiently culls invisible geometry using Doom's angle-based frustum culling
4. Maintains proper frame tracking to avoid redrawing sectors
5. Uses the same checkcoord lookup table and algorithm as original Doom

## Architecture

### Core Components

#### 1. BSP Tree Traversal (`bsp.c`)
The main rendering algorithm that traverses the BSP tree to determine visible sectors, following the Doom engine approach.

**Key Functions:**
- `draw_bsp()` - Entry point for BSP-based rendering
- `R_RenderBSPNode()` - Recursively traverses BSP nodes (from Doom)
- `R_Subsector()` - Renders a single subsector (leaf node)
- `R_PointOnSide()` - Determines which side of a partition line a point is on (from Doom)
- `R_CheckBBox()` - Checks if a bounding box might be visible using Doom's algorithm
- `R_PointToAngle()` - Calculates angle from one point to another

#### 2. Integration (`windows/game.c`)
The game rendering loop was updated to use BSP rendering instead of the flawed portal system.

**Changes:**
- Removed manual portal-based rendering
- Added call to `draw_bsp()` with automatic fallback
- Simplified rendering code

#### 3. Fallback System
For maps without BSP data (e.g., manually created maps), the system automatically falls back to the portal-based rendering approach.

## How BSP Rendering Works

### 1. BSP Tree Structure
The BSP tree is a binary tree where:
- **Leaf nodes** (subsectors) contain the actual geometry to render
- **Internal nodes** contain partition lines that divide space

Each node has:
- A partition line (x, y, dx, dy)
- Two bounding boxes (one for each child)
- Two children (which can be nodes or subsectors)

### 2. Traversal Algorithm

```
function RenderBSPNode(node):
    if node is a subsector:
        RenderSubsector(node)
        return
    
    side = PointOnSide(player_position, node.partition)
    
    // Render player's side first (front-to-back order)
    RenderBSPNode(node.children[side])
    
    // Check if back side might be visible
    if CheckBBox(node.bbox[other_side]):
        RenderBSPNode(node.children[other_side])
```

### 3. Point-On-Side Test
Determines which side of a partition line a point is on using cross product:

```
cross_product = (point.x - line.x) * line.dy - (point.y - line.y) * line.dx

if cross_product < 0:
    point is on front side
else:
    point is on back side
```

### 4. Frustum Culling (Doom Algorithm)
The bounding box visibility check uses Doom's original algorithm:

1. **Determine box position** relative to view point (9 possible positions)
2. **Select critical corners** using `checkcoord` lookup table
3. **Calculate angles** from view point to corners
4. **Check if outside FOV**: Both corners outside left or right clipping plane

This is more sophisticated than simple distance checks because it:
- Accounts for the player's view direction
- Uses angle-based frustum culling (90-degree FOV)
- Properly handles edge cases (player inside box, box behind player, etc.)

**Key differences from simplified approach:**
- Original implementation always returned `true` (no actual culling)
- Doom approach uses `checkcoord` table to select relevant box corners based on view position
- Calculates angles to corners and checks against field of view
- Can reject entire BSP subtrees when they're outside the view frustum

### 5. Frame Tracking
Each sector tracks the last frame number it was rendered in. This prevents:
- Rendering the same sector multiple times in one frame
- Overdraw and performance issues

## Performance Benefits

1. **Visibility Culling**: BSP tree automatically culls geometry outside the view frustum
2. **Front-to-Back Rendering**: Renders closer sectors first, enabling early-z rejection
3. **Bounding Box Checks**: Quickly rejects entire subtrees that aren't visible
4. **No Overdraw**: Frame tracking ensures each sector is rendered exactly once

## Testing

### BSP Traversal Tests (`tests/bsp_test.c`)
Comprehensive tests covering:

1. **Vertical Partition Test**: Points on left/right sides of a vertical line
2. **Horizontal Partition Test**: Points above/below a horizontal line
3. **Diagonal Partition Test**: Points on either side of a diagonal line
4. **Tree Traversal Test**: Verifies correct child selection
5. **Consistency Test**: Ensures deterministic results

All tests pass successfully:
```bash
make test
# or manually:
gcc -o bsp_test tests/bsp_test.c -Imapview -Itests -lm
./bsp_test
```

## Compatibility

### Maps With BSP Data
- All official DOOM/DOOM II maps
- Most user-created WADs
- Uses efficient BSP traversal

### Maps Without BSP Data
- Manually created maps in the editor
- Falls back to portal-based rendering
- Still functional, just less efficient

## Code Quality

### Improvements Made
1. **Proper abstraction**: Separate BSP logic from rendering
2. **Error handling**: Bounds checking on all array accesses
3. **Frame tracking**: Prevents duplicate rendering
4. **Fallback system**: Graceful degradation for maps without BSP
5. **Comprehensive tests**: 5 test cases covering core functionality

### Comparison with Original DOOM Code
Our implementation follows the original DOOM source code approach with these key features:
- **Same BSP traversal algorithm** as Doom/GZDoom
- **Same R_PointOnSide logic** for partition line testing
- **Same checkcoord lookup table** for bounding box culling
- **Angle-based frustum culling** matching Doom's approach
- **Modern C standards** and integration with OpenGL rendering
- **Simplified screen column clipping** (OpenGL handles projection)

## Future Improvements

Potential enhancements:
1. ~~**Frustum Culling**: Proper view frustum checks in `R_CheckBBox()`~~ ✅ **Implemented** using Doom's algorithm
2. **Occlusion Culling**: Track which screen columns are already drawn (like original Doom's solidsegs)
3. **Portal Rendering**: Combine BSP and portal systems for hybrid approach
4. **Dynamic Lighting**: Use BSP for shadow calculations
5. **Multi-threading**: Parallel BSP traversal for large maps

## References

- [Original DOOM source code](https://github.com/id-Software/DOOM)
- [Doom Wiki - BSP](https://doomwiki.org/wiki/Node)
- [Fabien Sanglard's DOOM code review](https://fabiensanglard.net/doomIphone/doomClassicRenderer.php)

## See Also

- `TRIANGULATION_IMPROVEMENTS.md` - Improvements to polygon triangulation
- `tests/TEST_README.md` - Test documentation and usage
- `mapview/bsp.c` - BSP implementation source code
- `tests/bsp_test.c` - BSP test suite
