#include "compressed_data.h"
#include <miniz/miniz.h>

#ifdef __cplusplus
extern "C" {
#endif

bool decompress_all_data() {
    if(!Items_decompress())
        return false;
    
    if(!Recipes_decompress())
        return false;
    
    if(!Textures_decompress())
        return false;
    
    return true;
}

#ifdef __cplusplus
}
#endif
