from PIL import Image
import struct

def convert_to_lvgl_c(img_path, var_name, output_path):
    img = Image.open(img_path).convert('RGBA')
    width, height = img.size
    
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
    for y in range(height):
        row_bytes = []
        for x in range(width):
            r, g, b, a = img.getpixel((x, y))
            
            # Zkusíme BGR565 (prohozené R a B)
            # RRRRR GGGGG G BBBBB
            # Tady zkusíme prohodit r a b v masce
            rgb565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
            
            # LVGL RGB565 + A8 formát: Low Byte, High Byte, Alpha Byte
            low_byte = rgb565 & 0xFF
            high_byte = (rgb565 >> 8) & 0xFF
            
            row_bytes.extend([f'0x{low_byte:02X}', f'0x{high_byte:02X}', f'0x{a:02X}'])
        
        bytes_data.append(','.join(row_bytes) + ',')
    
    c_content.extend(bytes_data)
    c_content.append('};\n')
    
    c_content.extend([
        f'const lv_img_dsc_t {var_name} = {{',
        '  .header.always_zero = 0,',
        f'  .header.w = {width},',
        f'  .header.h = {height},',
        f'  .data_size = {len(bytes_data) * width * 3},',
        '  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,',
        f'  .data = {var_name}_data',
        '};'
    ])
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(c_content))

# Spouštíme s BGR formátem
convert_to_lvgl_c('src/images/ocix02_open_240_scaled.png', 'ui_img_ocix02_mono_png', 'src/images/ui_img_ocix02_mono_png.c')
convert_to_lvgl_c('src/images/ocix01_closed_240_scaled.png', 'ui_img_ocix01_c_png', 'src/images/ui_img_ocix01_c_png.c')
