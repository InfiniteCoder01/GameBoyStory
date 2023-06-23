import cv2
import numpy as np

def convert(from_path, to_path):
    img = cv2.imread(from_path, cv2.IMREAD_UNCHANGED)
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGBA)
    img_raw = bytearray()
    for r, g, b, a in img.reshape(-1, 4):
        if a == 0:
            rgb565 = 0xF81F
        else:
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        img_raw.extend([rgb565 >> 8, rgb565 & 0xFF])

    with open(to_path, 'wb') as f:
        f.write(np.array(img.shape[:2]).astype(np.uint16).tobytes())
        f.write(np.array(img_raw).tobytes())

images = [
    ('/mnt/Dev/Arduino/Projects/GameBoyStory/res/Mario/menu/menu.png', 'assets/menu/menu.raw'),
    ('/mnt/Dev/Arduino/Projects/GameBoyStory/res/Mario/menu/ItemFrame.png', 'assets/menu/item_frame.raw'),
    ('/mnt/Dev/Arduino/Projects/GameBoyStory/res/Mario/menu/ItemSelect.png', 'assets/menu/item_select.raw'),
]

for image, to in images:
    convert(image, to)
