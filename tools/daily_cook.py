"""Daily Cook: date-derived recipe string. Same day = same gumbo worldwide.
Offline, zero server."""
import datetime

CUISINES = ["cajun", "detroit", "tokyo", "lagos", "bristol", "cajun"]  # sunday=cajun

def recipe_for(date=None):
    d = date or datetime.date.today()
    seed = d.timetuple().tm_yday * 37 + (d.year % 100)
    cuisine = CUISINES[d.weekday()]
    cayenne = (d.day * 7) % 100
    return f"gumbo://v1.{cuisine}.{seed}.{cayenne}.5"

def decode(recipe):
    """Pure string math. Returns dict or raises ValueError('not on the menu')."""
    p = recipe.replace("gumbo://", "").split(".")
    if len(p) != 5 or p[0] != "v1":
        raise ValueError("bad recipe string")
    _, cuisine, seed, cayenne, layers = p[0], p[1], int(p[2]), int(p[3]), int(p[4])
    if cuisine not in ("cajun", "detroit", "tokyo", "lagos", "bristol", "flip"):
        raise ValueError("not on the menu")
    return {"version": 1, "cuisine": cuisine, "seed": seed,
            "weirdness": cayenne / 100.0, "layers": layers}
