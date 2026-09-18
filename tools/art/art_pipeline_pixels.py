"""像素格線、量化、輪廓與接縫處理。"""

from __future__ import annotations

from typing import Sequence
from PIL import Image
from art_pipeline_palette import Rgb, _BAYER_4X4


def _remove_background(image: Image.Image, background: Rgb | None, tolerance: int) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels: list[tuple[int, int, int, int]] = []
    limit_squared = tolerance * tolerance
    for red, green, blue, alpha in rgba.get_flattened_data():
        if background is not None:
            distance_squared = sum((channel - target) ** 2 for channel, target in zip((red, green, blue), background))
            if distance_squared <= limit_squared:
                alpha = 0
        clean_alpha = 255 if alpha >= 128 else 0
        pixels.append((red, green, blue, clean_alpha) if clean_alpha else (0, 0, 0, 0))
    rgba.putdata(pixels)
    return rgba


def crop_align(
    image: Image.Image,
    size: tuple[int, int],
    kind: str,
    background: Rgb | None,
    background_tolerance: int,
) -> Image.Image:
    rgba = _remove_background(image, background, background_tolerance)
    alpha = rgba.getchannel("A")
    box = alpha.getbbox()
    if box is None:
        raise ValueError("去背後沒有任何可見像素")
    cropped = rgba.crop(box)
    if kind == "terrain":
        return cropped.resize(size, Image.Resampling.NEAREST)
    scale = min(size[0] / cropped.width, size[1] / cropped.height, 1.0)
    resized_size = (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale)))
    resized = cropped.resize(resized_size, Image.Resampling.NEAREST)
    aligned = Image.new("RGBA", size, (0, 0, 0, 0))
    offset = ((size[0] - resized.width) // 2, (size[1] - resized.height) // 2)
    aligned.alpha_composite(resized, offset)
    return aligned


def _nearest_palette_color(color: Rgb, palette: Sequence[Rgb]) -> Rgb:
    return min(
        palette,
        key=lambda candidate: (
            (color[0] - candidate[0]) ** 2
            + (color[1] - candidate[1]) ** 2
            + (color[2] - candidate[2]) ** 2
        ),
    )


def quantize_ordered(image: Image.Image, palette: Sequence[Rgb], strength: int = 16) -> Image.Image:
    if not palette:
        raise ValueError("色盤不可為空")
    if strength < 0 or strength > 64:
        raise ValueError("抖動強度必須介於 0 與 64")
    source = image.convert("RGBA")
    output = Image.new("RGBA", source.size)
    result: list[tuple[int, int, int, int]] = []
    for y in range(source.height):
        for x in range(source.width):
            red, green, blue, alpha = source.getpixel((x, y))
            if alpha == 0:
                result.append((0, 0, 0, 0))
                continue
            matrix_value = _BAYER_4X4[y % 4][x % 4]
            offset = ((matrix_value * 2 - 15) * strength) // 16
            adjusted = tuple(max(0, min(255, channel + offset)) for channel in (red, green, blue))
            result.append((*_nearest_palette_color(adjusted, palette), alpha))
    output.putdata(result)
    return output


def _opaque_boundary(mask: Sequence[bool], width: int, height: int) -> list[bool]:
    boundary = [False] * (width * height)
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if not mask[index]:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or nx >= width or ny < 0 or ny >= height or not mask[ny * width + nx]:
                    boundary[index] = True
                    break
    return boundary


def normalize_outline(image: Image.Image, kind: str, outline_color: Rgb, line_width: int = 1) -> Image.Image:
    if line_width < 1:
        raise ValueError("輪廓線寬必須至少為 1")
    rgba = image.convert("RGBA")
    pixels = list(rgba.get_flattened_data())
    mask = [pixel[3] > 0 for pixel in pixels]
    layers: list[list[bool]] = []
    remaining = mask.copy()
    for _ in range(line_width):
        layer = _opaque_boundary(remaining, rgba.width, rgba.height)
        layers.append(layer)
        remaining = [visible and not edge for visible, edge in zip(remaining, layer)]
    boundary = [any(layer[index] for layer in layers) for index in range(len(mask))]
    output = pixels.copy()
    if kind == "object":
        for index, is_boundary in enumerate(boundary):
            if is_boundary:
                output[index] = (*outline_color, 255)
    else:
        interior_indices = [index for index, visible in enumerate(remaining) if visible]
        for index, is_boundary in enumerate(boundary):
            if not is_boundary or not interior_indices:
                continue
            x, y = index % rgba.width, index // rgba.width
            nearest = min(
                interior_indices,
                key=lambda candidate: (
                    abs(x - candidate % rgba.width) + abs(y - candidate // rgba.width),
                    candidate,
                ),
            )
            output[index] = (*pixels[nearest][:3], pixels[index][3])
    rgba.putdata(output)
    return rgba


def seam_metric(image: Image.Image) -> float:
    rgba = image.convert("RGBA")
    difference = 0
    comparisons = 0
    for y in range(rgba.height):
        left = rgba.getpixel((0, y))
        right = rgba.getpixel((rgba.width - 1, y))
        difference += sum(abs(a - b) for a, b in zip(left, right))
        comparisons += 4
    for x in range(rgba.width):
        top = rgba.getpixel((x, 0))
        bottom = rgba.getpixel((x, rgba.height - 1))
        difference += sum(abs(a - b) for a, b in zip(top, bottom))
        comparisons += 4
    return difference / comparisons if comparisons else 0.0


def repair_seams(image: Image.Image, palette: Sequence[Rgb], seam_width: int = 1) -> Image.Image:
    if seam_width < 1 or seam_width * 2 > min(image.size):
        raise ValueError("接縫修補寬度超出影像範圍")
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    for offset in range(seam_width):
        opposite_x = rgba.width - 1 - offset
        for y in range(rgba.height):
            left = pixels[offset, y]
            right = pixels[opposite_x, y]
            average = tuple((left[channel] + right[channel]) // 2 for channel in range(3))
            color = _nearest_palette_color(average, palette)
            alpha = 0 if left[3] == 0 and right[3] == 0 else 255
            repaired = (0, 0, 0, 0) if alpha == 0 else (*color, alpha)
            pixels[offset, y] = repaired
            pixels[opposite_x, y] = repaired
        opposite_y = rgba.height - 1 - offset
        for x in range(rgba.width):
            top = pixels[x, offset]
            bottom = pixels[x, opposite_y]
            average = tuple((top[channel] + bottom[channel]) // 2 for channel in range(3))
            color = _nearest_palette_color(average, palette)
            alpha = 0 if top[3] == 0 and bottom[3] == 0 else 255
            repaired = (0, 0, 0, 0) if alpha == 0 else (*color, alpha)
            pixels[x, offset] = repaired
            pixels[x, opposite_y] = repaired
    return rgba
