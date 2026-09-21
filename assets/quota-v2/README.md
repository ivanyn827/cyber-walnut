# Approved mascot artwork / v2

Created with the built-in imagegen tool using the original approved image as the identity reference, not the CLI/API fallback. Source files are preserved; the firmware renders indexed raster art, never procedural replacement characters.

- `approved-reference.png`: user-approved original concept.
- `mascot-atlas.png`: final imagegen output, 1536×1024, 3×2 grid, last cell empty.
- `../../tools/compile_quota_art.py`: deterministic atlas slicing, scaling and RGB565/indexed conversion for the device.
- `../../firmware/salary_counter/quota_sprite.h`: subpixel breathing, sway, bounce and regional arm motion using the actual artwork. No alternate flat character drawings.

## Generation prompt

Use case: identity-preserve. Production sprite atlas for a 480x480 embedded screen. Reference image is the APPROVED design: faithfully reproduce its EXACT five creamy ivory soft 3D clay marshmallow mascots, including original silhouette, tiny feet, thick rounded arms, glossy black eyes, brows, little mouth, blush, smooth studio shading, mint/cyan/yellow/orange/lavender lighting. Do NOT reinterpret as flat vector, ellipse, emoji or a new character. Output one 1536x1024 atlas arranged in an exact 3-column by 2-row uniform grid, each cell 512x512. Entire background solid uniform #161719, no transparency needed, no UI, no words, no numbers, no card borders. Top row left: reference stage 80-100%, upright confident blob flexing BOTH muscular little arms with starry glossy eyes, cheerful open mouth, mint rim lighting and small mint sparkle. Top middle: reference 60-79%, smiling crescent eyes, open happy mouth, one thumbs-up, cyan rim lighting. Top right: reference 40-59%, upright round blob with two oval black eyes, raised brows, tiny straight mouth, both arms hanging, warm yellow rim. Bottom left: reference 20-39%, slumped blob, drooping eyelids, worried eyebrows, sweat drop, limp arms, peach-orange rim. Bottom middle: reference 0-19%, fully deflated wide puddle with flattened arms, exhausted eyelids and wavy mouth, lavender rim, three small floating fatigue strokes. Bottom right completely EMPTY solid #161719. Each character should occupy at most 380px wide and 360px tall, horizontally centered in its cell, feet/base resting at cell y=425. Keep the collapsed character low and wide, don't enlarge it vertically. Maintain reference anatomy and precise personality. Soft contact shadow directly underneath, no large background glow; backdrop outside the subject remains exact #161719. This atlas is the actual shipped art, preserve the approved design meticulously.

Motion preview and stills in `../../evidence/quota-art-*` are produced by the real C++ renderer, not by imagegen or a separately drawn UI.

Motion amplitude update: lateral sway ±4/7/10/13/16 px (low→high quota), arm source warp ±3/4.5/6/7.5/9 px, normal-stage vertical motion ±5 px and high-stage bounce 8 px. Breathing deformation increased about 2.5–3×. The maximum vertical scale is capped to protect the header; original raster artwork is unchanged.
