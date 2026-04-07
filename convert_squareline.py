from PIL import Image
import struct

def convert_to_squareline_style(img_path, var_name, output_path):
    img = Image.open(img_path).convert('RGB')
    width, height = img.size
    
    # SquareLine styl hlavičky
    c_content = [
        '#include "../ui.h"',
        '',
        '#ifndef LV_ATTRIBUTE_MEM_ALIGN',
        '#define LV_ATTRIBUTE_MEM_ALIGN',
        '#endif',
        '',
        f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t {var_name}_data[]  = {{'
    ]
    
    bytes_data = []
    # Generujeme data: Low byte, High byte pro každý pixel
    for y in range(height):
        row = []
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            # RGB565 conversion
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Little Endian: Low, then High
            row.append(f"0x{rgb565 & 0xFF:02X},0x{(rgb565 >> 8) & 0xFF:02X}")
        bytes_data.append("    " + ",".join(row) + ",")
    
    c_content.extend(bytes_data)
    c_content.append('    };')
    
    # Descriptor přesně podle gitu
    c_content.extend([
        f'const lv_img_dsc_t {var_name} = {{',
        '   .header.always_zero = 0,',
        f'   .header.w = {width},',
        f'   .header.h = {height},',
        f'   .data_size = sizeof({var_name}_data),',
        '   .header.cf = LV_IMG_CF_TRUE_COLOR,',
        f'   .data = {var_name}_data}};',
        ''
    ])
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(c_content))

convert_to_squareline_style('src/images/ocix02_open_240_scaled.png', 'ui_img_ocix02_mono_png', 'src/images/ui_img_ocix02_mono_png.c')
convert_to_squareline_style('src/images/ocix01_closed_240_scaled.png', 'ui_img_ocix01_c_png', 'src/images/ui_img_ocix01_c_png.c')
