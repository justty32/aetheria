"""主光偵測、方向正規化與警告。"""

from __future__ import annotations

from PIL import Image


def _luminance(pixel: tuple[int, int, int, int]) -> int:
    return 54 * pixel[0] + 183 * pixel[1] + 19 * pixel[2]


def detect_light_direction(image: Image.Image) -> str:
    rgba = image.convert("RGBA")
    gradient_x = 0
    gradient_y = 0
    samples = 0
    for y in range(1, rgba.height - 1):
        for x in range(1, rgba.width - 1):
            left = rgba.getpixel((x - 1, y))
            right = rgba.getpixel((x + 1, y))
            top = rgba.getpixel((x, y - 1))
            bottom = rgba.getpixel((x, y + 1))
            if min(left[3], right[3], top[3], bottom[3]) == 0:
                continue
            gradient_x += _luminance(right) - _luminance(left)
            gradient_y += _luminance(bottom) - _luminance(top)
            samples += 1
    if samples == 0 or (gradient_x == 0 and gradient_y == 0):
        return "undetermined"
    horizontal = "right" if gradient_x > 0 else "left"
    vertical = "bottom" if gradient_y > 0 else "top"
    return f"{vertical}-{horizontal}"


def normalize_lighting(image: Image.Image, kind: str) -> tuple[Image.Image, str, int]:
    if kind not in ("terrain", "object"):
        raise ValueError(f"未知素材種類 {kind!r}")
    direction = detect_light_direction(image)
    if kind == "object":
        return image.copy(), direction, 0
    rotations = {
        "top-left": (None, 0),
        "top-right": (Image.Transpose.ROTATE_90, 90),
        "bottom-right": (Image.Transpose.ROTATE_180, 180),
        "bottom-left": (Image.Transpose.ROTATE_270, 270),
        "undetermined": (None, 0),
    }
    transpose, degrees = rotations[direction]
    normalized = image.copy() if transpose is None else image.transpose(transpose)
    if normalized.size != image.size:
        scale = min(image.width / normalized.width, image.height / normalized.height)
        resized = normalized.resize(
            (max(1, int(normalized.width * scale)), max(1, int(normalized.height * scale))),
            Image.Resampling.NEAREST,
        )
        canvas = Image.new("RGBA", image.size, (0, 0, 0, 0))
        canvas.alpha_composite(
            resized,
            ((image.width - resized.width) // 2, (image.height - resized.height) // 2),
        )
        normalized = canvas
    return normalized, direction, degrees


def object_light_warning(label: str, direction: str) -> str | None:
    if direction == "top-left":
        return None
    return f"{label}: 主光方向 {direction}（object 僅警告、不旋轉）"
