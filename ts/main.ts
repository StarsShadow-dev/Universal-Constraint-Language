// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { compileFile } from "./compiler";

setUpBuiltin(false);

const filePath = process.argv[2];

compileFile(filePath);