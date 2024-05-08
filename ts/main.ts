// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { compile } from "./compiler";
import logger from "./logger";
import { CompileError } from "./report";

setUpBuiltin(false);

const filePath = process.argv[2];

try {
	const context = compile(filePath, null);
	console.log(context.topCodeGenText.join(""));
	// logger.printFileAccessLogs();
	// logger.printTimes();
} catch (error) {
	if (error instanceof CompileError) {
		console.log("uncaught compiler error");
		console.log(error.getText(true));
		process.exitCode = 1;
	} else {
		throw error;	
	}
}