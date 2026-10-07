package dev.arena.spatialhud;

import net.minecraft.client.renderer.VertexSorting;

/**
 * Tiny indirection so the mojmap import lives in exactly one place.
 * If VertexSorting's constant differs on your version, this is the only
 * file to touch.
 */
public final class VertexSortingHolder {
    private VertexSortingHolder() {
    }

    public static final VertexSorting BY_DISTANCE = VertexSorting.BY_DISTANCE;
}
