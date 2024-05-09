// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { compile } from "./compiler";
import { CompileError } from "./report";
import logger from "./logger";
import utilities from "./utilities";

type CompilerOptions = {
	outputPath?: string,
}

const options: CompilerOptions = {};

let i = 2;

function next(): string {
	return process.argv[i++];
}

const filePath = next();

while (i < process.argv.length) {
	const arg = next();
	console.log(`arg: ${arg}`);
	if (arg == "-o") {
		options.outputPath = next();
	}
}

try {
	setUpBuiltin(false);
	const context = compile(filePath, null);
	if (options.outputPath) {
		utilities.writeFile(options.outputPath, context.topCodeGenText.join(""));
	} else {
		console.log(context.topCodeGenText.join(""));
	}
} catch (error) {
	if (error instanceof CompileError) {
		console.log("uncaught compiler error");
		console.log(error.getText(true));
		process.exitCode = 1;
	} else {
		throw error;	
	}
}

logger.printFileAccessLogs();
logger.printTimes();