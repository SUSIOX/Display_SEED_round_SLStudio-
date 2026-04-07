from PIL import Image
import struct

def convert_to_lvgl_c_16bit(img_path, var_name, output_path):
    img = Image.open(img_path).convert('RGB') # Čisté RGB bez alfy
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
            r, g, b = img.getpixel((x, y))
            
            # RGB565 formát (celkem 2 byty)
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            
            # Pořadí bytů: Low, pak High
            low_byte = rgb565 & 0xFF
            high_byte = (rgb565 >> 8) & 0xFF
            
            row_bytes.extend([f'0x{low_byte:02X}', f'0x{high_byte:02X}'])
        
        bytes_data.append(','.join(row_bytes) + ',')
    
    c_content.extend(bytes_data)
    c_content.append('};\n')
    
    c_content.extend([
        f'const lv_img_dsc_t {var_name} = {{',
        '  .header.always_zero = 0,',
        f'  .header.w = {width},',
        f'  .header.h = {height},',
        f'  .data_size = {len(bytes_data) * width * 2},',
        '  .header.cf = LV_IMG_CF_TRUE_COLOR,', # Změna na TRUE_COLOR (16-bit)
        f'  .data = {var_name}_data',
        '};'
    ])
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(c_content))

convert_to_lvgl_c_16bit('src/images/ocix02_open_240_scaled.png', 'ui_img_ocix02_mono_png', 'src/images/ui_img_ocix02_mono_png.c')
convert_to_lvgl_c_16bit('src/images/ocix01_closed_240_scaled.png', 'ui_img_ocix01_c_png', 'src/images/ui_img_ocix01_c_png.c')
