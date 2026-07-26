# SICHARGE D - 3D Printable Enclosure

## Components Inside
| Component | Dimensions | Position |
|---|---|---|
| **ESP32 16ch Relay Board** | 165 × 90 × 25mm | Main area (left) |
| **LM2596 Step-Down DC-DC** | 44 × 21 × 14mm | Right side (top) |
| **DFRobot DAC (DFR0971)** | 35 × 27 × 12mm | Right side (bottom) |

## Enclosure Specs
- **External Size**: ~235 × 103 × 40mm
- **Wall Thickness**: 2.5mm
- **Corner Radius**: 4mm
- **Material**: PLA or PETG recommended

## Features
- ✅ 4 cable routing slots on the left (relay terminals)
- ✅ 2 cable slots on the right (power + DAC output)
- ✅ 1 USB access slot on the back
- ✅ Ventilation slots on all sides + hex pattern on lid
- ✅ 4 mounting posts for relay board (M3 screws)
- ✅ 2 mounting posts for step-down (M3 screws)
- ✅ Raised DAC platform with M2.5 mounting
- ✅ Lid with screw-down design (4x M3 self-tapping)
- ✅ SICHARGE D branding embossed on lid
- ✅ Corner gusset reinforcements

## How to Generate STL

### Option 1: OpenSCAD GUI
1. Install [OpenSCAD](https://openscad.org/downloads.html)
2. Open `enclosure.scad`
3. Set `render_part = "base"` → Render (F6) → Export STL
4. Set `render_part = "lid"` → Render (F6) → Export STL

### Option 2: Command Line
```bash
# Render base
openscad -D 'render_part="base"' -o enclosure_base.stl enclosure.scad

# Render lid
openscad -D 'render_part="lid"' -o enclosure_lid.stl enclosure.scad
```

## Print Settings
| Setting | Value |
|---|---|
| Layer Height | 0.2mm |
| Infill | 15-20% |
| Supports | None needed |
| Walls | 3 perimeters |
| Material | PLA / PETG |
| Orientation | Print base upside-down, lid flat |

## Assembly
1. Print base and lid separately
2. Mount relay board on the 4 posts with M3×8 screws
3. Mount LM2596 with M3×6 screws
4. Mount DAC on raised platform with M2.5×6 screws
5. Route cables through the side slots
6. Secure lid with 4x M3×10 self-tapping screws

## Cable Routing
```
┌─────────────────────────────────────────────┐
│                    BACK                      │
│  ┌──USB──┐  ┌──cable──┐                     │
│  │       │  │         │                     │
│  ├───────┴──┴─────────┤  ┌──────────┐       │
│  │                    │  │ LM2596   │ ←power│
│  │                    │  │ Step-Down │ ←slot │
│  │   ESP32 16ch       │  ├──────────┤       │
│  │   RELAY BOARD      │  │          │       │
│  │                    │  │ DFRobot  │ ←DAC  │
│  │                    │  │   DAC    │ ←slot │
│  │                    │  │          │       │
│  └─┤ ┤ ┤ ┤───────────┘  └──────────┘       │
│    ↑ ↑ ↑ ↑                                  │
│  4 cable slots                              │
│                   FRONT                      │
└─────────────────────────────────────────────┘
```
