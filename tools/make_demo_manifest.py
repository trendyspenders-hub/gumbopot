"""Generates demo pantry manifest.json - 20 CC0 slots to fill."""
import json, os

GENRES = ["cajun", "detroit", "tokyo", "lagos"]
ROLES = ["drums", "bass", "chordal", "texture"]

def main(out="manifest.json", pantry="pantry"):
    slots = []
    for g in GENRES:
        for r in ROLES:
            slots.append({"role": r, "genre": g, "bpm_range": [70, 140],
                          "file": f"{pantry}/{r}_{g}_01.wav", "source": "cc0:FILL_ME"})
    for i in range(1, 5):
        slots.append({"role": "melodic", "genre": "wildcard", "bpm_range": [70, 140],
                      "file": f"{pantry}/melodic_wild_{i:02d}.wav", "source": "cc0:FILL_ME"})
    doc = {"pantry_version": 1, "rules": "2-8 bar loops, 44.1/48k WAV, CC0 only",
           "slots": slots}
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    json.dump(doc, open(out, "w"), indent=2)
    print(f"{out}: {len(slots)} slots")

if __name__ == "__main__":
    main()
