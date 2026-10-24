"""Generate a TouchOSC template XML for the shelf controls from the param map."""
import xml.etree.ElementTree as ET

PARAMS = [("master","STOVE TEMP"),("gumbo","POT DENSITY"),("weirdness","CAYENNE"),
          ("seed","RECIPE #"),("layer1gain","L1 SCOOP"),("layer1pan","L1 PLACE"),
          ("layer1filter","L1 STRAIN"),("layer2gain","L2 SCOOP"),("layer2pan","L2 PLACE"),
          ("layer2filter","L2 STRAIN"),("layer3gain","L3 SCOOP"),("layer3pan","L3 PLACE"),
          ("layer3filter","L3 STRAIN"),("layer4gain","L4 SCOOP"),("layer4pan","L4 PLACE"),
          ("layer4filter","L4 STRAIN"),("layer5gain","L5 SCOOP"),("layer5pan","L5 PLACE"),
          ("layer5filter","L5 STRAIN"),("evolution","EVOLUTION"),("makeroom","MAKE ROOM")]

def main(out="gumbo.touchosc"):
    root = ET.Element("layout", version="18", mode="1", orientation="horizontal")
    tab = ET.SubElement(root, "tab", scalef="0.95")
    for i, (pid, label) in enumerate(PARAMS):
        x, y = 40 + (i % 5) * 150, 60 + (i // 5) * 190
        c = ET.SubElement(tab, "control", type="fader", x=str(x), y=str(y),
                          w="120", h="160", color="orange", label=label,
                          osc=f"/gumbo/{pid}", scalef="0.0", scalet="1.0")
    ET.ElementTree(root).write(out, encoding="utf-8", xml_declaration=True)
    print(f"{out}: {len(PARAMS)} controls")

if __name__ == "__main__":
    main()
