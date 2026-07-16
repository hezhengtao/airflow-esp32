"""Generate fan_head.bin and fan_stand.bin as ARGB8888 from fan.svg."""
import sys
sys.path.insert(0, r"D:\python\Lib\site-packages")

from svgelements import SVG, Path, Move, Line, CubicBezier, Close, QuadraticBezier, Arc
from PIL import Image, ImageDraw
import math

SIZE = 140

def split_subpaths(path):
    """Split a Path into subpaths at each Move command."""
    subpaths = []
    current = []
    for seg in path:
        if isinstance(seg, Move) and current:
            subpaths.append(current)
            current = []
        current.append(seg)
    if current:
        subpaths.append(current)
    return subpaths

def path_to_polygon_points(path, num_segments=300):
    """Approximate a path as a list of (x,y) points using piecewise sampling."""
    from svgelements import Line, Close, CubicBezier, QuadraticBezier, Arc, Move
    points = []
    for seg in path:
        if isinstance(seg, Move):
            if points:
                pass  # Continuation
            points.append((seg.end.x, seg.end.y))
        elif isinstance(seg, Line):
            points.append((seg.end.x, seg.end.y))
        elif isinstance(seg, Close):
            if len(points) > 0:
                points.append(points[0])
        elif isinstance(seg, CubicBezier):
            for i in range(1, num_segments + 1):
                t = i / num_segments
                pt = seg.point(t)
                points.append((pt.x, pt.y))
        elif isinstance(seg, QuadraticBezier):
            for i in range(1, num_segments + 1):
                t = i / num_segments
                pt = seg.point(t)
                points.append((pt.x, pt.y))
        elif isinstance(seg, Arc):
            for i in range(1, num_segments + 1):
                t = i / num_segments
                pt = seg.point(t)
                points.append((pt.x, pt.y))
    return points

def render_paths_to_image(paths, size, svg_width=256, svg_height=256):
    """Render paths to a PIL RGBA image at given size."""
    scale_x = size / svg_width
    scale_y = size / svg_height

    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    for path in paths:
        points = path_to_polygon_points(path, num_segments=200)
        if len(points) < 3:
            continue
        # Scale points
        scaled = [(x * scale_x, y * scale_y) for x, y in points]
        # Fill with UI_ACCENT color (0x0DBFC0 = R=13, G=191, B=192)
        draw.polygon(scaled, fill=(13, 191, 192, 255))

    return img

def write_argb8888(img, outpath):
    """Write PIL RGBA image as ARGB8888 binary (BGRA byte order for little-endian)."""
    w, h = img.size
    pixels = img.load()
    with open(outpath, 'wb') as f:
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixels[x, y]
                # ARGB8888: little-endian memory layout = B, G, R, A
                f.write(bytes([b, g, r, a]))
    print(f"  Wrote {outpath}: {w}x{h}, {w*h*4} bytes")

def main():
    svg_path = r"D:\c\esp32\jhq\fan.svg"
    out_dir = r"D:\c\esp32\jhq\main\ui"

    print("Parsing SVG...")
    svg = SVG.parse(svg_path)
    width = svg.width if svg.width else 256
    height = svg.height if svg.height else 256
    print(f"SVG viewBox: {svg.viewbox}, size: {width}x{height}")

    # Collect all path elements
    all_paths = list(svg.elements())
    paths = [p for p in all_paths if isinstance(p, Path)]
    print(f"Found {len(paths)} SVG paths")

    if not paths:
        print("ERROR: No paths found in SVG")
        return 1

    # Split the main path into subpaths
    subpaths = split_subpaths(paths[0])
    print(f"Split into {len(subpaths)} subpaths")
    for i, sp in enumerate(subpaths):
        # Compute approximate bbox from polygon points
        pts = path_to_polygon_points(sp, num_segments=100)
        if pts:
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            print(f"  Subpath {i}: {len(sp)} segments, x=[{min(xs):.1f}, {max(xs):.1f}], y=[{min(ys):.1f}, {max(ys):.1f}]")

    # Classify subpaths:
    # The fan head is a large shape filling the upper portion of the image
    # The stand is two narrow shapes in the lower portion
    # Use y-center to classify: head is in upper portion, stand is lower
    head_paths = []
    stand_paths = []
    for sp in subpaths:
        pts = path_to_polygon_points(sp, num_segments=100)
        if not pts:
            continue
        ys = [p[1] for p in pts]
        y_center = (min(ys) + max(ys)) / 2
        # Head y-center is < 110 (upper portion), stand is > 110
        if y_center < 110:
            head_paths.append(sp)
        else:
            stand_paths.append(sp)

    print(f"\nHead paths: {len(head_paths)}, Stand paths: {len(stand_paths)}")

    # Render head
    print("\nRendering head image...")
    head_img = render_paths_to_image(head_paths, SIZE, width, height)
    write_argb8888(head_img, f"{out_dir}/fan_head.bin")

    # Render stand
    print("\nRendering stand image...")
    stand_img = render_paths_to_image(stand_paths, SIZE, width, height) if stand_paths else Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    write_argb8888(stand_img, f"{out_dir}/fan_stand.bin")

    print("\nDone!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
