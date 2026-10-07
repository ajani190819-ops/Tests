package dev.arena.spatialhud;

import net.minecraft.client.util.VertexSorter;

/**
 * Tiny indirection so the import lives in exactly one place.
 * !! VERIFY POINT 4 !! — the yarn package of VertexSorter has moved between
 * versions. If this import fails, search your IDE for "VertexSorter" and
 * fix the import; nothing else changes.
 */
public final class VertexSorterHolder {
    private VertexSorterHolder() {
    }

    public static final VertexSorter BY_DISTANCE = VertexSorter.BY_DISTANCE;
}
