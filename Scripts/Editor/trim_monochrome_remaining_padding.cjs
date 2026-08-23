const fs = require("node:fs");
const path = require("node:path");
const sharp = require("C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp");

const projectRoot = path.resolve(__dirname, "../..");
const sourceDir = path.join(projectRoot, "SourceArt/UI/Concepts/Monochrome");
const assets = [
  "T_UIButton_Monochrome_Normal_2048x512.png",
  "T_UIButton_Monochrome_Hovered_2048x512.png",
  "T_UIButton_Monochrome_Pressed_2048x512.png",
  "T_UIIcon_Copy_Monochrome_2048.png",
  "T_UIIcon_ReadyCheck_Monochrome_2048.png",
  "T_UILogo_MuseumHeist_Monochrome_2048.png",
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
    right: info.width - 1 - maxX,
    bottom: info.height - 1 - maxY,
  };
}

async function analyze() {
  for (const filename of assets) {
    const bounds = await getAlphaBounds(path.join(sourceDir, filename));
    console.log(JSON.stringify({ filename, bounds }));
  }
}

async function trim() {
  for (const filename of assets) {
    const filePath = path.join(sourceDir, filename);
    const temporaryPath = `${filePath}.tight.png`;
    const before = await getAlphaBounds(filePath);

    await sharp(filePath)
      .extract({ left: before.left, top: before.top, width: before.width, height: before.height })
      .png()
      .toFile(temporaryPath);

    fs.copyFileSync(temporaryPath, filePath);
    fs.unlinkSync(temporaryPath);

    const after = await getAlphaBounds(filePath);
    if (after.left !== 0 || after.top !== 0 || after.right !== 0 || after.bottom !== 0) {
      throw new Error(`Tight crop verification failed for ${filename}: ${JSON.stringify(after)}`);
    }

    console.log(JSON.stringify({
      filename,
      beforePadding: { left: before.left, top: before.top, right: before.right, bottom: before.bottom },
      outputSize: `${after.imageWidth}x${after.imageHeight}`,
      afterPadding: { left: after.left, top: after.top, right: after.right, bottom: after.bottom },
    }));
  }
}

(process.argv.includes("--trim") ? trim() : analyze()).catch((error) => {
  console.error(error);
  process.exit(1);
});
