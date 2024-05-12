// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { CompilerOptions, compile } from "./compiler";
import { CompileError } from "./report";
import logger from "./logger";
import utilities from "./utilities";

let i = 2;

function next(): string {
	return process.argv[i++];
}

const options: CompilerOptions = {
	filePath: next(),
	check: true,
};

while (i < process.argv.length) {
	const arg = next();
	// console.log(`arg: ${arg}`);
	if (arg == "-o") {
		options.outputPath = next();
	} else if (arg == "-ide") {
		const mode = next();
		if (mode == "compileFile") {
			options.ideOptions = {
				mode: mode,
			};
		} else {
			utilities.TODO();
		}
	} else if (arg == "-noCheck") {
		options.check = false;
	} else {
		utilities.TODO();
	}
}

logger.global("options:", options);

try {
	setUpBuiltin(false);
	const context = compile(options, null);
	if (options.outputPath) {
		utilities.writeFile(options.outputPath, context.topCodeGenText.join(""));
	} else {
		if (!options.ideOptions) {
			console.log(context.topCodeGenText.join(""));
		}
	}
} catch (error) {
	if (error instanceof CompileError) {
		process.exitCode = 1;
		if (options.ideOptions && options.ideOptions.mode == "compileFile") {
			console.log(JSON.stringify(error));
		} else {
			console.log("uncaught compiler error");
			console.log(error.getText(true));
		}
	} else {
		throw error;	
	}
}

// logger.printFileAccessLogs();
// logger.printTimes();