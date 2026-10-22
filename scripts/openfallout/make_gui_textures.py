#!/usr/bin/env python3
"""Writes the interface textures that the MyGUI skins of OpenFallout name and that the Fallout games do not have.

The skins and layouts under files/data/mygui were made for Morrowind and look for `textures/menu_*.dds`, `tx_menubook*.dds`,
the mouse pointers and the like in the Morrowind archives. Fallout 3 and New Vegas have none of them, so every frame, button
and pointer came out as a missing image. The images made here are plain: thin lines in a green that the Pip-Boy of Fallout 3
uses, on a dark panel; they are the work of this script and of nobody else, so the repository can carry them. They are
uncompressed 32 bit DDS files without mip maps, written to files/data/textures/ (and files/data/icons/ for the few that
the interface looks for there).

    python3 scripts/openfallout/make_gui_textures.py [directory of the data files]

The main menu of Morrowind is a picture and seven buttons whose names are written into their images; the games have none of
them, so the buttons are made here with their names in a plain letter shape of 5 by 7 dots.

The sizes follow the skins: the border pieces are 2 pixels (the thin and the head block) or 4 pixels (the thick border and
the frame of a button) across, a piece that tiles along an edge is 8 or 16 pixels long, and a button image has the size that
its layout gives it.
"""
import math
import os
import struct
import sys

ACCENT = (26, 255, 128)
DIM = (13, 128, 64)
PANEL = (6, 26, 14)


def with_alpha(color, alpha):
    return (color[0], color[1], color[2], alpha)


class Image:
    def __init__(self, width, height, fill=(0, 0, 0, 0)):
        self.width = width
        self.height = height
        self.pixels = [fill] * (width * height)

    def set(self, x, y, color):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[y * self.width + x] = color

    def get(self, x, y):
        return self.pixels[y * self.width + x]

    def blend(self, x, y, color):
        """Puts color over the pixel (straight alpha)."""
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        under = self.get(x, y)
        a = color[3] / 255.0
        ua = under[3] / 255.0
        out_a = a + ua * (1 - a)
        if out_a <= 0:
            self.set(x, y, (0, 0, 0, 0))
            return
        out = tuple(int(round((color[i] * a + under[i] * ua * (1 - a)) / out_a)) for i in range(3))
        self.set(x, y, out + (int(round(out_a * 255)),))

    def rect(self, x0, y0, x1, y1, color):
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.blend(x, y, color)

    def line(self, x0, y0, x1, y1, color, thickness=1):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1) * 2
        half = thickness / 2.0
        for i in range(steps + 1):
            t = i / steps
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for dy in range(-int(half), int(math.ceil(half)) + 1):
                for dx in range(-int(half), int(math.ceil(half)) + 1):
                    self.blend(int(round(x)) + dx, int(round(y)) + dy, color)

    def polygon(self, points, color):
        """Fills a polygon (even-odd) given as pixel coordinates."""
        for y in range(self.height):
            for x in range(self.width):
                inside = False
                j = len(points) - 1
                for i in range(len(points)):
                    xi, yi = points[i]
                    xj, yj = points[j]
                    if (yi > y + 0.5) != (yj > y + 0.5):
                        cross = (xj - xi) * (y + 0.5 - yi) / (yj - yi) + xi
                        if x + 0.5 < cross:
                            inside = not inside
                    j = i
                if inside:
                    self.blend(x, y, color)

    def to_dds(self):
        header = bytearray(128)
        header[0:4] = b'DDS '
        # dwSize, dwFlags (caps, height, width, pitch, pixelformat), height, width, pitch
        struct.pack_into('<7I', header, 4, 124, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000, self.height, self.width,
                         self.width * 4, 0, 0)
        # dwPFSize, dwPFFlags (alpha pixels, rgb), fourcc, bit count, masks (R, G, B, A)
        struct.pack_into('<8I', header, 76, 32, 0x1 | 0x40, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
        # dwCaps (texture)
        struct.pack_into('<I', header, 108, 0x1000)
        body = bytearray()
        for r, g, b, a in self.pixels:
            body += bytes((b, g, r, a))
        return bytes(header) + bytes(body)


def frame_color(depth, levels):
    """The colour of the pixel `depth` pixels in from the outer edge of a frame; `levels` lists (colour, alpha)."""
    if depth < len(levels):
        return levels[depth]
    return (0, 0, 0, 0)


def border_pieces(prefix, thickness, levels, length, names):
    """The eight pieces of a frame: the four edges (`length` along the edge) and the four corners."""
    out = {}
    t = thickness

    def build(w, h, depth_of):
        image = Image(w, h)
        for y in range(h):
            for x in range(w):
                image.set(x, y, frame_color(depth_of(x, y), levels))
        return image

    out[prefix + names['top']] = build(length, t, lambda x, y: y)
    out[prefix + names['bottom']] = build(length, t, lambda x, y: t - 1 - y)
    out[prefix + names['left']] = build(t, length, lambda x, y: x)
    out[prefix + names['right']] = build(t, length, lambda x, y: t - 1 - x)
    out[prefix + names['top_left']] = build(t, t, lambda x, y: min(x, y))
    out[prefix + names['top_right']] = build(t, t, lambda x, y: min(t - 1 - x, y))
    out[prefix + names['bottom_left']] = build(t, t, lambda x, y: min(x, t - 1 - y))
    out[prefix + names['bottom_right']] = build(t, t, lambda x, y: min(t - 1 - x, t - 1 - y))
    return out


CORNER_NAMES = dict(top='top', bottom='bottom', left='left', right='right', top_left='top_left_corner',
                    top_right='top_right_corner', bottom_left='bottom_left_corner', bottom_right='bottom_right_corner')
SHORT_NAMES = dict(top='top', bottom='bottom', left='left', right='right', top_left='top_left',
                   top_right='top_right', bottom_left='bottom_left', bottom_right='bottom_right')


# The letters of the names on the buttons and of the title of the main menu, 5 dots across and 7 down
FONT = {
    'A': ('.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'),
    'C': ('.###.', '#...#', '#....', '#....', '#....', '#...#', '.###.'),
    'D': ('####.', '#...#', '#...#', '#...#', '#...#', '#...#', '####.'),
    'E': ('#####', '#....', '#....', '####.', '#....', '#....', '#####'),
    'F': ('#####', '#....', '#....', '####.', '#....', '#....', '#....'),
    'G': ('.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.'),
    'I': ('#####', '..#..', '..#..', '..#..', '..#..', '..#..', '#####'),
    'L': ('#....', '#....', '#....', '#....', '#....', '#....', '#####'),
    'M': ('#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'),
    'N': ('#...#', '##..#', '##..#', '#.#.#', '#..##', '#..##', '#...#'),
    'O': ('.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'),
    'P': ('####.', '#...#', '#...#', '####.', '#....', '#....', '#....'),
    'R': ('####.', '#...#', '#...#', '####.', '#.#..', '#..#.', '#...#'),
    'S': ('.####', '#....', '#....', '.###.', '....#', '....#', '####.'),
    'T': ('#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'),
    'U': ('#...#', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'),
    'V': ('#...#', '#...#', '#...#', '#...#', '#...#', '.#.#.', '..#..'),
    'W': ('#...#', '#...#', '#...#', '#.#.#', '#.#.#', '##.##', '#...#'),
    'X': ('#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#'),
}


def text_width(text, scale):
    return len(text) * 6 * scale - scale


def draw_text(image, x, y, text, scale, color):
    """Draws the text in capital letters with its top left corner at x, y; a letter is 5 dots across and 7 down."""
    for index, letter in enumerate(text):
        rows = FONT.get(letter)
        if rows is None:
            continue
        for row, dots in enumerate(rows):
            for column, dot in enumerate(dots):
                if dot == '#':
                    image.rect(x + (index * 6 + column) * scale, y + row * scale,
                               x + (index * 6 + column + 1) * scale, y + (row + 1) * scale, color)


def solid(w, h, color):
    return Image(w, h, color)


def arrow_pointer():
    """The mouse pointer: an arrow with its tip at the top left corner, 32 x 32."""
    image = Image(32, 32)
    outline = [(0, 0), (0, 21), (5, 16), (9, 25), (13, 23), (9, 15), (16, 15)]
    image.polygon(outline, with_alpha(ACCENT, 255))
    inner = [(2, 5), (2, 16), (5, 13), (10, 22), (11, 21), (7, 12), (11, 12)]
    image.polygon(inner, with_alpha(PANEL, 255))
    return image


def move_pointer():
    """The pointer for moving and resizing: four arrows from a point."""
    image = Image(32, 32)
    c = 15
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        image.line(c, c, c + dx * 12, c + dy * 12, with_alpha(ACCENT, 255), 2)
        tip_x, tip_y = c + dx * 13, c + dy * 13
        image.polygon([(tip_x, tip_y), (tip_x - dx * 5 + dy * 4, tip_y - dy * 5 + dx * 4),
                       (tip_x - dx * 5 - dy * 4, tip_y - dy * 5 - dx * 4)], with_alpha(ACCENT, 255))
    return image


def drop_pointer():
    """The pointer for putting an item down on the ground: an arrow pointing down onto a line."""
    image = Image(32, 32)
    image.line(15, 2, 15, 20, with_alpha(ACCENT, 255), 3)
    image.polygon([(15, 28), (6, 17), (24, 17)], with_alpha(ACCENT, 255))
    image.line(3, 30, 28, 30, with_alpha(ACCENT, 255), 2)
    return image


def crosshair():
    image = Image(27, 27)
    green = with_alpha(ACCENT, 230)
    for d in range(3, 11):
        image.blend(13, 13 - d, green)
        image.blend(13, 13 + d, green)
        image.blend(13 - d, 13, green)
        image.blend(13 + d, 13, green)
    image.blend(13, 13, green)
    return image


def compass_image():
    image = Image(32, 32)
    for y in range(32):
        for x in range(32):
            r = math.hypot(x - 15.5, y - 15.5)
            if 13.5 <= r <= 15.5:
                image.set(x, y, with_alpha(ACCENT, 220))
            elif r < 13.5:
                image.set(x, y, with_alpha(PANEL, 160))
    image.polygon([(15.5, 3), (12, 15.5), (19, 15.5)], with_alpha(ACCENT, 255))
    return image


def panel(w, h, border=2):
    image = Image(w, h, with_alpha(PANEL, 215))
    for d in range(border):
        a = 255 if d == 0 else 150
        image.rect(d, d, w - d, d + 1, with_alpha(ACCENT, a))
        image.rect(d, h - d - 1, w - d, h - d, with_alpha(ACCENT, a))
        image.rect(d, d, d + 1, h - d, with_alpha(ACCENT, a))
        image.rect(w - d - 1, d, w - d, h - d, with_alpha(ACCENT, a))
    return image


def glyph_button(w, h, state, glyph):
    """A button image of a book or a scroll: a frame, and a sign for what it does; `state` is idle, over or pressed."""
    fill = {'idle': with_alpha(PANEL, 215), 'over': with_alpha((12, 64, 34), 230), 'pressed': with_alpha(DIM, 255)}[state]
    edge = {'idle': 150, 'over': 255, 'pressed': 255}[state]
    image = Image(w, h, fill)
    image.rect(0, 0, w, 1, with_alpha(ACCENT, edge))
    image.rect(0, h - 1, w, h, with_alpha(ACCENT, edge))
    image.rect(0, 0, 1, h, with_alpha(ACCENT, edge))
    image.rect(w - 1, 0, w, h, with_alpha(ACCENT, edge))
    sign = with_alpha(ACCENT, 255 if state != 'idle' else 220)
    cx, cy = w // 2, h // 2
    s = min(h // 2 - 4, 8)
    if glyph == 'prev':
        image.polygon([(cx - s, cy), (cx + s // 2, cy - s), (cx + s // 2, cy + s)], sign)
    elif glyph == 'next':
        image.polygon([(cx + s, cy), (cx - s // 2, cy - s), (cx - s // 2, cy + s)], sign)
    elif glyph == 'close':
        image.line(cx - s, cy - s, cx + s, cy + s, sign, 2)
        image.line(cx - s, cy + s, cx + s, cy - s, sign, 2)
    elif glyph == 'take':
        image.line(cx, cy - s, cx, cy + s - 3, sign, 2)
        image.polygon([(cx, cy + s), (cx - s, cy + 1), (cx + s, cy + 1)], sign)
    elif glyph == 'topics':
        for dy in (-s + 1, 0, s - 1):
            image.line(cx - s, cy + dy, cx + s, cy + dy, sign, 2)
    elif glyph == 'journal':
        image.rect(cx - s, cy - s, cx + s, cy + s, sign)
        image.rect(cx - s + 2, cy - s + 2, cx + s - 2, cy + s - 2, fill)
    elif glyph == 'cancel':
        image.line(cx - s, cy - s, cx + s, cy + s, sign, 2)
        image.line(cx - s, cy + s, cx + s, cy - s, sign, 2)
    return image


MENU_BUTTONS = (('return', 'RETURN'), ('newgame', 'NEW GAME'), ('savegame', 'SAVE GAME'), ('loadgame', 'LOAD GAME'),
                ('options', 'OPTIONS'), ('credits', 'CREDITS'), ('exitgame', 'EXIT'))


def menu_button(label, state):
    """A button of the main menu: its name in capital letters, in the top 48 dots of an image 192 across and 64 down (the
    menu cuts off the lower 16); lit when the pointer is over it and filled when it is pressed."""
    image = Image(192, 64)
    scale = 3
    width = text_width(label, scale)
    x, y = (192 - width) // 2, (48 - 7 * scale) // 2
    if state == 'idle':
        draw_text(image, x, y, label, scale, with_alpha(ACCENT, 210))
    elif state == 'over':
        image.rect(4, 4, 188, 44, with_alpha(PANEL, 200))
        image.rect(4, 4, 188, 5, with_alpha(ACCENT, 255))
        image.rect(4, 43, 188, 44, with_alpha(ACCENT, 255))
        draw_text(image, x, y, label, scale, with_alpha(ACCENT, 255))
    else:
        image.rect(4, 4, 188, 44, with_alpha(ACCENT, 255))
        draw_text(image, x, y, label, scale, with_alpha(PANEL, 255))
    return image


def menu_background():
    """The picture behind the main menu: a dark panel that is lighter in the middle, with a frame and the name of the engine."""
    width, height = 384, 216
    image = Image(width, height)
    for y in range(height):
        for x in range(width):
            d = math.hypot((x - width / 2) / (width / 2), (y - height / 2) / (height / 2))
            shade = max(0.0, 1.0 - d * 0.8)
            image.set(x, y, (int(4 + 10 * shade), int(14 + 34 * shade), int(8 + 20 * shade), 255))
    for d in range(3):
        a = 255 if d == 0 else 120
        image.rect(8 + d, 8 + d, width - 8 - d, 9 + d, with_alpha(ACCENT, a))
        image.rect(8 + d, height - 9 - d, width - 8 - d, height - 8 - d, with_alpha(ACCENT, a))
        image.rect(8 + d, 8 + d, 9 + d, height - 8 - d, with_alpha(ACCENT, a))
        image.rect(width - 9 - d, 8 + d, width - 8 - d, height - 8 - d, with_alpha(ACCENT, a))
    title = 'OPENFALLOUT'
    scale = 4
    draw_text(image, (width - text_width(title, scale)) // 2, 40, title, scale, with_alpha(ACCENT, 255))
    return image


def sneak_icon():
    """The sign of sneaking in the HUD: an eye, 32 x 32."""
    image = Image(32, 32)
    for x in range(2, 30):
        t = (x - 15.5) / 13.5
        half = 8.0 * max(0.0, 1.0 - t * t)
        for y in range(32):
            if abs(y - 15.5) <= half:
                edge = half - abs(y - 15.5) < 1.5
                image.set(x, y, with_alpha(ACCENT, 255) if edge else with_alpha(PANEL, 215))
    for y in range(32):
        for x in range(32):
            r = math.hypot(x - 15.5, y - 15.5)
            if r <= 4.5:
                image.set(x, y, with_alpha(ACCENT, 255))
    return image


def fist_icon():
    """The sign of fighting with the hands, for the HUD and the quick keys: a fist, 32 x 32."""
    image = Image(32, 32)
    green = with_alpha(ACCENT, 255)
    for index in range(4):
        left = 5 + index * 5
        image.rect(left, 6, left + 4, 16, green)
    image.rect(5, 16, 25, 26, green)
    image.rect(2, 14, 7, 22, green)
    image.rect(7, 18, 23, 24, with_alpha(PANEL, 255))
    return image


def coin_icon():
    """A bottle cap for the money: a ring with a dot in it, 16 x 16."""
    image = Image(16, 16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x - 7.5, y - 7.5)
            if r <= 7.5:
                image.set(x, y, with_alpha(ACCENT, 255) if r > 5.5 or r < 2.5 else with_alpha(DIM, 255))
    return image


def hit_flash():
    """The flash on the screen when the player is hurt: a soft red patch that fades out towards its edge, 128 x 128. The
    interface shows it in the middle of the screen when there is no bm_player_hit_01.dds."""
    image = Image(128, 128)
    for y in range(128):
        for x in range(128):
            d = math.hypot(x - 63.5, y - 63.5) / 64.0
            alpha = int(max(0.0, 1.0 - d) ** 1.3 * 190)
            image.set(x, y, (200, 20, 20, alpha))
    return image


def build_all():
    images = {}

    # the frame of the boxes: thin (2), the border of windows (4), the frame of a button (4)
    images.update(border_pieces('menu_thin_border_', 2, [with_alpha(ACCENT, 200), with_alpha(ACCENT, 90)], 8, CORNER_NAMES))
    images.update(border_pieces('menu_thick_border_', 4,
                                [with_alpha(ACCENT, 255), with_alpha(ACCENT, 255), with_alpha(DIM, 200),
                                 with_alpha(DIM, 110)], 8, CORNER_NAMES))
    images.update(border_pieces('menu_button_frame_', 4,
                                [with_alpha(ACCENT, 255), with_alpha(ACCENT, 130), with_alpha(DIM, 90),
                                 with_alpha(DIM, 50)], 8, CORNER_NAMES))

    # the title bar of a window
    images['menu_head_block_middle'] = solid(256, 16, with_alpha((8, 48, 26), 230))
    images.update(border_pieces('menu_head_block_', 2, [with_alpha(ACCENT, 255), with_alpha(ACCENT, 120)], 8,
                                CORNER_NAMES))
    images['menu_head_block_top'] = border_pieces('x', 2, [with_alpha(ACCENT, 255), with_alpha(ACCENT, 120)], 256,
                                                  CORNER_NAMES)['xtop']
    images['menu_head_block_bottom'] = border_pieces('x', 2, [with_alpha(ACCENT, 255), with_alpha(ACCENT, 120)], 256,
                                                     CORNER_NAMES)['xbottom']
    images['menu_head_block_left'] = border_pieces('x', 2, [with_alpha(ACCENT, 255), with_alpha(ACCENT, 120)], 16,
                                                   CORNER_NAMES)['xleft']
    images['menu_head_block_right'] = border_pieces('x', 2, [with_alpha(ACCENT, 255), with_alpha(ACCENT, 120)], 16,
                                                    CORNER_NAMES)['xright']

    # the small push button on the right of a dialogue window, up and down; the middle is 15 x 15
    for state, centre, edge in (('up', with_alpha(PANEL, 220), 200), ('down', with_alpha(DIM, 255), 255)):
        pieces = border_pieces('menu_rightbutton%s_' % state, 2, [with_alpha(ACCENT, edge), with_alpha(ACCENT, edge // 2)],
                               15, SHORT_NAMES)
        images.update(pieces)
        images['menu_rightbutton%s_center' % state] = solid(15, 15, centre)

    # the end pieces of the small bars of the HUD
    images['menu_small_energy_bar_top'] = solid(8, 2, with_alpha(ACCENT, 200))
    images['menu_small_energy_bar_bottom'] = solid(8, 2, with_alpha(ACCENT, 200))
    images['menu_small_energy_bar_vert'] = solid(2, 8, with_alpha(ACCENT, 200))
    # the track of the progress bars; the skins colour it
    gray = Image(16, 8)
    for y in range(8):
        shade = 255 - abs(y - 3.5) * 14
        for x in range(16):
            gray.set(x, y, (int(shade), int(shade), int(shade), 255))
    images['menu_bar_gray'] = gray

    # pointers, crosshair, compass, door marker
    images['tx_cursor'] = arrow_pointer()
    images['tx_cursormove'] = move_pointer()
    images['cursor_drop_ground'] = drop_pointer()
    images['target'] = crosshair()
    images['compass'] = compass_image()
    door = Image(8, 8, with_alpha(ACCENT, 255))
    door.rect(2, 2, 6, 6, with_alpha(PANEL, 255))
    images['door_icon'] = door

    # the pages and scrolls of books and notes
    # the images are stretched to the size of the window, so half the size will do
    images['tx_menubook'] = panel(364, 199, 1)
    images['scroll'] = panel(256, 186, 1)
    bookmark = panel(32, 64, 1)
    images['tx_menubook_bookmark'] = bookmark
    for name, glyph, size in (('prev', 'prev', (48, 32)), ('next', 'next', (48, 32)), ('close', 'close', (48, 32)),
                              ('take', 'take', (64, 32)), ('journal', 'journal', (64, 32)),
                              ('topics', 'topics', (56, 32)), ('cancel', 'cancel', (56, 32))):
        for state in ('idle', 'over', 'pressed'):
            images['tx_menubook_%s_%s' % (name, state)] = glyph_button(size[0], size[1], state, glyph)

    # the colour that terrain without a texture layer gets: a dry, dusty ground
    ground = Image(64, 64)
    for y in range(64):
        for x in range(64):
            n = (math.sin(x * 0.7 + y * 0.3) + math.sin(x * 0.2 - y * 0.9) + math.sin((x + y) * 0.45)) / 3.0
            base = 122 + int(n * 14)
            ground.set(x, y, (base, base - 22, base - 52, 255))
    images['_land_default'] = ground

    # the main menu: its picture and its buttons, idle, with the pointer over them and pressed
    images['menu_morrowind'] = menu_background()
    for name, label in MENU_BUTTONS:
        images['menu_' + name] = menu_button(label, 'idle')
        images['menu_%s_over' % name] = menu_button(label, 'over')
        images['menu_%s_pressed' % name] = menu_button(label, 'pressed')

    # the signs of the HUD, the money and the quick keys, which the interface looks for under icons/
    images['icons/k/stealth_sneak'] = sneak_icon()
    images['icons/k/stealth_handtohand'] = fist_icon()
    images['icons/tx_goldicon'] = coin_icon()
    images['player_hit_01'] = hit_flash()
    return images


def relative_path(name):
    """Where the image is below the data files: icons/... stay where they are, the rest is under textures/."""
    return name + '.dds' if name.startswith('icons/') else 'textures/%s.dds' % name


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', '..', 'files', 'data')
    images = build_all()
    for name in sorted(images):
        path = os.path.join(out, relative_path(name))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(images[name].to_dds())
    print('wrote %d images to %s' % (len(images), out))


if __name__ == '__main__':
    main()
