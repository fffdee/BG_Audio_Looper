$files = @(
    "F:\project_and_dataset\project\BG_Audio_Looper\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth\BG_HAL\bg_log.c",
    "F:\project_and_dataset\project\BG_Audio_Looper\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth\BG_HAL\bg_storage.c",
    "F:\project_and_dataset\project\BG_Audio_Looper\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth\BG_Midi_Controller\midi_controller.c",
    "F:\project_and_dataset\project\BG_Audio_Looper\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth\BG_Midi_Controller\midi_soundbank_bridge.c",
    "F:\project_and_dataset\project\BG_Audio_Looper\BG_Audio_Looper\BanBox\src\banux\05_component\bangtsynth\BG_Soundbank\bgs_parser.c"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        $content = Get-Content $file -Raw
        
        # Check if it already has the guard
        if ($content -notmatch '#ifdef BANGTSYNTH_EN') {
            # Add guard at the beginning (after first include)
            $content = $content -replace '(^[^\n]*\n)', "#include "product_def.h"

#ifdef BANGTSYNTH_EN

$1" 
            
            # Add guard at the end
            $content = $content -replace '(\s*)$', "

#endif /* BANGTSYNTH_EN */
"
            
            Set-Content $file $content
            Write-Host "Wrapped: $file"
        } else {
            Write-Host "Already wrapped: $file"
        }
    }
}
