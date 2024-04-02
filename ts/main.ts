// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { compileFile } from "./compiler";

setUpBuiltin();

const filePath = process.argv[2];

try {
	compileFile(filePath);
} catch (error) {
	if (error == "__do nothing__") {
		// do nothing
	} else {
		throw error;
	}
}