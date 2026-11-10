.include "hdr.asm"
.section ".rodata1" superfree
bg_tiles: .incbin "build/tiles.pic"
bg_tiles_end:
bg_palette: .incbin "build/tiles.pal"
bg_palette_end:
sprite_tiles: .incbin "build/sprites.pic"
sprite_tiles_end:
sprite_palette: .incbin "build/sprites.pal"
sprite_palette_end:
type_brr: .incbin "build/type.brr"
type_brr_end:
error_brr: .incbin "build/error.brr"
error_brr_end:
clear_brr: .incbin "build/clear.brr"
clear_brr_end:
.ends
