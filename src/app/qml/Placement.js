.pragma library

// Where the floating selection toolbar of `size` goes for a selection, both in view
// coordinates: centred above it, `gap` away, and kept inside `view` on both axes, even when the
// selection itself is scrolled out of it. Above if it fits, otherwise below; when neither fits
// the selection fills the view, so: at the top.
function floatingPosition(selection, size, view, gap) {
	const clamp = (value, low, high) => Math.min(Math.max(value, low), Math.max(low, high))
	const left = view.x + gap
	const right = view.x + view.width - gap - size.width
	const top = view.y + gap
	const bottom = view.y + view.height - gap - size.height
	const x = clamp(selection.x + selection.width / 2 - size.width / 2, left, right)

	const above = selection.y - gap - size.height
	const below = selection.y + selection.height + gap
	let y = top
	if (above >= top)
		y = above
	else if (below <= bottom)
		y = below
	return Qt.point(x, clamp(y, top, bottom))
}
