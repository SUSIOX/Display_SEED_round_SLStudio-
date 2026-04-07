from PIL import Image
import struct

def convert_with_shift(img_path, var_name, output_path, shift_y):
    img = Image.open(img_path).convert('RGB')
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
        row = []
        # Posuneme index čtení: pro y=0 budeme číst y=11 (tím se oko posune nahoru)
        read_y = y + shift_y
        # Pokud bychom vyjeli z obrázku, budeme opakovat poslední řádek (kůži)
        if read_y >= height:
            read_y = height - 1
        if read_y < 0:
            read_y = 0
            
        for x in range(width):
            r, g, b = img.getpixel((x, read_y))
            # RGB565 conversion
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Big Endian: High Byte, Low Byte
            row.append(f"0x{(rgb565 >> 8) & 0xFF:02X},0x{rgb565 & 0xFF:02X}")
        bytes_data.append("    " + ",".join(row) + ",")
    
    c_content.extend(bytes_data)
    c_content.append('    };')
    
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

# Posun o 11px nahoru (shift_y = 11)
convert_with_shift('src/images/ocix02_open_240_scaled.png', 'ui_img_ocix02_mono_png', 'src/images/ui_img_ocix02_mono_png.c', 11)
convert_with_shift('src/images/ocix01_closed_240_scaled.png', 'ui_img_ocix01_c_png', 'src/images/ui_img_ocix01_c_png.c', 11)
