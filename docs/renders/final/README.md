# Final renders (v1.0)

Renderings of the CAD model, not photographs of a physical product.

Final assembled device (sender and receiver are the same hardware): case from `mechanics/doorbell-case.FCStd`
(Foot: 9 mm seat pocket, back wall with J cable slot) with the DBN-MAIN board from
`electronics/dbn-main/dbn-main.kicad_pcb` (KiCad component models, aligned to the board body in the case model),
Diffuser and Plunger. Rendered with Blender 5.2.2 Cycles, 1920x1080, 192 samples, softbox light.

| file | view |
|---|---|
| 01-hero.png | iso front-right |
| 02-back-j-slot.png | iso back-left, J cable slot in the Foot back wall |
| 03-front.png | front: diffuser ring (LED) and button |
| 04-side-profile.png | side profile in the Foot |
| 05-detail-usb-c-j-slot.png | close-up from behind, Foot shown semi-transparent: USB-C plug in the case, cable hooked in the J pocket |

Colours are a render choice (case slate blue, Foot charcoal, natural-white diffuser/plunger), not a material spec.
The USB-C cable in 05 is a render prop (20 mm overmold, Ø4.5 cable, checked
for 0.0 mm³ interference with every part). The render source script is not part of this release.
