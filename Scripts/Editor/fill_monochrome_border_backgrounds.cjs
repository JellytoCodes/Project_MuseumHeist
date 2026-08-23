const fs = require("node:fs");
const path = require("node:path");
const { PNG } = require("C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pngjs");

const projectRoot = path.resolve(__dirname, "../..");
const sourceDir = path.join(projectRoot, "SourceArt/UI/Concepts/Monochrome");
const files = [
  "T_UIBorder_Monochrome_1x1_2048.png",
  "T_UIBorder_Monochrome_2x1_2048x1024.png",
  "T_UIBorder_Monochrome_1x2_1024x2048.png",
];

const fill = { r: 0x18, g: 0x17, b: 0x15, a: 0xff };

function pixelOffset(width, x, y) {
  return (y * width + x) * 4;
}

function findTransparentSeed(png) {
  const centerX = Math.floor(png.width / 2);
  const centerY = Math.floor(png.height / 2);
  const maxRadius = Math.max(png.width, png.height);

  for (let radius = 0; radius <= maxRadius; radius += 1) {
    const minX = Math.max(0, centerX - radius);
    const maxX = Math.min(png.width - 1, centerX + radius);
    const minY = Math.max(0, centerY - radius);
    const maxY = Math.min(png.height - 1, centerY + radius);

    for (let x = minX; x <= maxX; x += 1) {
      for (const y of [minY, maxY]) {
        if (png.data[pixelOffset(png.width, x, y) + 3] === 0) return { x, y };
      }
    }
    for (let y = minY + 1; y < maxY; y += 1) {
      for (const x of [minX, maxX]) {
        if (png.data[pixelOffset(png.width, x, y) + 3] === 0) return { x, y };
      }
    }
  }

  throw new Error("No transparent interior seed found");
}

function fillInterior(png) {
  const seed = findTransparentSeed(png);
  const visited = new Uint8Array(png.width * png.height);
  const queueX = new Int32Array(png.width * png.height);
  const queueY = new Int32Array(png.width * png.height);
  let head = 0;
  let tail = 0;
  let filledPixels = 0;

  queueX[tail] = seed.x;
  queueY[tail] = seed.y;
  visited[seed.y * png.width + seed.x] = 1;
  tail += 1;

  while (head < tail) {
    const x = queueX[head];
    const y = queueY[head];
    head += 1;

    const index = y * png.width + x;
    const offset = index * 4;
    if (png.data[offset + 3] !== 0) continue;

    png.data[offset] = fill.r;
    png.data[offset + 1] = fill.g;
    png.data[offset + 2] = fill.b;
    png.data[offset + 3] = fill.a;
    filledPixels += 1;

    const neighbors = [
      [x - 1, y],
      [x + 1, y],
      [x, y - 1],
      [x, y + 1],
    ];
    for (const [nextX, nextY] of neighbors) {
      if (nextX < 0 || nextX >= png.width || nextY < 0 || nextY >= png.height) continue;
      const nextIndex = nextY * png.width + nextX;
      if (visited[nextIndex] !== 0) continue;
      if (png.data[nextIndex * 4 + 3] !== 0) continue;
      visited[nextIndex] = 1;
      queueX[tail] = nextX;
      queueY[tail] = nextY;
      tail += 1;
    }
  }

  return { seed, filledPixels };
}

for (const filename of files) {
  const filePath = path.join(sourceDir, filename);
  const png = PNG.sync.read(fs.readFileSync(filePath));
  const { seed, filledPixels } = fillInterior(png);
  fs.writeFileSync(filePath, PNG.sync.write(png, { colorType: 6 }));
  console.log(JSON.stringify({
    filename,
    width: png.width,
    height: png.height,
    fill: "#181715FF",
    seed,
    filledPixels,
  }));
}
