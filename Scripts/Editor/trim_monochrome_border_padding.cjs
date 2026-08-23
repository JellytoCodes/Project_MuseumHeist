const fs = require("node:fs");
const path = require("node:path");
const sharp = require("C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp");

const projectRoot = path.resolve(__dirname, "../..");
const sourceDir = path.join(projectRoot, "SourceArt/UI/Concepts/Monochrome");
const assets = [
  { filename: "T_UIBorder_Monochrome_1x1_2048.png", width: 2048, height: 2048 },
  { filename: "T_UIBorder_Monochrome_2x1_2048x1024.png", width: 2048, height: 1024 },
  { filename: "T_UIBorder_Monochrome_1x2_1024x2048.png", width: 1024, height: 2048 },
];

async function getAlphaBounds(filePath) {
  const { data, info } = await sharp(filePath)
    .ensureAlpha()
    .raw()
    .toBuffer({ resolveWithObject: true });

  let minX = info.width;
  let minY = info.height;
  let maxX = -1;
  let maxY = -1;

  for (let y = 0; y < info.height; y += 1) {
    for (let x = 0; x < info.width; x += 1) {
      if (data[(y * info.width + x) * 4 + 3] === 0) continue;
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }

  if (maxX < minX || maxY < minY) {
    throw new Error(`No non-transparent pixels: ${filePath}`);
  }

  return {
    imageWidth: info.width,
    imageHeight: info.height,
    left: minX,
    top: minY,
    width: maxX - minX + 1,
    height: maxY - minY + 1,
    rightPadding: info.width - 1 - maxX,
    bottomPadding: info.height - 1 - maxY,
  };
}

async function processAsset(asset) {
  const filePath = path.join(sourceDir, asset.filename);
  const temporaryPath = `${filePath}.tight.png`;
  const before = await getAlphaBounds(filePath);

  await sharp(filePath)
    .extract({
      left: before.left,
      top: before.top,
      width: before.width,
      height: before.height,
    })
    .resize({
      width: asset.width,
      height: asset.height,
      fit: "fill",
      kernel: sharp.kernel.lanczos3,
    })
    .png()
    .toFile(temporaryPath);

  fs.copyFileSync(temporaryPath, filePath);
  fs.unlinkSync(temporaryPath);

  const after = await getAlphaBounds(filePath);
  const padding = {
    left: after.left,
    top: after.top,
    right: after.rightPadding,
    bottom: after.bottomPadding,
  };
  if (
    after.imageWidth !== asset.width ||
    after.imageHeight !== asset.height ||
    Object.values(padding).some((value) => value !== 0)
  ) {
    throw new Error(`Tight crop verification failed for ${asset.filename}: ${JSON.stringify({ after, padding })}`);
  }

  console.log(JSON.stringify({
    filename: asset.filename,
    beforePadding: {
      left: before.left,
      top: before.top,
      right: before.rightPadding,
      bottom: before.bottomPadding,
    },
    afterPadding: padding,
    outputSize: `${after.imageWidth}x${after.imageHeight}`,
  }));
}

(async () => {
  for (const asset of assets) await processAsset(asset);
})().catch((error) => {
  console.error(error);
  process.exit(1);
});
