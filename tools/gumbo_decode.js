// GUMBO POT recipe decoder - pure string math, no deps.
// Mirror of tools/daily_cook.py decode(). Runs in browser + node.
// The menu board out front: paste a gumbo:// link, see the cook.
(function (root) {
  "use strict";
  var CUISINES = ["cajun", "detroit", "tokyo", "lagos", "bristol", "flip"];

  function decode(recipe) {
    var p = String(recipe).replace("gumbo://", "").split(".");
    if (p.length !== 5 || p[0] !== "v1") throw new Error("bad recipe string");
    var cuisine = p[1];
    if (CUISINES.indexOf(cuisine) === -1) throw new Error("not on the menu");
    var seed = parseInt(p[2], 10);
    var cayenne = parseInt(p[3], 10);
    var layers = parseInt(p[4], 10);
    if (isNaN(seed) || isNaN(cayenne) || isNaN(layers)) throw new Error("bad numbers");
    return { version: 1, cuisine: cuisine, seed: seed,
             weirdness: cayenne / 100.0, layers: layers };
  }

  function describe(d) {
    var heat = d.weirdness < 0.25 ? "mild" : d.weirdness < 0.5 ? "medium"
             : d.weirdness < 0.75 ? "hot" : "five-alarm";
    return d.cuisine + " gumbo, " + d.layers + " bowls, " + heat
         + " (seed " + d.seed + ")";
  }

  var api = { decode: decode, describe: describe, CUISINES: CUISINES };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.GumboRecipe = api;
})(typeof self !== "undefined" ? self : this);
